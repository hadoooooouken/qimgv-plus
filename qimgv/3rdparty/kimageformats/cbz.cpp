/*
    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "cbz.h"
#include "zipreader_p.h"

#include <QBuffer>
#include <QCollator>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QSet>
#include <QStringList>
#include <QVector>

#include <algorithm>
#include <limits>
#include <utility>

namespace
{

constexpr quint64 kBytesPerMebibyte = 1024ULL * 1024ULL;
constexpr quint64 kMaximumPageSourceBytes = 256ULL * kBytesPerMebibyte;
constexpr qint64 kMaximumImageDimension = 300'000;
constexpr quint64 kMaximumSourcePixels = 128ULL * 1024ULL * 1024ULL;
constexpr quint64 kMaximumDecodedImageBytes = 512ULL * kBytesPerMebibyte;

struct CbzPageEntry
{
    quint32 index = 0;
    QString path;
    QByteArray format;
    quint64 compressedSize = 0;
    quint64 uncompressedSize = 0;
};

bool checkedMultiply(quint64 lhs, quint64 rhs, quint64 &result)
{
    if (rhs != 0 && lhs > std::numeric_limits<quint64>::max() / rhs) {
        return false;
    }
    result = lhs * rhs;
    return true;
}

bool isSourceImageSizeAllowed(const QSize &size)
{
    if (!size.isValid() || size.width() <= 0 || size.height() <= 0
        || size.width() > kMaximumImageDimension || size.height() > kMaximumImageDimension) {
        return false;
    }

    quint64 pixelCount = 0;
    return checkedMultiply(static_cast<quint64>(size.width()), static_cast<quint64>(size.height()), pixelCount)
        && pixelCount <= kMaximumSourcePixels;
}

bool isDecodedImageAllowed(const QImage &image)
{
    if (image.isNull() || image.width() <= 0 || image.height() <= 0
        || image.width() > kMaximumImageDimension || image.height() > kMaximumImageDimension) {
        return false;
    }

    const qsizetype bytes = image.sizeInBytes();
    return bytes > 0 && static_cast<quint64>(bytes) <= kMaximumDecodedImageBytes;
}

QByteArray pageFormat(const QString &path)
{
    const QByteArray suffix = QFileInfo(path).suffix().toLower().toLatin1();
    static const QSet<QByteArray> supportedFormats = {
        "jpg", "jpeg", "png", "webp", "avif", "jxl", "heic", "heif",
        "bmp", "gif", "tif", "tiff", "qoi"
    };

    if (!supportedFormats.contains(suffix)) {
        return {};
    }
    if (suffix == "tif") {
        return "tiff";
    }
    return suffix;
}

bool isHiddenOrJunkPath(QString path)
{
    path.replace(u'\\', u'/');
    if (path.startsWith(u'/')) {
        return true;
    }

    const QStringList components = path.split(u'/', Qt::SkipEmptyParts);
    if (components.isEmpty()) {
        return true;
    }

    for (const QString &component : components) {
        if (component == u".." || component.startsWith(u'.')
            || component.compare(u"__MACOSX", Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

bool naturalPathLessThan(const CbzPageEntry &left, const CbzPageEntry &right)
{
    static thread_local QCollator collator = [] {
        QCollator value;
        value.setNumericMode(true);
        value.setCaseSensitivity(Qt::CaseInsensitive);
        return value;
    }();

    const int comparison = collator.compare(left.path, right.path);
    if (comparison != 0) {
        return comparison < 0;
    }
    return QString::compare(left.path, right.path, Qt::CaseSensitive) < 0;
}

} // namespace

class CbzHandlerPrivate
{
public:
    bool ensureIndex(QIODevice *device) const
    {
        if (indexAttempted) {
            return !pages.isEmpty();
        }
        if (!device || !device->isOpen() || !device->isReadable() || device->isSequential()) {
            return false;
        }

        const qint64 originalPosition = device->pos();
        const qint64 archiveSize = device->size();
        if (originalPosition < 0 || archiveSize <= 0 || !device->seek(0)) {
            return false;
        }
        indexAttempted = true;

        const auto restoreDevicePosition = [&]() {
            return device->seek(originalPosition);
        };

        readContext.device = device;
        readContext.archiveSize = static_cast<quint64>(archiveSize);
        readContext.failed = false;
        if (!zipReader.initialize(readContext)) {
            restoreDevicePosition();
            return false;
        }

        const quint32 count = zipReader.entryCount();
        pages.reserve(static_cast<qsizetype>(count));
        for (quint32 index = 0; index < count; ++index) {
            QimgvZipInternal::ZipEntryStat stat;
            if (!zipReader.entryStat(index, stat)) {
                pages.clear();
                restoreDevicePosition();
                return false;
            }

            QString normalizedPath = stat.name;
            normalizedPath.replace(u'\\', u'/');
            const QByteArray format = pageFormat(normalizedPath);
            if (stat.directory || stat.encrypted || !stat.supported || format.isEmpty()
                || isHiddenOrJunkPath(normalizedPath) || stat.compressedSize == 0
                || stat.uncompressedSize == 0 || stat.compressedSize > kMaximumPageSourceBytes
                || stat.uncompressedSize > kMaximumPageSourceBytes) {
                continue;
            }

            pages.push_back({stat.index, std::move(normalizedPath), format,
                             stat.compressedSize, stat.uncompressedSize});
        }

        std::sort(pages.begin(), pages.end(), naturalPathLessThan);
        if (currentPage >= pages.size()) {
            currentPage = 0;
        }
        if (!restoreDevicePosition()) {
            pages.clear();
            return false;
        }
        return !pages.isEmpty();
    }

    bool loadCurrentPage(QIODevice *device) const
    {
        if (!ensureIndex(device) || currentPage < 0 || currentPage >= pages.size()) {
            return false;
        }
        if (cachedPage == currentPage && !pageData.isEmpty()) {
            return true;
        }

        const CbzPageEntry &page = pages.at(currentPage);
        QByteArray extracted;
        if (!zipReader.extractEntry(page.index, page.uncompressedSize, extracted,
                                    page.compressedSize)) {
            clearPageCache();
            return false;
        }

        pageData = std::move(extracted);
        cachedPage = currentPage;
        return true;
    }

    QSize currentPageSize(QIODevice *device) const
    {
        if (!loadCurrentPage(device)) {
            return {};
        }

        QBuffer buffer(&pageData);
        if (!buffer.open(QIODevice::ReadOnly)) {
            return {};
        }

        QImageReader reader(&buffer, pages.at(currentPage).format);
        reader.setAutoDetectImageFormat(false);
        reader.setDecideFormatFromContent(false);
        reader.setAutoTransform(true);
        const QSize size = reader.size();
        return isSourceImageSizeAllowed(size) ? size : QSize();
    }

    void clearPageCache() const
    {
        pageData.clear();
        pageData.squeeze();
        cachedPage = -1;
    }

    mutable bool indexAttempted = false;
    mutable QimgvZipInternal::DeviceReadContext readContext;
    mutable QimgvZipInternal::ZipReader zipReader;
    mutable QVector<CbzPageEntry> pages;
    mutable int currentPage = 0;
    mutable QByteArray pageData;
    mutable int cachedPage = -1;
    QSize scaledSize;
    QRect scaledClipRect;
};

CbzHandler::CbzHandler()
    : d(std::make_unique<CbzHandlerPrivate>())
{
}

CbzHandler::~CbzHandler() = default;

bool CbzHandler::canRead() const
{
    if (!d->ensureIndex(device())) {
        return false;
    }
    setFormat("cbz");
    return true;
}

bool CbzHandler::read(QImage *image)
{
    if (!image || !d->loadCurrentPage(device())) {
        return false;
    }

    QBuffer buffer(&d->pageData);
    if (!buffer.open(QIODevice::ReadOnly)) {
        return false;
    }

    QImageReader reader(&buffer, d->pages.at(d->currentPage).format);
    reader.setAutoDetectImageFormat(false);
    reader.setDecideFormatFromContent(false);
    reader.setAutoTransform(true);

    const QSize sourceSize = reader.size();
    if (!isSourceImageSizeAllowed(sourceSize)) {
        d->clearPageCache();
        return false;
    }

    if (d->scaledSize.isValid() && !d->scaledSize.isEmpty()) {
        reader.setScaledSize(d->scaledSize);
    }
    if (d->scaledClipRect.isValid() && !d->scaledClipRect.isEmpty()) {
        reader.setScaledClipRect(d->scaledClipRect);
    }

    QImage decoded;
    const bool success = reader.read(&decoded) && isDecodedImageAllowed(decoded);
    d->clearPageCache();
    if (!success) {
        return false;
    }

    *image = std::move(decoded);
    return true;
}

int CbzHandler::imageCount() const
{
    return d->ensureIndex(device()) ? d->pages.size() : 0;
}

int CbzHandler::currentImageNumber() const
{
    return d->ensureIndex(device()) ? d->currentPage : 0;
}

bool CbzHandler::jumpToImage(int imageNumber)
{
    if (!d->ensureIndex(device()) || imageNumber < 0 || imageNumber >= d->pages.size()) {
        return false;
    }

    if (d->currentPage != imageNumber) {
        d->currentPage = imageNumber;
        d->clearPageCache();
    }
    return true;
}

bool CbzHandler::jumpToNextImage()
{
    return jumpToImage(currentImageNumber() + 1);
}

bool CbzHandler::supportsOption(ImageOption option) const
{
    return option == Size || option == ScaledSize || option == ScaledClipRect;
}

QVariant CbzHandler::option(ImageOption option) const
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

void CbzHandler::setOption(ImageOption option, const QVariant &value)
{
    if (option == ScaledSize) {
        d->scaledSize = value.toSize();
    } else if (option == ScaledClipRect) {
        d->scaledClipRect = value.toRect();
    }
}

QImageIOPlugin::Capabilities CbzPlugin::capabilities(QIODevice *, const QByteArray &format) const
{
    if (format == "cbz" || format == "CBZ") {
        return Capabilities(CanRead);
    }

    // CBZ has the same PK ZIP signature as KRA, ORA, EPUB, DOCX and ordinary ZIP
    // archives. Never claim an untyped ZIP based on magic alone.
    return {};
}

QImageIOHandler *CbzPlugin::create(QIODevice *device, const QByteArray &format) const
{
    auto *handler = new CbzHandler;
    handler->setDevice(device);
    handler->setFormat(format.isEmpty() ? QByteArray("cbz") : format);
    return handler;
}

#include "moc_cbz.cpp"
