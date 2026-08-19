/*
    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#pragma once

extern "C" {
#include "miniz.h"
}

#include <QByteArray>
#include <QIODevice>
#include <QString>
#include <algorithm>
#include <cstdlib>
#include <limits>
#include <utility>

namespace QimgvZipInternal
{

constexpr quint64 kBytesPerMebibyte = 1024ULL * 1024ULL;
// miniz reads the central directory eagerly, so cap each of its internal allocations independently.
constexpr size_t kMaximumMinizAllocationBytes = 64ULL * static_cast<size_t>(kBytesPerMebibyte);
// Keep pathological archives from turning central-directory processing into excessive CPU/memory work.
constexpr quint32 kMaximumArchiveEntryCount = 100'000U;
// ZIP stores filename lengths in 16 bits; bound temporary filename buffers explicitly.
constexpr quint32 kMaximumArchiveFileNameBytes = 65'535U;
// Keep individual QIODevice reads small even if malformed metadata asks miniz for a large range.
constexpr size_t kMaximumDeviceReadChunkBytes = 64ULL * 1024ULL;
constexpr mz_uint kNoZipFlags = 0;

struct DeviceReadContext
{
    QIODevice *device = nullptr;
    quint64 archiveSize = 0;
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

struct ZipEntryStat
{
    quint32 index = 0;
    QString name;
    quint64 compressedSize = 0;
    quint64 uncompressedSize = 0;
    quint64 localHeaderOffset = 0;
    bool directory = false;
    bool encrypted = false;
    bool supported = false;
};

class ZipReader final
{
public:
    ZipReader()
    {
        mz_zip_zero_struct(&m_archive);
    }

    ~ZipReader()
    {
        close();
    }

    ZipReader(const ZipReader &) = delete;
    ZipReader &operator=(const ZipReader &) = delete;

    bool initialize(DeviceReadContext &context)
    {
        if (m_initialized || !context.device || !context.device->isOpen() || !context.device->isReadable()
            || context.device->isSequential() || context.archiveSize == 0
            || context.archiveSize > static_cast<quint64>(std::numeric_limits<qint64>::max()) || context.failed) {
            return false;
        }

        mz_zip_zero_struct(&m_archive);
        m_context = &context;
        m_archive.m_pRead = readDevice;
        m_archive.m_pIO_opaque = &context;
        m_archive.m_pAlloc = allocateMinizMemory;
        m_archive.m_pRealloc = reallocateMinizMemory;
        m_archive.m_pFree = freeMinizMemory;

        constexpr auto readerFlags = static_cast<mz_uint>(MZ_ZIP_FLAG_DO_NOT_SORT_CENTRAL_DIRECTORY);
        if (!mz_zip_reader_init(&m_archive, context.archiveSize, readerFlags)) {
            m_context = nullptr;
            return false;
        }
        m_initialized = true;

        if (context.failed || mz_zip_reader_get_num_files(&m_archive) > kMaximumArchiveEntryCount) {
            close();
            return false;
        }

        return true;
    }

    quint32 entryCount()
    {
        if (!isUsable()) {
            return 0;
        }
        return static_cast<quint32>(mz_zip_reader_get_num_files(&m_archive));
    }

    bool entryStat(quint32 index, ZipEntryStat &result)
    {
        mz_zip_archive_file_stat fileStat{};
        if (!rawEntryStat(index, fileStat)) {
            return false;
        }

        QString entryName;
        if (!readEntryName(index, entryName)) {
            return false;
        }

        result.index = index;
        result.name = std::move(entryName);
        result.compressedSize = static_cast<quint64>(fileStat.m_comp_size);
        result.uncompressedSize = static_cast<quint64>(fileStat.m_uncomp_size);
        result.localHeaderOffset = static_cast<quint64>(fileStat.m_local_header_ofs);
        result.directory = fileStat.m_is_directory != 0;
        result.encrypted = fileStat.m_is_encrypted != 0;
        result.supported = fileStat.m_is_supported != 0;
        return true;
    }

    bool findEntry(const char *name, quint32 &index)
    {
        if (!isUsable() || !name || !*name) {
            return false;
        }

        mz_uint32 minizIndex = 0;
        if (!mz_zip_reader_locate_file_v2(&m_archive, name, nullptr, kNoZipFlags, &minizIndex)
            || !isUsable()) {
            return false;
        }

        index = static_cast<quint32>(minizIndex);
        return index < entryCount();
    }

    bool extractEntry(quint32 index, quint64 maximumUncompressedBytes, QByteArray &data,
                      quint64 maximumCompressedBytes = std::numeric_limits<quint64>::max())
    {
        mz_zip_archive_file_stat fileStat{};
        if (!rawEntryStat(index, fileStat)) {
            return false;
        }

        const quint64 compressedSize = static_cast<quint64>(fileStat.m_comp_size);
        const quint64 uncompressedSize = static_cast<quint64>(fileStat.m_uncomp_size);
        const quint64 localHeaderOffset = static_cast<quint64>(fileStat.m_local_header_ofs);
        const quint64 archiveSize = static_cast<quint64>(m_context->archiveSize);

        if (fileStat.m_is_directory || fileStat.m_is_encrypted || !fileStat.m_is_supported
            || uncompressedSize == 0 || uncompressedSize > maximumUncompressedBytes
            || uncompressedSize > static_cast<quint64>(std::numeric_limits<size_t>::max())
            || uncompressedSize > static_cast<quint64>(std::numeric_limits<qsizetype>::max())
            || compressedSize == 0 || compressedSize > maximumCompressedBytes
            || localHeaderOffset > archiveSize || compressedSize > archiveSize - localHeaderOffset) {
            return false;
        }

        const auto outputSize = static_cast<qsizetype>(uncompressedSize);
        data.clear();
        data.resize(outputSize);
        if (data.size() != outputSize
            || !mz_zip_reader_extract_to_mem(&m_archive, static_cast<mz_uint>(index), data.data(),
                                             static_cast<size_t>(outputSize), kNoZipFlags)
            || !isUsable()) {
            data.clear();
            return false;
        }

        return true;
    }

private:
    bool isUsable() const
    {
        return m_initialized && m_context && !m_context->failed;
    }


    bool readEntryName(quint32 index, QString &name)
    {
        if (!isUsable() || index >= entryCount()) {
            return false;
        }

        const mz_uint requiredBytes = mz_zip_reader_get_filename(&m_archive, static_cast<mz_uint>(index), nullptr, 0);
        if (requiredBytes == 0 || requiredBytes > kMaximumArchiveFileNameBytes + 1U) {
            return false;
        }

        QByteArray fileNameData;
        fileNameData.resize(static_cast<qsizetype>(requiredBytes));
        if (fileNameData.size() != static_cast<qsizetype>(requiredBytes)) {
            return false;
        }

        const mz_uint bytesWritten = mz_zip_reader_get_filename(
            &m_archive, static_cast<mz_uint>(index), fileNameData.data(), requiredBytes);
        if (bytesWritten != requiredBytes || !isUsable()) {
            return false;
        }

        name = QString::fromUtf8(fileNameData.constData(), static_cast<qsizetype>(requiredBytes - 1U));
        return true;
    }

    bool rawEntryStat(quint32 index, mz_zip_archive_file_stat &fileStat)
    {
        if (!isUsable() || index >= entryCount()) {
            return false;
        }

        return mz_zip_reader_file_stat(&m_archive, static_cast<mz_uint>(index), &fileStat) != 0
            && isUsable();
    }

    void close()
    {
        if (m_initialized) {
            mz_zip_reader_end(&m_archive);
            m_initialized = false;
        }
        m_context = nullptr;
    }

    mz_zip_archive m_archive{};
    DeviceReadContext *m_context = nullptr;
    bool m_initialized = false;
};

} // namespace QimgvZipInternal
