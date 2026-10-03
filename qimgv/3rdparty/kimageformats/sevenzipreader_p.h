/*
    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#pragma once

#include <QByteArray>
#include <QIODevice>
#include <QString>
#include <cstddef>
#include <limits>
#include <memory>

namespace QimgvSevenZipInternal
{

constexpr quint64 kBytesPerMebibyte = 1024ULL * 1024ULL;
constexpr size_t kMaximumAllocationBytes = 512ULL * static_cast<size_t>(kBytesPerMebibyte);
constexpr quint32 kMaximumArchiveEntryCount = 100'000U;
constexpr quint32 kMaximumArchiveFileNameChars = 65'535U;
constexpr size_t kLookStreamBufferSize = 256ULL * 1024ULL;

// Per-entry metadata collected during archive indexing.
// Notes on limitations:
//   - 'supported' is intentionally absent: codec validation happens at extraction
//     time via SzArEx_Extract(). Unsupported-method entries fail gracefully in read().
//   - Encrypted archives are rejected entirely at SzArEx_Open() because AES
//     support (Aes.c / AesOpt.c) is not compiled into qimgv_7z_sdk.
struct SevenZipEntryStat
{
    quint32 index = 0;
    QString name;
    quint64 uncompressedSize = 0;
    bool directory = false;
};

class SevenZipReader final
{
public:
    SevenZipReader();
    ~SevenZipReader();

    SevenZipReader(const SevenZipReader &) = delete;
    SevenZipReader &operator=(const SevenZipReader &) = delete;

    bool initialize(QIODevice *device, quint64 archiveSize);
    quint32 entryCount() const;
    bool entryStat(quint32 index, SevenZipEntryStat &result) const;
    bool extractEntry(quint32 index, quint64 maxBytes, QByteArray &data);
    void close();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace QimgvSevenZipInternal
