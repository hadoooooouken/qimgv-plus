#include "pngtextchunkreader.h"
#include "comfymetadatalimits.h"

#include <QFile>
#include <QScopeGuard>

#include <array>
#include <cstring>
#include <limits>
#include <zlib.h>

namespace {
constexpr std::size_t kInflateBufferBytes = 8192;
constexpr qsizetype kPngSignatureBytes = 8;
constexpr qsizetype kPngChunkFieldBytes = 4;
constexpr qint64 kPngCrcBytes = 4;
constexpr char kZlibCompressionMethod = '\0';
}

static quint32 readBigEndianU32(const uchar *p)
{
    return (quint32(p[0]) << 24) | (quint32(p[1]) << 16) |
           (quint32(p[2]) << 8)  |  quint32(p[3]);
}

std::expected<QByteArray, QString>
PngTextChunkReader::zlibInflate(const QByteArray &compressed, qsizetype maxOutputBytes)
{
    if (compressed.isEmpty())
        return std::unexpected(QStringLiteral("Compressed PNG text payload is empty"));
    if (maxOutputBytes < 0 ||
        compressed.size() > static_cast<qsizetype>(std::numeric_limits<uInt>::max())) {
        return std::unexpected(QStringLiteral("Compressed PNG text parameters are invalid"));
    }

    z_stream strm{};
    if (inflateInit(&strm) != Z_OK)
        return std::unexpected(QStringLiteral("Failed to initialize zlib"));
    const auto inflateCleanup = qScopeGuard([&strm] {
        inflateEnd(&strm);
    });

    strm.next_in  = reinterpret_cast<Bytef *>(const_cast<char *>(compressed.data()));
    strm.avail_in = static_cast<uInt>(compressed.size());

    QByteArray out;
    std::array<char, kInflateBufferBytes> buffer{};

    while (true) {
        strm.next_out = reinterpret_cast<Bytef *>(buffer.data());
        strm.avail_out = static_cast<uInt>(buffer.size());

        const int inflateResult = inflate(&strm, Z_NO_FLUSH);
        if (inflateResult != Z_OK && inflateResult != Z_STREAM_END) {
            return std::unexpected(
                QStringLiteral("Failed to inflate PNG text metadata: %1")
                    .arg(QString::fromLatin1(zError(inflateResult))));
        }

        const qsizetype producedBytes =
            static_cast<qsizetype>(buffer.size() - strm.avail_out);
        if (producedBytes > maxOutputBytes - out.size()) {
            return std::unexpected(
                QStringLiteral("Decompressed PNG text metadata exceeds the %1-byte limit")
                    .arg(maxOutputBytes));
        }
        out.append(buffer.data(), producedBytes);

        if (inflateResult == Z_STREAM_END)
            return out;

        if (producedBytes == 0 && strm.avail_in == 0) {
            return std::unexpected(
                QStringLiteral("Compressed PNG text metadata ended before the zlib stream"));
        }
    }
}

std::expected<QMap<QString, QByteArray>, QString>
PngTextChunkReader::readTextChunks(const QString &pngPath, bool stopAtIDAT)
{
    QMap<QString, QByteArray> result;
    qsizetype encodedMetadataBytes = 0;
    qsizetype decodedMetadataBytes = 0;
    qsizetype textChunkCount = 0;

    QFile file(pngPath);
    if (!file.open(QIODevice::ReadOnly))
        return std::unexpected(
            QStringLiteral("Failed to open file: %1").arg(file.errorString()));

    static const char pngSig[kPngSignatureBytes] = {
        '\x89', 'P', 'N', 'G', '\r', '\n', '\x1a', '\n'
    };
    QByteArray sig = file.read(kPngSignatureBytes);
    if (sig.size() != kPngSignatureBytes ||
        std::memcmp(sig.constData(), pngSig, kPngSignatureBytes) != 0) {
        return std::unexpected(QStringLiteral("File is not a PNG (invalid signature)"));
    }

    while (!file.atEnd()) {
        QByteArray lenBuf = file.read(kPngChunkFieldBytes);
        if (lenBuf.size() != kPngChunkFieldBytes)
            break;
        quint32 len = readBigEndianU32(reinterpret_cast<const uchar *>(lenBuf.constData()));

        QByteArray type = file.read(kPngChunkFieldBytes);
        if (type.size() != kPngChunkFieldBytes)
            break;

        if (type == "IEND")
            break;
        if (stopAtIDAT && type == "IDAT")
            break;

        // Guard against corrupted/truncated/malicious files: the chunk header's
        // length isn't validated by the PNG spec at this point, and a garbage
        // value (e.g. 0xFFFFFFFF) without bounds checking would try to allocate
        // ~4GB and crash with std::bad_alloc. Clamp against both the remaining
        // file length and an absolute per-chunk ceiling.
        const qint64 remaining = file.size() - file.pos();
        if (len > ComfyMetadataLimits::kMaxPngChunkBytes ||
            static_cast<qint64>(len) + kPngCrcBytes > remaining) {
            return std::unexpected(QStringLiteral("PNG chunk length is invalid or too large"));
        }

        const bool isTextChunk = type == "tEXt" || type == "zTXt";
        if (isTextChunk) {
            ++textChunkCount;
            if (textChunkCount > ComfyMetadataLimits::kMaxTextChunkCount) {
                return std::unexpected(
                    QStringLiteral("PNG contains more than %1 text chunks")
                        .arg(ComfyMetadataLimits::kMaxTextChunkCount));
            }
            if (static_cast<qsizetype>(len) >
                ComfyMetadataLimits::kMaxMetadataBytes - encodedMetadataBytes) {
                return std::unexpected(
                    QStringLiteral("Encoded PNG text metadata exceeds the %1-byte total limit")
                        .arg(ComfyMetadataLimits::kMaxMetadataBytes));
            }
            encodedMetadataBytes += static_cast<qsizetype>(len);
        }

        QByteArray data = file.read(len);
        if (data.size() != static_cast<int>(len))
            return std::unexpected(QStringLiteral("PNG ended in the middle of a chunk"));
        if (file.read(kPngCrcBytes).size() != kPngCrcBytes)
            return std::unexpected(QStringLiteral("PNG ended before a chunk CRC"));
        // The CRC is consumed but not verified.

        if (type == "tEXt") {
            int nullPos = data.indexOf('\0');
            if (nullPos < 0)
                continue;
            QString keyword = QString::fromLatin1(data.left(nullPos));
            QByteArray text  = data.mid(nullPos + 1);
            if (text.size() >
                ComfyMetadataLimits::kMaxMetadataBytes - decodedMetadataBytes) {
                return std::unexpected(
                    QStringLiteral("PNG text metadata exceeds the %1-byte total limit")
                        .arg(ComfyMetadataLimits::kMaxMetadataBytes));
            }
            decodedMetadataBytes += text.size();
            result.insert(keyword, text);
        } else if (type == "zTXt") {
            int nullPos = data.indexOf('\0');
            if (nullPos < 0 || nullPos + 1 >= data.size())
                continue;
            QString keyword = QString::fromLatin1(data.left(nullPos));
            // the byte right after the null terminator is the compression
            // method (always 0 = zlib)
            if (data.at(nullPos + 1) != kZlibCompressionMethod)
                continue;
            QByteArray compressed = data.mid(nullPos + 2);
            auto text = zlibInflate(
                compressed,
                ComfyMetadataLimits::kMaxMetadataBytes - decodedMetadataBytes);
            if (!text)
                return std::unexpected(text.error());
            decodedMetadataBytes += text->size();
            result.insert(keyword, *text);
        }
        // iTXt is virtually never used in comfy files; if needed it can be
        // added following the same approach (see the PNG spec).
    }

    return result;
}
