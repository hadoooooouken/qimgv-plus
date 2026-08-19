/*
    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#pragma once

#include "zipreader_p.h"

#include <QBuffer>
#include <QByteArray>
#include <QIODevice>
#include <QImage>
#include <QImageReader>

#include <limits>
#include <utility>

namespace KraOraInternal
{

constexpr quint64 kBytesPerMebibyte = 1024ULL * 1024ULL;

// A merged preview above 256 MiB is not needed for normal KRA/ORA viewing and is unsafe to inflate eagerly.
constexpr quint64 kMaximumMergedImageBytes = 256ULL * kBytesPerMebibyte;
// Deflate overhead can make incompressible input slightly larger than its uncompressed representation.
constexpr quint64 kMaximumCompressedMergedImageBytes = kMaximumMergedImageBytes + kBytesPerMebibyte;

// Match the codec layer's established dimension ceiling; the separate pixel cap bounds total memory.
constexpr qint64 kMaximumImageDimension = 300'000;
// These limits bound both PNG header abuse and the worst-case 64-bit RGBA QImage allocation.
constexpr quint64 kMaximumDecodedPixels = 64ULL * 1024ULL * 1024ULL;
constexpr quint64 kMaximumDecodedBytesPerPixel = 8ULL;
constexpr quint64 kMaximumDecodedImageBytes = 512ULL * kBytesPerMebibyte;

constexpr char kMergedImageFileName[] = "mergedimage.png";
constexpr char kPngFormat[] = "PNG";

constexpr bool checkedMultiply(quint64 lhs, quint64 rhs, quint64 &result)
{
    if (rhs != 0 && lhs > std::numeric_limits<quint64>::max() / rhs) {
        return false;
    }

    result = lhs * rhs;
    return true;
}

constexpr bool isMergedImageSizeAllowed(quint64 size)
{
    return size > 0
        && size <= kMaximumMergedImageBytes
        && size <= static_cast<quint64>(std::numeric_limits<size_t>::max())
        && size <= static_cast<quint64>(std::numeric_limits<qsizetype>::max());
}

constexpr bool isDecodedImageSizeAllowed(qint64 width, qint64 height)
{
    if (width <= 0 || height <= 0 || width > kMaximumImageDimension || height > kMaximumImageDimension) {
        return false;
    }

    quint64 pixelCount = 0;
    if (!checkedMultiply(static_cast<quint64>(width), static_cast<quint64>(height), pixelCount)
        || pixelCount > kMaximumDecodedPixels) {
        return false;
    }

    quint64 decodedBytes = 0;
    return checkedMultiply(pixelCount, kMaximumDecodedBytesPerPixel, decodedBytes)
        && decodedBytes <= kMaximumDecodedImageBytes;
}

static_assert(isMergedImageSizeAllowed(kMaximumMergedImageBytes));
static_assert(!isMergedImageSizeAllowed(0));
static_assert(!isMergedImageSizeAllowed(kMaximumMergedImageBytes + 1ULL));
static_assert(!isMergedImageSizeAllowed(std::numeric_limits<quint64>::max()));
static_assert(isDecodedImageSizeAllowed(1, 1));
static_assert(!isDecodedImageSizeAllowed(kMaximumImageDimension + 1, 1));
static_assert(!isDecodedImageSizeAllowed(kMaximumImageDimension, kMaximumImageDimension));

inline bool decodePng(QByteArray &pngData, QImage &image)
{
    QBuffer buffer(&pngData);
    if (!buffer.open(QIODevice::ReadOnly)) {
        return false;
    }

    QImageReader reader(&buffer, kPngFormat);
    reader.setAutoDetectImageFormat(false);
    reader.setDecideFormatFromContent(false);

    const QSize declaredSize = reader.size();
    if (!declaredSize.isValid() || !isDecodedImageSizeAllowed(declaredSize.width(), declaredSize.height())) {
        return false;
    }

    QImage decodedImage;
    if (!reader.read(&decodedImage)
        || !isDecodedImageSizeAllowed(decodedImage.width(), decodedImage.height())) {
        return false;
    }

    const qsizetype decodedSize = decodedImage.sizeInBytes();
    if (decodedSize <= 0 || static_cast<quint64>(decodedSize) > kMaximumDecodedImageBytes) {
        return false;
    }

    image = std::move(decodedImage);
    return true;
}

inline bool readMergedImage(QIODevice *device, QImage *image)
{
    if (!device || !image || !device->isOpen() || !device->isReadable() || device->isSequential()) {
        return false;
    }

    const qint64 deviceSize = device->size();
    if (deviceSize <= 0 || !device->seek(0)) {
        return false;
    }

    QimgvZipInternal::DeviceReadContext readContext{device, static_cast<quint64>(deviceSize)};
    QimgvZipInternal::ZipReader zipReader;
    if (!zipReader.initialize(readContext)) {
        return false;
    }

    quint32 fileIndex = 0;
    if (!zipReader.findEntry(kMergedImageFileName, fileIndex)) {
        return false;
    }

    QimgvZipInternal::ZipEntryStat fileStat;
    if (!zipReader.entryStat(fileIndex, fileStat)
        || fileStat.directory || fileStat.encrypted || !fileStat.supported
        || !isMergedImageSizeAllowed(fileStat.uncompressedSize)
        || fileStat.compressedSize == 0 || fileStat.compressedSize > kMaximumCompressedMergedImageBytes) {
        return false;
    }

    QByteArray mergedImageData;
    if (!zipReader.extractEntry(fileIndex, kMaximumMergedImageBytes, mergedImageData,
                                kMaximumCompressedMergedImageBytes)) {
        return false;
    }

    return decodePng(mergedImageData, *image);
}

} // namespace KraOraInternal
