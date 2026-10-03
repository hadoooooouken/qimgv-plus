/*
    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "sevenzipreader_p.h"

#include "7z.h"
#include "7zAlloc.h"
#include "7zCrc.h"
#include "7zTypes.h"

#include <QVector>
#include <algorithm>
#include <cstdlib>
#include <mutex>

namespace QimgvSevenZipInternal
{

namespace
{

void *safeSzAlloc(ISzAllocPtr, size_t size)
{
    if (size == 0 || size > kMaximumAllocationBytes) {
        return nullptr;
    }
    return std::malloc(size);
}

void safeSzFree(ISzAllocPtr, void *address)
{
    std::free(address);
}

const ISzAlloc g_SafeAlloc = { safeSzAlloc, safeSzFree };

struct CQIODeviceInStream
{
    ISeekInStream vt;
    QIODevice *device = nullptr;
    quint64 archiveSize = 0;
    bool failed = false;
};

CQIODeviceInStream *getStream(ISeekInStreamPtr pp)
{
    return static_cast<CQIODeviceInStream *>(const_cast<void *>(static_cast<const void *>(pp)));
}

SRes QIODeviceInStream_Read(ISeekInStreamPtr pp, void *buf, size_t *size)
{
    auto *stream = getStream(pp);
    if (!stream || stream->failed || !stream->device || !stream->device->isOpen() || !stream->device->isReadable()
        || !buf || !size) {
        if (stream) {
            stream->failed = true;
        }
        if (size) {
            *size = 0;
        }
        return SZ_ERROR_READ;
    }

    if (*size == 0) {
        return SZ_OK;
    }

    const qint64 maxToRead = static_cast<qint64>(std::min<size_t>(*size, static_cast<size_t>(std::numeric_limits<qint64>::max())));
    const qint64 bytesRead = stream->device->read(static_cast<char *>(buf), maxToRead);
    if (bytesRead < 0) {
        stream->failed = true;
        *size = 0;
        return SZ_ERROR_READ;
    }

    *size = static_cast<size_t>(bytesRead);
    return SZ_OK;
}

SRes QIODeviceInStream_Seek(ISeekInStreamPtr pp, Int64 *pos, ESzSeek origin)
{
    auto *stream = getStream(pp);
    if (!stream || stream->failed || !stream->device || !stream->device->isOpen() || !pos) {
        if (stream) {
            stream->failed = true;
        }
        return SZ_ERROR_READ;
    }

    qint64 target = 0;
    switch (origin) {
    case SZ_SEEK_SET:
        target = static_cast<qint64>(*pos);
        break;
    case SZ_SEEK_CUR:
        target = stream->device->pos() + static_cast<qint64>(*pos);
        break;
    case SZ_SEEK_END:
        target = stream->device->size() + static_cast<qint64>(*pos);
        break;
    default:
        stream->failed = true;
        return SZ_ERROR_READ;
    }

    if (target < 0 || target > static_cast<qint64>(stream->archiveSize) || !stream->device->seek(target)
        || stream->device->pos() != target) {
        stream->failed = true;
        return SZ_ERROR_READ;
    }

    *pos = static_cast<Int64>(stream->device->pos());
    return SZ_OK;
}

void ensureCrcTableInitialized()
{
    static std::once_flag once;
    std::call_once(once, []() {
        CrcGenerateTable();
    });
}

} // namespace

struct SevenZipReader::Impl
{
    CQIODeviceInStream ioStream;
    CLookToRead2 lookStream;
    QByteArray lookBuffer;
    CSzArEx db;
    UInt32 solidBlockIndex = 0xFFFFFFFF;
    Byte *solidBuffer = nullptr;
    size_t solidBufferSize = 0;
    bool initialized = false;

    Impl()
    {
        ioStream.vt.Read = QIODeviceInStream_Read;
        ioStream.vt.Seek = QIODeviceInStream_Seek;
        LookToRead2_CreateVTable(&lookStream, False);
        lookStream.realStream = &ioStream.vt;
        lookStream.buf = nullptr;
        lookStream.bufSize = 0;
        LookToRead2_INIT(&lookStream);
        SzArEx_Init(&db);
    }

    ~Impl()
    {
        close();
    }

    void close()
    {
        if (solidBuffer) {
            safeSzFree(nullptr, solidBuffer);
            solidBuffer = nullptr;
        }
        solidBufferSize = 0;
        solidBlockIndex = 0xFFFFFFFF;

        if (initialized) {
            ISzAlloc alloc = g_SafeAlloc;
            SzArEx_Free(&db, &alloc);
            initialized = false;
        }

        lookBuffer.clear();
        lookStream.buf = nullptr;
        lookStream.bufSize = 0;
        LookToRead2_INIT(&lookStream);

        ioStream.device = nullptr;
        ioStream.archiveSize = 0;
        ioStream.failed = false;
    }
};

SevenZipReader::SevenZipReader()
    : m_impl(std::make_unique<Impl>())
{
}

SevenZipReader::~SevenZipReader() = default;

bool SevenZipReader::initialize(QIODevice *device, quint64 archiveSize)
{
    close();
    if (!device || !device->isOpen() || !device->isReadable() || device->isSequential()
        || archiveSize == 0 || archiveSize > static_cast<quint64>(std::numeric_limits<qint64>::max())) {
        return false;
    }

    ensureCrcTableInitialized();

    m_impl->ioStream.device = device;
    m_impl->ioStream.archiveSize = archiveSize;
    m_impl->ioStream.failed = false;

    m_impl->lookBuffer.resize(static_cast<qsizetype>(kLookStreamBufferSize));
    m_impl->lookStream.buf = reinterpret_cast<Byte *>(m_impl->lookBuffer.data());
    m_impl->lookStream.bufSize = kLookStreamBufferSize;
    m_impl->lookStream.realStream = &m_impl->ioStream.vt;
    LookToRead2_INIT(&m_impl->lookStream);

    ISzAlloc alloc = g_SafeAlloc;
    SzArEx_Init(&m_impl->db);
    const SRes openRes = SzArEx_Open(&m_impl->db, &m_impl->lookStream.vt, &alloc, &alloc);
    if (openRes != SZ_OK || m_impl->ioStream.failed) {
        m_impl->close();
        return false;
    }

    m_impl->initialized = true;

    if (m_impl->db.NumFiles > kMaximumArchiveEntryCount) {
        close();
        return false;
    }

    return true;
}

quint32 SevenZipReader::entryCount() const
{
    if (!m_impl->initialized || m_impl->ioStream.failed) {
        return 0;
    }
    return m_impl->db.NumFiles;
}

bool SevenZipReader::entryStat(quint32 index, SevenZipEntryStat &result) const
{
    if (!m_impl->initialized || m_impl->ioStream.failed || index >= m_impl->db.NumFiles) {
        return false;
    }

    const size_t nameLen = SzArEx_GetFileNameUtf16(&m_impl->db, index, nullptr);
    if (nameLen == 0 || nameLen > kMaximumArchiveFileNameChars) {
        return false;
    }

    QVector<UInt16> nameBuf(static_cast<qsizetype>(nameLen));
    SzArEx_GetFileNameUtf16(&m_impl->db, index, nameBuf.data());

    result.index = index;
    result.name = (nameLen > 1)
        ? QString::fromUtf16(reinterpret_cast<const char16_t *>(nameBuf.constData()), static_cast<qsizetype>(nameLen - 1))
        : QString();
    result.directory = SzArEx_IsDir(&m_impl->db, index) != 0;
    result.uncompressedSize = SzArEx_GetFileSize(&m_impl->db, index);
    result.supported = true;
    return true;
}

bool SevenZipReader::extractEntry(quint32 index, quint64 maxBytes, QByteArray &data)
{
    data.clear();
    if (!m_impl->initialized || m_impl->ioStream.failed || index >= m_impl->db.NumFiles) {
        return false;
    }

    if (SzArEx_IsDir(&m_impl->db, index)) {
        return false;
    }

    const quint64 fileSize = SzArEx_GetFileSize(&m_impl->db, index);
    if (fileSize == 0 || fileSize > maxBytes
        || fileSize > static_cast<quint64>(std::numeric_limits<size_t>::max())
        || fileSize > static_cast<quint64>(std::numeric_limits<qsizetype>::max())) {
        return false;
    }

    size_t offset = 0;
    size_t outSizeProcessed = 0;
    ISzAlloc alloc = g_SafeAlloc;

    const SRes extractRes = SzArEx_Extract(
        &m_impl->db,
        &m_impl->lookStream.vt,
        index,
        &m_impl->solidBlockIndex,
        &m_impl->solidBuffer,
        &m_impl->solidBufferSize,
        &offset,
        &outSizeProcessed,
        &alloc,
        &alloc);

    if (extractRes != SZ_OK || !m_impl->solidBuffer || m_impl->ioStream.failed) {
        data.clear();
        return false;
    }

    if (outSizeProcessed == 0 || outSizeProcessed > maxBytes
        || offset > m_impl->solidBufferSize
        || outSizeProcessed > m_impl->solidBufferSize - offset) {
        data.clear();
        return false;
    }

    data = QByteArray(reinterpret_cast<const char *>(m_impl->solidBuffer + offset),
                      static_cast<qsizetype>(outSizeProcessed));
    return true;
}

void SevenZipReader::close()
{
    m_impl->close();
}

} // namespace QimgvSevenZipInternal
