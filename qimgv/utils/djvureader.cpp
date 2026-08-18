#include "djvureader.h"

#include "djvu.h"

#include <QDebug>
#include <QFile>

#include <algorithm>
#include <limits>
#include <mutex>
#include <utility>

namespace {

std::once_flag g_djvuInitOnce;

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
    if (maxEdge <= 0)
        return 1;

    const int longestEdge = std::max(nativeInfo.width, nativeInfo.height);
    if (longestEdge <= maxEdge)
        return 1;

    return std::max(1, (longestEdge + maxEdge - 1) / maxEdge);
}

} // namespace

DjvuRenderResult DjvuReader::renderPage(const QString &path, int page,
                                        int maxEdge) {
    DjvuRenderResult result;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "DjvuReader: failed to open" << path << file.errorString();
        return result;
    }

    const qint64 fileSize = file.size();
    if (fileSize <= 0 ||
        static_cast<quint64>(fileSize) >
            static_cast<quint64>(std::numeric_limits<size_t>::max())) {
        qWarning() << "DjvuReader: invalid or unsupported file size" << path
                   << fileSize;
        return result;
    }

    QByteArray data = file.readAll();
    if (data.size() != fileSize) {
        qWarning() << "DjvuReader: failed to read complete document" << path;
        return result;
    }

    ensureDjvuInitialized();

    djvu_ctx *ctx =
        djvu_ctx_new(nullptr, nullptr, nullptr, nullptr, djvuDiagnostic, nullptr);
    if (!ctx) {
        qWarning() << "DjvuReader: failed to create decoder context" << path;
        return result;
    }

    djvu_doc *doc = djvu_doc_open(
        ctx, reinterpret_cast<const uint8_t *>(data.constData()),
        static_cast<size_t>(data.size()));
    if (!doc) {
        qWarning() << "DjvuReader: failed to open document" << path;
        djvu_ctx_free(ctx);
        return result;
    }

    result.pageCount = djvu_doc_page_count(doc);
    if (result.pageCount <= 0) {
        qWarning() << "DjvuReader: document has no pages" << path;
        djvu_doc_close(doc);
        djvu_ctx_free(ctx);
        result.pageCount = 0;
        return result;
    }

    if (page < 0 || page >= result.pageCount)
        page = 0;
    result.pageIndex = page;

    djvu_render_info nativeInfo{};
    if (djvu_page_render_info(doc, page, 1, &nativeInfo) != 0 ||
        nativeInfo.width <= 0 || nativeInfo.height <= 0) {
        qWarning() << "DjvuReader: failed to query page geometry" << path
                   << "page" << page;
        djvu_doc_close(doc);
        djvu_ctx_free(ctx);
        return result;
    }

    result.originalSize = QSize(nativeInfo.width, nativeInfo.height);
    const int subsample = subsampleForEdge(nativeInfo, maxEdge);

    djvu_render_info renderInfo{};
    if (djvu_page_render_info(doc, page, subsample, &renderInfo) != 0 ||
        renderInfo.width <= 0 || renderInfo.height <= 0) {
        qWarning() << "DjvuReader: failed to query render geometry" << path
                   << "page" << page << "subsample" << subsample;
        djvu_doc_close(doc);
        djvu_ctx_free(ctx);
        return result;
    }

    QImage::Format imageFormat = QImage::Format_Invalid;
    if (renderInfo.format == DJVU_FORMAT_RGB24)
        imageFormat = QImage::Format_RGB888;
    else if (renderInfo.format == DJVU_FORMAT_GRAY8)
        imageFormat = QImage::Format_Grayscale8;

    if (imageFormat == QImage::Format_Invalid) {
        qWarning() << "DjvuReader: unsupported decoder output format" << path;
        djvu_doc_close(doc);
        djvu_ctx_free(ctx);
        return result;
    }

    QImage rendered(renderInfo.width, renderInfo.height, imageFormat);
    if (rendered.isNull() ||
        djvu_page_render_into(doc, page, subsample, rendered.bits(),
                              rendered.bytesPerLine()) != 0) {
        qWarning() << "DjvuReader: failed to render page" << path << "page"
                   << page;
        rendered = QImage();
    }

    result.image = std::move(rendered);
    djvu_doc_close(doc);
    djvu_ctx_free(ctx);
    return result;
}
