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

struct SevenZipEntryStat
{
    quint32 index = 0;
    QString name;
    quint64 uncompressedSize = 0;
    bool directory = false;
    bool supported = true;
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
