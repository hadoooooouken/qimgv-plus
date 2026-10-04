/*
    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "rar_p.h"
#include "unrarreader_p.h"

#include <QBuffer>
#include <QFileDevice>
#include <QImage>
#include <QImageReader>
#include <QVector>

#include <algorithm>
#include <limits>

namespace {

// ---------------------------------------------------------------------------
// Safety limits — mirror the 7z plugin exactly.
// ---------------------------------------------------------------------------
constexpr quint64 kMaximumPageSourceBytes =
    256ULL * QimgvRarInternal::kRarBytesPerMebibyte;
constexpr qint64 kMaximumImageDimension = 300'000;
constexpr quint64 kMaximumSourcePixels = 128ULL * 1024ULL * 1024ULL;
constexpr quint64 kMaximumDecodedImageBytes =
    512ULL * QimgvRarInternal::kRarBytesPerMebibyte;

// ---------------------------------------------------------------------------
// RAR magic byte sequences.
// ---------------------------------------------------------------------------
constexpr unsigned char kRar4Sig[] = { 0x52, 0x61, 0x72, 0x21, 0x1A, 0x07, 0x00 };
constexpr unsigned char kRar5Sig[] = { 0x52, 0x61, 0x72, 0x21, 0x1A, 0x07, 0x01, 0x00 };
constexpr int kRar4SigLen = static_cast<int>(sizeof(kRar4Sig));
constexpr int kRar5SigLen = static_cast<int>(sizeof(kRar5Sig));

// ---------------------------------------------------------------------------
// Helpers (mirrors the 7z plugin style)
// ---------------------------------------------------------------------------

bool checkedMultiply(quint64 lhs, quint64 rhs, quint64 &result)
{
    if (rhs != 0 && lhs > std::numeric_limits<quint64>::max() / rhs)
        return false;
    result = lhs * rhs;
    return true;
}

bool isSourceImageSizeAllowed(const QSize &size)
{
    if (!size.isValid() || size.width() <= 0 || size.height() <= 0
        || size.width() > kMaximumImageDimension
        || size.height() > kMaximumImageDimension) {
        return false;
    }
    quint64 pixels = 0;
    return checkedMultiply(static_cast<quint64>(size.width()),
                           static_cast<quint64>(size.height()), pixels)
        && pixels <= kMaximumSourcePixels;
}

bool isDecodedImageAllowed(const QImage &image)
{
    if (image.isNull() || image.width() <= 0 || image.height() <= 0
        || image.width() > kMaximumImageDimension
        || image.height() > kMaximumImageDimension) {
        return false;
    }
    const qsizetype bytes = image.sizeInBytes();
    return bytes > 0
        && static_cast<quint64>(bytes) <= kMaximumDecodedImageBytes;
}

// Validate that the device's current content starts with a RAR signature.
// Uses QIODevice::peek() so the device position is not advanced.
bool deviceHasRarSignature(QIODevice *device)
{
    const QByteArray header = device->peek(kRar5SigLen);
    if (header.size() < kRar4SigLen)
        return false;

    const auto *hdr = reinterpret_cast<const unsigned char *>(header.constData());

    // RAR5 signature (8 bytes) — check first since it's a superset start.
    if (header.size() >= kRar5SigLen) {
        bool rar5 = true;
        for (int i = 0; i < kRar5SigLen; ++i)
            rar5 = rar5 && (hdr[i] == kRar5Sig[i]);
        if (rar5)
            return true;
    }

    // RAR4 signature (7 bytes).
    bool rar4 = true;
    for (int i = 0; i < kRar4SigLen; ++i)
        rar4 = rar4 && (hdr[i] == kRar4Sig[i]);
    return rar4;
}

} // anonymous namespace

// ===========================================================================
// RarHandlerPrivate
// ===========================================================================

class RarHandlerPrivate
{
public:
    // Build the image index on first call.
    // Requires a QFileDevice so that UnRAR can open the archive by path.
    bool ensureIndex(QIODevice *device) const;

    // Extract and cache the current page data.
    bool loadCurrentPage(QIODevice *device) const;

    // Decode only the image dimensions of the current page.
    QSize currentPageSize(QIODevice *device) const;

    void clearPageCache() const;

    mutable bool indexAttempted = false;
    mutable QimgvRarInternal::UnrarReader unrarReader;
    mutable QVector<QimgvRarInternal::RarPageEntry> pages;
    mutable int currentPage = 0;
    mutable QByteArray pageData;
    mutable int cachedPage = -1;
    QSize scaledSize;
    QRect scaledClipRect;
};

bool RarHandlerPrivate::ensureIndex(QIODevice *device) const
{
    if (indexAttempted)
        return !pages.isEmpty();

    if (!device || !device->isOpen() || !device->isReadable()
        || device->isSequential()) {
        return false;
    }

    // UnRAR requires a real filesystem path; any non-file device is rejected.
    auto *fileDevice = qobject_cast<QFileDevice *>(device);
    if (!fileDevice)
        return false;

    const QString filePath = fileDevice->fileName();
    if (filePath.isEmpty())
        return false;

    const qint64 rawSize = device->size();
    if (rawSize <= 0)
        return false;

    indexAttempted = true;

    if (!unrarReader.initialize(filePath, static_cast<quint64>(rawSize)))
        return false;

    const quint32 count = unrarReader.entryCount();
    pages.reserve(static_cast<qsizetype>(count));

    for (quint32 i = 0; i < count; ++i) {
        QimgvRarInternal::RarPageEntry entry;
        if (!unrarReader.entryInfo(i, entry)) {
            pages.clear();
            return false;
        }
        pages.push_back(std::move(entry));
    }

    // UnrarReader already sorts entries; no re-sort needed here.
    if (currentPage >= pages.size())
        currentPage = 0;

    return !pages.isEmpty();
}

bool RarHandlerPrivate::loadCurrentPage(QIODevice *device) const
{
    if (!ensureIndex(device) || currentPage < 0
        || currentPage >= pages.size()) {
        return false;
    }

    if (cachedPage == currentPage && !pageData.isEmpty())
        return true;

    const QimgvRarInternal::RarPageEntry &page = pages.at(currentPage);

    // Pass the declared uncompressed size as the extraction limit.
    // Entries exceeding kRarMaximumPageSourceBytes were filtered during indexing.
    QByteArray extracted;
    if (!unrarReader.extractEntry(page.archiveOrder, page.uncompressedSize, extracted)) {
        clearPageCache();
        return false;
    }

    pageData = std::move(extracted);
    cachedPage = currentPage;
    return true;
}

QSize RarHandlerPrivate::currentPageSize(QIODevice *device) const
{
    if (!loadCurrentPage(device))
        return {};

    QBuffer buffer(&pageData);
    if (!buffer.open(QIODevice::ReadOnly))
        return {};

    QImageReader reader(&buffer, pages.at(currentPage).format);
    reader.setAutoDetectImageFormat(false);
    reader.setDecideFormatFromContent(false);
    reader.setAutoTransform(true);

    const QSize size = reader.size();
    return isSourceImageSizeAllowed(size) ? size : QSize();
}

void RarHandlerPrivate::clearPageCache() const
{
    pageData.clear();
    pageData.squeeze();
    cachedPage = -1;
}

// ===========================================================================
// RarHandler
// ===========================================================================

RarHandler::RarHandler()
    : d(std::make_unique<RarHandlerPrivate>())
{
}

RarHandler::~RarHandler() = default;

bool RarHandler::canRead() const
{
    QIODevice *dev = device();
    if (!dev || !dev->isOpen() || !dev->isReadable() || dev->isSequential())
        return false;

    // Quick reject: validate RAR magic bytes without changing device position.
    if (!deviceHasRarSignature(dev))
        return false;

    // Full index build: requires a QFileDevice and at least one image entry.
    if (!d->ensureIndex(dev))
        return false;

    setFormat(QByteArrayLiteral("rar"));
    return true;
}

bool RarHandler::read(QImage *image)
{
    if (!image || !d->loadCurrentPage(device()))
        return false;

    QBuffer buffer(&d->pageData);
    if (!buffer.open(QIODevice::ReadOnly))
        return false;

    QImageReader reader(&buffer, d->pages.at(d->currentPage).format);
    reader.setAutoDetectImageFormat(false);
    reader.setDecideFormatFromContent(false);
    reader.setAutoTransform(true);

    const QSize sourceSize = reader.size();
    if (!isSourceImageSizeAllowed(sourceSize)) {
        d->clearPageCache();
        return false;
    }

    if (d->scaledSize.isValid() && !d->scaledSize.isEmpty())
        reader.setScaledSize(d->scaledSize);
    if (d->scaledClipRect.isValid() && !d->scaledClipRect.isEmpty())
        reader.setScaledClipRect(d->scaledClipRect);

    QImage decoded;
    const bool ok = reader.read(&decoded) && isDecodedImageAllowed(decoded);
    d->clearPageCache();

    if (!ok)
        return false;

    *image = std::move(decoded);
    return true;
}

int RarHandler::imageCount() const
{
    return d->ensureIndex(device()) ? d->pages.size() : 0;
}

int RarHandler::currentImageNumber() const
{
    return d->ensureIndex(device()) ? d->currentPage : 0;
}

bool RarHandler::jumpToImage(int imageNumber)
{
    if (!d->ensureIndex(device()) || imageNumber < 0
        || imageNumber >= d->pages.size()) {
        return false;
    }

    if (d->currentPage != imageNumber) {
        d->currentPage = imageNumber;
        d->clearPageCache();
    }
    return true;
}

bool RarHandler::jumpToNextImage()
{
    return jumpToImage(currentImageNumber() + 1);
}

bool RarHandler::supportsOption(ImageOption option) const
{
    return option == Size || option == ScaledSize || option == ScaledClipRect;
}

QVariant RarHandler::option(ImageOption option) const
{
    switch (option) {
    case Size: {
        const QSize size = d->currentPageSize(device());
        return size.isValid() ? QVariant::fromValue(size) : QVariant();
    }
    case ScaledSize:
        return QVariant::fromValue(d->scaledSize);
    case ScaledClipRect:
        return QVariant::fromValue(d->scaledClipRect);
    default:
        return {};
    }
}

void RarHandler::setOption(ImageOption option, const QVariant &value)
{
    if (option == ScaledSize)
        d->scaledSize = value.toSize();
    else if (option == ScaledClipRect)
        d->scaledClipRect = value.toRect();
}

// ===========================================================================
// RarPlugin
// ===========================================================================

QImageIOPlugin::Capabilities RarPlugin::capabilities(QIODevice * /*device*/,
                                                      const QByteArray &format) const
{
    if (format.compare("rar", Qt::CaseInsensitive) == 0)
        return Capabilities(CanRead);
    return {};
}

QImageIOHandler *RarPlugin::create(QIODevice *device, const QByteArray &format) const
{
    auto *handler = new RarHandler;
    handler->setDevice(device);
    handler->setFormat(format.isEmpty() ? QByteArrayLiteral("rar") : format);
    return handler;
}

#include "moc_rar.cpp"
