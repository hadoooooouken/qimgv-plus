/*
    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#pragma once

extern "C" {
#include "miniz.h"
}

#include <QBuffer>
#include <QByteArray>
#include <QIODevice>
#include <QImage>
#include <QImageReader>

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <utility>

namespace KraOraInternal
{

constexpr quint64 kBytesPerMebibyte = 1024ULL * 1024ULL;

// A merged preview above 256 MiB is not needed for normal KRA/ORA viewing and is unsafe to inflate eagerly.
constexpr mz_uint64 kMaximumMergedImageBytes = 256ULL * kBytesPerMebibyte;
// Deflate overhead can make incompressible input slightly larger than its uncompressed representation.
constexpr mz_uint64 kMaximumCompressedMergedImageBytes = kMaximumMergedImageBytes + kBytesPerMebibyte;
// miniz reads the central directory eagerly, so cap each of its internal allocations independently.
constexpr size_t kMaximumMinizAllocationBytes = 64ULL * static_cast<size_t>(kBytesPerMebibyte);
// Large layer counts are not useful for locating one flattened preview and can consume excessive CPU.
constexpr mz_uint kMaximumArchiveEntryCount = 100'000U;
// Keep individual QIODevice reads small even if malformed metadata asks miniz for a large range.
constexpr size_t kMaximumDeviceReadChunkBytes = 64ULL * 1024ULL;

// Match the codec layer's established dimension ceiling; the separate pixel cap bounds total memory.
constexpr qint64 kMaximumImageDimension = 300'000;
// These limits bound both PNG header abuse and the worst-case 64-bit RGBA QImage allocation.
constexpr quint64 kMaximumDecodedPixels = 64ULL * 1024ULL * 1024ULL;
constexpr quint64 kMaximumDecodedBytesPerPixel = 8ULL;
constexpr quint64 kMaximumDecodedImageBytes = 512ULL * kBytesPerMebibyte;

constexpr char kMergedImageFileName[] = "mergedimage.png";
constexpr char kPngFormat[] = "PNG";
constexpr mz_uint kNoZipFlags = 0;

constexpr bool checkedMultiply(quint64 lhs, quint64 rhs, quint64 &result)
{
    if (rhs != 0 && lhs > std::numeric_limits<quint64>::max() / rhs) {
        return false;
    }

    result = lhs * rhs;
    return true;
}

constexpr bool isMergedImageSizeAllowed(mz_uint64 size)
{
    return size > 0
        && size <= kMaximumMergedImageBytes
        && size <= static_cast<mz_uint64>(std::numeric_limits<size_t>::max())
        && size <= static_cast<mz_uint64>(std::numeric_limits<qsizetype>::max());
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
static_assert(!isMergedImageSizeAllowed(std::numeric_limits<mz_uint64>::max()));
static_assert(isDecodedImageSizeAllowed(1, 1));
static_assert(!isDecodedImageSizeAllowed(kMaximumImageDimension + 1, 1));
static_assert(!isDecodedImageSizeAllowed(kMaximumImageDimension, kMaximumImageDimension));

struct DeviceReadContext
{
    QIODevice *device = nullptr;
    mz_uint64 archiveSize = 0;
    bool failed = false;
};

inline size_t readDevice(void *opaque, mz_uint64 fileOffset, void *buffer, size_t byteCount)
{
    if (!opaque) {
        return 0;
    }

    auto &context = *static_cast<DeviceReadContext *>(opaque);
    if (context.failed || !context.device || (byteCount > 0 && !buffer)
        || fileOffset > context.archiveSize || byteCount > context.archiveSize - fileOffset
        || fileOffset > static_cast<mz_uint64>(std::numeric_limits<qint64>::max())
        || byteCount > static_cast<size_t>(std::numeric_limits<qint64>::max())) {
        context.failed = true;
        return 0;
    }

    const auto deviceOffset = static_cast<qint64>(fileOffset);
    if (!context.device->seek(deviceOffset) || context.device->pos() != deviceOffset) {
        context.failed = true;
        return 0;
    }

    size_t totalBytesRead = 0;
    auto *destination = static_cast<char *>(buffer);
    while (totalBytesRead < byteCount) {
        const size_t chunkSize = std::min(byteCount - totalBytesRead, kMaximumDeviceReadChunkBytes);
        const qint64 bytesRead = context.device->read(destination + totalBytesRead, static_cast<qint64>(chunkSize));
        if (bytesRead <= 0 || static_cast<size_t>(bytesRead) > chunkSize) {
            context.failed = true;
            return 0;
        }
        totalBytesRead += static_cast<size_t>(bytesRead);
    }

    return totalBytesRead;
}

inline bool checkedAllocationSize(size_t itemCount, size_t itemSize, size_t &allocationSize)
{
    if (itemCount == 0 || itemSize == 0 || itemCount > std::numeric_limits<size_t>::max() / itemSize) {
        return false;
    }

    allocationSize = itemCount * itemSize;
    return allocationSize <= kMaximumMinizAllocationBytes;
}

inline void *allocateMinizMemory(void *, size_t itemCount, size_t itemSize)
{
    size_t allocationSize = 0;
    return checkedAllocationSize(itemCount, itemSize, allocationSize) ? std::malloc(allocationSize) : nullptr;
}

inline void *reallocateMinizMemory(void *, void *address, size_t itemCount, size_t itemSize)
{
    size_t allocationSize = 0;
    if (!checkedAllocationSize(itemCount, itemSize, allocationSize)) {
        return nullptr;
    }
    return std::realloc(address, allocationSize);
}

inline void freeMinizMemory(void *, void *address)
{
    std::free(address);
}

class ZipReader final
{
public:
    ZipReader()
    {
        mz_zip_zero_struct(&m_archive);
    }

    ~ZipReader()
    {
        if (m_initialized) {
            mz_zip_reader_end(&m_archive);
        }
    }

    ZipReader(const ZipReader &) = delete;
    ZipReader &operator=(const ZipReader &) = delete;

    bool initialize(DeviceReadContext &context)
    {
        m_archive.m_pRead = readDevice;
        m_archive.m_pIO_opaque = &context;
        m_archive.m_pAlloc = allocateMinizMemory;
        m_archive.m_pRealloc = reallocateMinizMemory;
        m_archive.m_pFree = freeMinizMemory;

        constexpr auto readerFlags = static_cast<mz_uint>(MZ_ZIP_FLAG_DO_NOT_SORT_CENTRAL_DIRECTORY);
        if (!mz_zip_reader_init(&m_archive, context.archiveSize, readerFlags)) {
            return false;
        }

        m_initialized = true;
        return !context.failed;
    }

    mz_zip_archive *archive()
    {
        return &m_archive;
    }

private:
    mz_zip_archive m_archive{};
    bool m_initialized = false;
};

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

    DeviceReadContext readContext{device, static_cast<mz_uint64>(deviceSize)};
    ZipReader zipReader;
    if (!zipReader.initialize(readContext)
        || mz_zip_reader_get_num_files(zipReader.archive()) > kMaximumArchiveEntryCount) {
        return false;
    }

    mz_uint32 fileIndex = 0;
    if (!mz_zip_reader_locate_file_v2(zipReader.archive(), kMergedImageFileName, nullptr, kNoZipFlags, &fileIndex)
        || readContext.failed) {
        return false;
    }

    mz_zip_archive_file_stat fileStat{};
    const auto minizFileIndex = static_cast<mz_uint>(fileIndex);
    if (!mz_zip_reader_file_stat(zipReader.archive(), minizFileIndex, &fileStat)
        || readContext.failed || fileStat.m_is_directory || fileStat.m_is_encrypted || !fileStat.m_is_supported
        || !isMergedImageSizeAllowed(fileStat.m_uncomp_size)
        || fileStat.m_comp_size == 0 || fileStat.m_comp_size > kMaximumCompressedMergedImageBytes
        || fileStat.m_local_header_ofs > readContext.archiveSize
        || fileStat.m_comp_size > readContext.archiveSize - fileStat.m_local_header_ofs) {
        return false;
    }

    const auto mergedImageSize = static_cast<qsizetype>(fileStat.m_uncomp_size);
    QByteArray mergedImageData;
    mergedImageData.resize(mergedImageSize);
    if (mergedImageData.size() != mergedImageSize
        || !mz_zip_reader_extract_to_mem(zipReader.archive(), minizFileIndex, mergedImageData.data(),
                                        static_cast<size_t>(mergedImageSize), kNoZipFlags)
        || readContext.failed) {
        return false;
    }

    return decodePng(mergedImageData, *image);
}

} // namespace KraOraInternal
