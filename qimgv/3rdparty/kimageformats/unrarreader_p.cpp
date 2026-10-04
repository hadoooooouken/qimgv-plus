/*
    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "unrarreader_p.h"

// Windows types (HANDLE, LPARAM, CALLBACK, …) must be visible before dll.hpp.
#define WIN32_LEAN_AND_MEAN
// Keep windows.h from defining min/max macros: they break std::numeric_limits<T>::max().
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

// UnRAR embedded library API (RARDLL mode).
// Compiled definitions: RARDLL, UNRAR, SILENT, _WIN_ALL are set on qimgv_unrar_sdk,
// not on this translation unit.  We only consume the public C API declared here.
#include "dll.hpp"

#include <QCollator>
#include <QString>
#include <QVector>

#include <algorithm>
#include <limits>
#include <wchar.h>

namespace QimgvRarInternal {

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

namespace {

// Conservative limitation: the indexer reads names through
// RARHeaderDataEx::FileNameW, a fixed buffer that UnRAR fills with wcsncpyz(),
// so longer names are silently truncated to (buffer size - 1) characters.
// A name that fills the buffer cannot be told apart from a truncated one, so
// such entries are skipped instead of being matched on a possibly cut-off
// extension. Supporting longer names would require the FileNameEx buffer.
constexpr size_t kRarFileNameBufferChars =
    sizeof(RARHeaderDataEx::FileNameW) / sizeof(wchar_t);
constexpr size_t kRarMaximumReliableFileNameChars = kRarFileNameBufferChars - 1;

// Determine an image format tag from a file path.
// Returns an empty QByteArray for unsupported / non-image extensions.
QByteArray rarPageFormat(const QString &path)
{
    const int dot = path.lastIndexOf(u'.');
    if (dot < 0)
        return {};

    const QByteArray suffix = path.mid(dot + 1).toLower().toLatin1();

    // Mirrors the format set accepted by the 7z / CBZ plugins.
    static const QByteArray kSupported[] = {
        "jpg", "jpeg", "png", "webp", "avif", "jxl",
        "heic", "heif", "bmp", "gif", "tif", "tiff", "qoi"
    };
    for (const QByteArray &s : kSupported) {
        if (suffix == s) {
            // Canonicalise the reader hint: "tif" → "tiff".
            if (suffix == "tif")
                return QByteArray("tiff");
            return suffix;
        }
    }
    return {};
}

// Reject path components that indicate junk, hidden, or traversal entries.
// Mirrors the identical check in the 7z plugin.
bool isRarHiddenOrJunkPath(QString path)
{
    path.replace(u'\\', u'/');
    if (path.startsWith(u'/'))
        return true;

    const QStringList parts = path.split(u'/', Qt::SkipEmptyParts);
    if (parts.isEmpty())
        return true;

    for (const QString &part : parts) {
        if (part == u".." || part.startsWith(u'.')
            || part.compare(u"__MACOSX", Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

// Natural-order comparator used to sort collected image entries.
// Uses QCollator with numeric mode and case-insensitive comparison,
// with a secondary case-sensitive tiebreak — identical to the 7z plugin.
bool rarNaturalPathLessThan(const RarPageEntry &left, const RarPageEntry &right)
{
    static thread_local QCollator collator = [] {
        QCollator c;
        c.setNumericMode(true);
        c.setCaseSensitivity(Qt::CaseInsensitive);
        return c;
    }();

    const int cmp = collator.compare(left.path, right.path);
    if (cmp != 0)
        return cmp < 0;
    return QString::compare(left.path, right.path, Qt::CaseSensitive) < 0;
}

// ---------------------------------------------------------------------------
// RAII wrapper for the opaque RAR HANDLE returned by RAROpenArchiveEx.
// ---------------------------------------------------------------------------
struct RarHandleDeleter {
    void operator()(HANDLE h) const noexcept
    {
        if (h)
            RARCloseArchive(h);
    }
};
using UniqueRarHandle = std::unique_ptr<void, RarHandleDeleter>;

// ---------------------------------------------------------------------------
// Context threaded through the extraction UNRARCALLBACK.
// ---------------------------------------------------------------------------
struct ExtractionContext {
    QByteArray *data = nullptr;
    quint64 maxBytes = 0;
    bool aborted = false;
};

// Reply to UCM_CHANGEVOLUME[W].  p2 is RAR_VOL_ASK when the next volume could
// not be found and RAR_VOL_NOTIFY when it was opened successfully.
int rarVolumeChangeResult(LPARAM p2) noexcept
{
    return p2 == RAR_VOL_NOTIFY ? 1 : -1;
}

// ---------------------------------------------------------------------------
// UNRARCALLBACK used during entry extraction (RAR_OM_EXTRACT).
//
// UCM_PROCESSDATA:
//   Accumulate the decompressed chunk (P1 = data pointer, P2 = byte count).
//   Return −1 to signal abort if the size limit is exceeded.
//
// UCM_CHANGEVOLUME / UCM_CHANGEVOLUMEW:
//   RAR_VOL_NOTIFY (next volume opened): return 1 (allow).
//   RAR_VOL_ASK (next volume is missing): return −1 (abort).  Returning 1 with
//   an unchanged name means "wait until the volume appears", which makes UnRAR
//   retry in a busy loop forever.  Aborting makes RARProcessFile return
//   ERAR_EOPEN, which we treat as a graceful failure.
//
// UCM_NEEDPASSWORD / UCM_NEEDPASSWORDW:
//   Return −1 (reject).  No password UI is provided for encrypted archives.
//
// UCM_LARGEDICT and any future message:
//   Return −1 (reject).
// ---------------------------------------------------------------------------
int CALLBACK rarExtractionCallback(UINT msg, LPARAM userData, LPARAM p1, LPARAM p2)
{
    switch (msg) {
    case UCM_PROCESSDATA: {
        auto *ctx = reinterpret_cast<ExtractionContext *>(static_cast<uintptr_t>(userData));
        if (!ctx || ctx->aborted)
            return -1;

        // p1 is the data pointer (LPARAM is LONG_PTR, 64-bit on x64).
        const auto *addr = reinterpret_cast<const char *>(static_cast<uintptr_t>(p1));

        // p2 carries size_t cast to LPARAM.  For our capped sizes the value
        // is always positive and fits in quint64.
        if (p2 <= 0)
            return 1; // Zero-length write — nothing to do.
        const auto count = static_cast<quint64>(static_cast<DWORD_PTR>(p2));

        // Overflow-safe check: accumulated + count <= maxBytes
        const quint64 accumulated = static_cast<quint64>(ctx->data->size());
        if (count > ctx->maxBytes || accumulated > ctx->maxBytes - count) {
            ctx->aborted = true;
            return -1;
        }

        ctx->data->append(addr, static_cast<qsizetype>(count));
        return 1;
    }
    case UCM_CHANGEVOLUME:
    case UCM_CHANGEVOLUMEW:
        return rarVolumeChangeResult(p2);
    case UCM_NEEDPASSWORD:
    case UCM_NEEDPASSWORDW:
        return -1;
    default:
        // UCM_LARGEDICT (25) and any future messages: reject.
        return -1;
    }
}

// ---------------------------------------------------------------------------
// UNRARCALLBACK used during archive listing (RAR_OM_LIST).
// No data accumulation is needed; only volume / password messages matter.
// ---------------------------------------------------------------------------
int CALLBACK rarListingCallbackImpl(UINT msg, LPARAM /*userData*/, LPARAM /*p1*/, LPARAM p2)
{
    switch (msg) {
    case UCM_CHANGEVOLUME:
    case UCM_CHANGEVOLUMEW:
        return rarVolumeChangeResult(p2);
    case UCM_NEEDPASSWORD:
    case UCM_NEEDPASSWORDW:
        return -1;
    default:
        return -1;
    }
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// UnrarReader::Impl
// ---------------------------------------------------------------------------

struct UnrarReader::Impl {
    QString filePath;
    QVector<RarPageEntry> entries; // naturally-sorted image entries
    bool initialized = false;
};

// ---------------------------------------------------------------------------
// UnrarReader
// ---------------------------------------------------------------------------

UnrarReader::UnrarReader()
    : m_impl(std::make_unique<Impl>())
{
}

UnrarReader::~UnrarReader() = default;

bool UnrarReader::initialize(const QString &filePath, quint64 archiveSize)
{
    close();

    if (filePath.isEmpty()
        || archiveSize == 0
        || archiveSize > static_cast<quint64>(std::numeric_limits<qint64>::max())) {
        return false;
    }

    // Keep the wide path alive for the duration of the call.
    const std::wstring wpath = filePath.toStdWString();

    RAROpenArchiveDataEx openData{};
    openData.ArcNameW = const_cast<wchar_t *>(wpath.c_str());
    openData.OpenMode = RAR_OM_LIST;
    openData.OpFlags = ROADOF_SHARED;
    openData.Callback = rarListingCallbackImpl;
    openData.UserData = 0;

    UniqueRarHandle handle(RAROpenArchiveEx(&openData));

    if (!handle || openData.OpenResult != ERAR_SUCCESS)
        return false;

    // Reject archives with encrypted headers — no password UI.
    if (openData.Flags & ROADF_ENCHEADERS)
        return false;

    const bool isSolidArchive = (openData.Flags & ROADF_SOLID) != 0;

    quint32 scanOrder = 0;

    for (;;) {
        // Upper bound on the number of headers examined, regardless of how many
        // of them turn out to be images.
        if (scanOrder >= kRarMaximumScannedEntryCount)
            break;

        RARHeaderDataEx header{};
        const int readResult = RARReadHeaderEx(handle.get(), &header);

        if (readResult == ERAR_END_ARCHIVE)
            break;

        if (readResult != ERAR_SUCCESS) {
            // ERAR_BAD_DATA, ERAR_EOPEN (missing volume), ERAR_BAD_PASSWORD …
            // Stop scanning rather than hard-failing — entries collected so
            // far are still valid.
            break;
        }

        // Advance past the entry (list mode: just seeks to the next header).
        const int skipResult = RARProcessFileW(handle.get(), RAR_SKIP, nullptr, nullptr);
        if (skipResult != ERAR_SUCCESS && skipResult != ERAR_END_ARCHIVE)
            break;

        // Skip directories.
        if (header.Flags & RHDF_DIRECTORY) {
            ++scanOrder;
            continue;
        }

        // Encrypted file data cannot be extracted without a password, so the
        // entry is never offered as a page.
        if (header.Flags & RHDF_ENCRYPTED) {
            // In the current implementation extractEntry() reopens the archive
            // and reaches the target with RAR_SKIP. In a solid archive that
            // skip may need to process the preceding entries, which is
            // impossible for an encrypted one without a password. Entries
            // after it therefore cannot be reliably extracted, so indexing
            // stops here. Entries indexed so far precede it and stay valid.
            if (isSolidArchive)
                break;
            ++scanOrder;
            continue;
        }

        // Build a normalized, null-safe path string.
        // RARHeaderDataEx.FileNameW is always null-terminated by the API, but
        // names that fill the buffer may have been truncated (see
        // kRarMaximumReliableFileNameChars).
        QString path = QString::fromWCharArray(header.FileNameW);
        if (path.isEmpty()
            || static_cast<size_t>(path.size()) >= kRarMaximumReliableFileNameChars) {
            ++scanOrder;
            continue;
        }
        path.replace(u'\\', u'/');

        // Reject traversal / hidden / junk entries.
        if (isRarHiddenOrJunkPath(path)) {
            ++scanOrder;
            continue;
        }

        // Only accept entries whose extension maps to a supported image format.
        const QByteArray fmt = rarPageFormat(path);
        if (fmt.isEmpty()) {
            ++scanOrder;
            continue;
        }

        // Combine the high and low 32-bit halves into a 64-bit uncompressed size.
        const quint64 unpSize = static_cast<quint64>(header.UnpSize)
            | (static_cast<quint64>(header.UnpSizeHigh) << 32);

        if (unpSize == 0 || unpSize > kRarMaximumPageSourceBytes) {
            ++scanOrder;
            continue;
        }

        // Hard cap on accepted image entries; nothing more can be added.
        if (m_impl->entries.size() >= static_cast<qsizetype>(kRarMaximumImageEntryCount))
            break;

        m_impl->entries.push_back({scanOrder, std::move(path), fmt, unpSize});
        ++scanOrder;
    }

    if (m_impl->entries.isEmpty())
        return false;

    std::sort(m_impl->entries.begin(), m_impl->entries.end(), rarNaturalPathLessThan);

    m_impl->filePath = filePath;
    m_impl->initialized = true;
    return true;
}

quint32 UnrarReader::entryCount() const
{
    if (!m_impl->initialized)
        return 0;
    return static_cast<quint32>(m_impl->entries.size());
}

bool UnrarReader::entryInfo(quint32 sortedIndex, RarPageEntry &entry) const
{
    if (!m_impl->initialized
        || sortedIndex >= static_cast<quint32>(m_impl->entries.size())) {
        return false;
    }
    entry = m_impl->entries.at(static_cast<qsizetype>(sortedIndex));
    return true;
}

bool UnrarReader::extractEntry(quint32 archiveOrder, quint64 maxBytes, QByteArray &data)
{
    data.clear();

    if (!m_impl->initialized || m_impl->filePath.isEmpty() || maxBytes == 0)
        return false;

    const std::wstring wpath = m_impl->filePath.toStdWString();

    ExtractionContext ctx;
    ctx.data = &data;
    ctx.maxBytes = maxBytes;
    ctx.aborted = false;

    RAROpenArchiveDataEx openData{};
    openData.ArcNameW = const_cast<wchar_t *>(wpath.c_str());
    openData.OpenMode = RAR_OM_EXTRACT;
    openData.OpFlags = ROADOF_SHARED;
    openData.Callback = rarExtractionCallback;
    openData.UserData = reinterpret_cast<LPARAM>(&ctx);

    UniqueRarHandle handle(RAROpenArchiveEx(&openData));

    if (!handle || openData.OpenResult != ERAR_SUCCESS)
        return false;

    if (openData.Flags & ROADF_ENCHEADERS)
        return false;

    quint32 scanOrder = 0;
    bool found = false;

    for (;;) {
        RARHeaderDataEx header{};
        const int readResult = RARReadHeaderEx(handle.get(), &header);

        if (readResult == ERAR_END_ARCHIVE)
            break;

        if (readResult != ERAR_SUCCESS) {
            data.clear();
            return false;
        }

        if (scanOrder == archiveOrder) {
            // Extract this entry to memory via the UCM_PROCESSDATA callback.
            // RAR_TEST decompresses without writing to disk.
            const int processResult =
                RARProcessFileW(handle.get(), RAR_TEST, nullptr, nullptr);
            if (processResult != ERAR_SUCCESS || ctx.aborted) {
                data.clear();
                return false;
            }
            found = true;
            break;
        }

        // Skip entries before the target.
        // For solid archives, RAR_SKIP in RAR_OM_EXTRACT mode still
        // decompresses the entry internally (updating the LZ dictionary)
        // but does NOT invoke the UCM_PROCESSDATA callback, so our
        // accumulator stays clean.
        const int skipResult =
            RARProcessFileW(handle.get(), RAR_SKIP, nullptr, nullptr);
        if (skipResult != ERAR_SUCCESS && skipResult != ERAR_END_ARCHIVE) {
            data.clear();
            return false;
        }

        ++scanOrder;
    }

    if (!found || data.isEmpty()) {
        data.clear();
        return false;
    }

    return true;
}

void UnrarReader::close()
{
    m_impl->entries.clear();
    m_impl->entries.squeeze();
    m_impl->filePath.clear();
    m_impl->initialized = false;
}

} // namespace QimgvRarInternal
