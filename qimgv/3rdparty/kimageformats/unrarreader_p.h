/*
    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>
#include <cstdint>
#include <limits>
#include <memory>

namespace QimgvRarInternal {

constexpr quint64 kRarBytesPerMebibyte = 1024ULL * 1024ULL;

// Maximum allowed uncompressed size of a single image entry (256 MiB).
constexpr quint64 kRarMaximumPageSourceBytes = 256ULL * kRarBytesPerMebibyte;

// Maximum number of entries scanned from one archive.
constexpr quint32 kRarMaximumArchiveEntryCount = 100'000U;

// Maximum Unicode filename length accepted from archive headers.
constexpr quint32 kRarMaximumFileNameChars = 65'535U;

// Per-entry metadata collected during archive indexing.
struct RarPageEntry {
    quint32 archiveOrder = 0;     // 0-based sequential scan position (used for extraction)
    QString path;                  // Normalised path with forward slashes
    QByteArray format;             // Image format hint ("jpg", "png", …)
    quint64 uncompressedSize = 0;  // From RARHeaderDataEx.UnpSize / UnpSizeHigh
};

class UnrarReader final {
public:
    UnrarReader();
    ~UnrarReader();

    UnrarReader(const UnrarReader &) = delete;
    UnrarReader &operator=(const UnrarReader &) = delete;

    // Scan the archive and index all readable image entries.
    // filePath must be a real filesystem path (not an in-memory device).
    // Returns true if at least one image entry was found.
    bool initialize(const QString &filePath, quint64 archiveSize);

    // Number of image entries found, naturally sorted.
    quint32 entryCount() const;

    // Returns the entry at natural-sorted position sortedIndex.
    bool entryInfo(quint32 sortedIndex, RarPageEntry &entry) const;

    // Extracts the entry identified by archiveOrder into data.
    // Aborts if accumulated bytes would exceed maxBytes.
    bool extractEntry(quint32 archiveOrder, quint64 maxBytes, QByteArray &data);

    void close();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace QimgvRarInternal
