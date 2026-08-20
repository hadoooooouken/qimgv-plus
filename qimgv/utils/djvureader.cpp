#include "djvureader.h"

#include "djvu.h"

#include <QDebug>
#include <QFile>

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <utility>

namespace {

constexpr quint64 kBytesPerKiB = 1024;
constexpr quint64 kBytesPerMiB = kBytesPerKiB * kBytesPerKiB;
constexpr quint64 kQImageScanlineAlignmentBytes = 4;
constexpr quint64 kRgbChannelCount = 3;
constexpr quint64 kGrayscaleChannelCount = 1;
constexpr quint64 kMaximumDecoderInputBytes =
    std::numeric_limits<quint32>::max();

std::once_flag g_djvuInitOnce;

bool checkedAdd(quint64 left, quint64 right, quint64 &result) noexcept {
    if (left > std::numeric_limits<quint64>::max() - right)
        return false;

    result = left + right;
    return true;
}

bool checkedMultiply(quint64 left, quint64 right, quint64 &result) noexcept {
    if (left != 0 && right > std::numeric_limits<quint64>::max() / left)
        return false;

    result = left * right;
    return true;
}

class DecodeBudget {
public:
    explicit DecodeBudget(quint64 limitBytes) noexcept
        : mLimitBytes(limitBytes) {}

    bool reserve(quint64 bytes) noexcept {
        if (bytes > mLimitBytes - mUsedBytes) {
            mBudgetExceeded = true;
            return false;
        }

        mUsedBytes += bytes;
        return true;
    }

    void release(quint64 bytes) noexcept {
        if (bytes <= mUsedBytes)
            mUsedBytes -= bytes;
    }

    void markSystemAllocationFailure() noexcept {
        mSystemAllocationFailed = true;
    }

    quint64 limitBytes() const noexcept { return mLimitBytes; }
    quint64 usedBytes() const noexcept { return mUsedBytes; }
    bool budgetExceeded() const noexcept { return mBudgetExceeded; }
    bool systemAllocationFailed() const noexcept {
        return mSystemAllocationFailed;
    }

private:
    quint64 mLimitBytes = 0;
    quint64 mUsedBytes = 0;
    bool mBudgetExceeded = false;
    bool mSystemAllocationFailed = false;
};

struct alignas(std::max_align_t) AllocationHeader {
    quint64 chargedBytes = 0;
};

void *budgetAllocate(void *user, void *, size_t size) noexcept {
    auto *budget = static_cast<DecodeBudget *>(user);
    if (!budget || size > std::numeric_limits<size_t>::max() -
                              sizeof(AllocationHeader))
        return nullptr;

    const size_t allocationBytes = sizeof(AllocationHeader) + size;
    if (!budget->reserve(static_cast<quint64>(allocationBytes)))
        return nullptr;

    void *storage = std::malloc(allocationBytes);
    if (!storage) {
        budget->release(static_cast<quint64>(allocationBytes));
        budget->markSystemAllocationFailure();
        return nullptr;
    }

    auto *header = static_cast<AllocationHeader *>(storage);
    header->chargedBytes = static_cast<quint64>(allocationBytes);
    return header + 1;
}

void budgetFree(void *user, void *, void *ptr) noexcept {
    if (ptr) {
        auto *header = static_cast<AllocationHeader *>(ptr) - 1;
        if (auto *budget = static_cast<DecodeBudget *>(user))
            budget->release(header->chargedBytes);
        std::free(header);
    }
}

struct DjvuContextDeleter {
    void operator()(djvu_ctx *ctx) const noexcept { djvu_ctx_free(ctx); }
};

struct DjvuDocumentDeleter {
    void operator()(djvu_doc *doc) const noexcept { djvu_doc_close(doc); }
};

using DjvuContextPtr = std::unique_ptr<djvu_ctx, DjvuContextDeleter>;
using DjvuDocumentPtr = std::unique_ptr<djvu_doc, DjvuDocumentDeleter>;

void ensureDjvuInitialized() {
    std::call_once(g_djvuInitOnce, [] { djvu_init(); });
}

void djvuDiagnostic(void *, djvu_severity severity, const char *message) {
    if (!message || severity < DJVU_SEVERITY_WARNING)
        return;

    if (severity >= DJVU_SEVERITY_ERROR)
        qWarning().noquote() << "DjvuReader:" << QString::fromUtf8(message);
    else
        qDebug().noquote() << "DjvuReader:" << QString::fromUtf8(message);
}

int subsampleForEdge(const djvu_render_info &nativeInfo, int maxEdge) {
    const int longestEdge = std::max(nativeInfo.width, nativeInfo.height);
    if (longestEdge <= maxEdge)
        return 1;

    return std::max(1, (longestEdge + maxEdge - 1) / maxEdge);
}

bool calculateImageBytes(const djvu_render_info &renderInfo,
                         quint64 &imageBytes) noexcept {
    if (renderInfo.width <= 0 || renderInfo.height <= 0)
        return false;

    quint64 channelCount = 0;
    if (renderInfo.format == DJVU_FORMAT_RGB24)
        channelCount = kRgbChannelCount;
    else if (renderInfo.format == DJVU_FORMAT_GRAY8)
        channelCount = kGrayscaleChannelCount;
    else
        return false;

    quint64 rowBytes = 0;
    if (!checkedMultiply(static_cast<quint64>(renderInfo.width), channelCount,
                         rowBytes))
        return false;

    quint64 paddedRowBytes = 0;
    if (!checkedAdd(rowBytes, kQImageScanlineAlignmentBytes - 1,
                    paddedRowBytes))
        return false;
    paddedRowBytes =
        paddedRowBytes / kQImageScanlineAlignmentBytes *
        kQImageScanlineAlignmentBytes;

    return checkedMultiply(paddedRowBytes,
                           static_cast<quint64>(renderInfo.height), imageBytes);
}

void logDecoderAllocationFailure(const QString &path,
                                 const DecodeBudget &budget) {
    if (budget.budgetExceeded()) {
        qWarning() << "DjvuReader: decoder memory budget exceeded" << path
                   << "used bytes" << budget.usedBytes() << "limit bytes"
                   << budget.limitBytes();
    } else if (budget.systemAllocationFailed()) {
        qWarning() << "DjvuReader: system allocation failed while decoding"
                   << path;
    }
}

} // namespace

DjvuDecodeLimits DjvuDecodeLimits::fromMemoryLimitMiB(
    int memoryLimitMiB, int maximumEdge) noexcept {
    DjvuDecodeLimits limits;
    if (memoryLimitMiB <= 0 || maximumEdge <= 0)
        return limits;

    const quint64 limitMiB = static_cast<quint64>(memoryLimitMiB);
    if (limitMiB > std::numeric_limits<quint64>::max() / kBytesPerMiB)
        return limits;

    limits.memoryBudgetBytes = limitMiB * kBytesPerMiB;
    limits.maximumInputBytes =
        std::min(limits.memoryBudgetBytes, kMaximumDecoderInputBytes);
    limits.maximumEdge = maximumEdge;
    return limits;
}

bool DjvuDecodeLimits::isValid() const noexcept {
    return maximumInputBytes > 0 &&
           maximumInputBytes <= memoryBudgetBytes && maximumEdge > 0;
}

DjvuRenderResult DjvuReader::renderPage(const QString &path, int page,
                                        const DjvuDecodeLimits &limits) {
    DjvuRenderResult result;

    if (!limits.isValid()) {
        qWarning() << "DjvuReader: invalid decode limits" << path;
        return result;
    }

    DecodeBudget budget(limits.memoryBudgetBytes);

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "DjvuReader: failed to open" << path << file.errorString();
        return result;
    }

    const qint64 fileSize = file.size();
    if (fileSize <= 0 ||
        static_cast<quint64>(fileSize) > limits.maximumInputBytes ||
        static_cast<quint64>(fileSize) >
            static_cast<quint64>(std::numeric_limits<size_t>::max())) {
        qWarning() << "DjvuReader: invalid or oversized document" << path
                   << "size bytes" << fileSize << "input limit bytes"
                   << limits.maximumInputBytes;
        return result;
    }

    const quint64 inputBytes = static_cast<quint64>(fileSize);
    if (!budget.reserve(inputBytes)) {
        qWarning() << "DjvuReader: document exceeds decode memory budget" << path
                   << "size bytes" << fileSize << "limit bytes"
                   << budget.limitBytes();
        return result;
    }

    QByteArray data;
    try {
        data = file.read(fileSize);
    } catch (const std::bad_alloc &) {
        qWarning() << "DjvuReader: failed to allocate document buffer" << path;
        return result;
    }
    if (data.size() != fileSize) {
        qWarning() << "DjvuReader: failed to read complete document" << path;
        return result;
    }

    ensureDjvuInitialized();

    DjvuContextPtr ctx(djvu_ctx_new(budgetAllocate, budgetFree, nullptr,
                                   nullptr, djvuDiagnostic, &budget));
    if (!ctx) {
        qWarning() << "DjvuReader: failed to create decoder context" << path;
        logDecoderAllocationFailure(path, budget);
        return result;
    }

    DjvuDocumentPtr doc(djvu_doc_open(
        ctx.get(), reinterpret_cast<const uint8_t *>(data.constData()),
        static_cast<size_t>(data.size())));
    if (!doc) {
        qWarning() << "DjvuReader: failed to open document" << path;
        logDecoderAllocationFailure(path, budget);
        return result;
    }

    result.pageCount = djvu_doc_page_count(doc.get());
    if (result.pageCount <= 0) {
        qWarning() << "DjvuReader: document has no pages" << path;
        result.pageCount = 0;
        return result;
    }

    if (page < 0 || page >= result.pageCount)
        page = 0;
    result.pageIndex = page;

    djvu_render_info nativeInfo{};
    if (djvu_page_render_info(doc.get(), page, 1, &nativeInfo) != 0 ||
        nativeInfo.width <= 0 || nativeInfo.height <= 0) {
        qWarning() << "DjvuReader: failed to query page geometry" << path
                   << "page" << page;
        return result;
    }

    result.originalSize = QSize(nativeInfo.width, nativeInfo.height);
    const int subsample = subsampleForEdge(nativeInfo, limits.maximumEdge);

    djvu_render_info renderInfo{};
    if (djvu_page_render_info(doc.get(), page, subsample, &renderInfo) != 0 ||
        renderInfo.width <= 0 || renderInfo.height <= 0) {
        qWarning() << "DjvuReader: failed to query render geometry" << path
                   << "page" << page << "subsample" << subsample;
        return result;
    }

    QImage::Format imageFormat = QImage::Format_Invalid;
    if (renderInfo.format == DJVU_FORMAT_RGB24)
        imageFormat = QImage::Format_RGB888;
    else if (renderInfo.format == DJVU_FORMAT_GRAY8)
        imageFormat = QImage::Format_Grayscale8;

    if (imageFormat == QImage::Format_Invalid) {
        qWarning() << "DjvuReader: unsupported decoder output format" << path;
        return result;
    }

    quint64 outputBytes = 0;
    if (!calculateImageBytes(renderInfo, outputBytes) ||
        !budget.reserve(outputBytes)) {
        qWarning() << "DjvuReader: rendered page exceeds decode memory budget"
                   << path << "page" << page << "size" << renderInfo.width
                   << "x" << renderInfo.height << "required output bytes"
                   << outputBytes << "remaining bytes"
                   << budget.limitBytes() - budget.usedBytes();
        return result;
    }

    QImage rendered(renderInfo.width, renderInfo.height, imageFormat);
    if (rendered.isNull()) {
        qWarning() << "DjvuReader: failed to allocate rendered page" << path
                   << "page" << page;
        return result;
    }

    if (static_cast<quint64>(rendered.sizeInBytes()) != outputBytes) {
        qWarning() << "DjvuReader: unexpected rendered image allocation size"
                   << path << "page" << page << "expected bytes" << outputBytes
                   << "actual bytes" << rendered.sizeInBytes();
        return result;
    }

    if (djvu_page_render_into(doc.get(), page, subsample, rendered.bits(),
                              rendered.bytesPerLine()) != 0) {
        qWarning() << "DjvuReader: failed to render page" << path << "page"
                   << page;
        logDecoderAllocationFailure(path, budget);
        rendered = QImage();
    }

    result.image = std::move(rendered);
    return result;
}
