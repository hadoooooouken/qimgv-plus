#include "pngtextchunkreader.h"

#include <QFile>
#include <zlib.h>

static quint32 readBigEndianU32(const uchar *p)
{
    return (quint32(p[0]) << 24) | (quint32(p[1]) << 16) |
           (quint32(p[2]) << 8)  |  quint32(p[3]);
}

QByteArray PngTextChunkReader::zlibInflate(const QByteArray &compressed)
{
    if (compressed.isEmpty())
        return {};

    z_stream strm{};
    if (inflateInit(&strm) != Z_OK)
        return {};

    strm.next_in  = reinterpret_cast<Bytef *>(const_cast<char *>(compressed.data()));
    strm.avail_in = static_cast<uInt>(compressed.size());

    QByteArray out;
    char buffer[8192];
    int ret;
    do {
        strm.next_out  = reinterpret_cast<Bytef *>(buffer);
        strm.avail_out = sizeof(buffer);
        ret = inflate(&strm, Z_NO_FLUSH);
        if (ret != Z_OK && ret != Z_STREAM_END && ret != Z_BUF_ERROR) {
            inflateEnd(&strm);
            return {};
        }
        out.append(buffer, sizeof(buffer) - strm.avail_out);
    } while (ret != Z_STREAM_END && strm.avail_out == 0);

    inflateEnd(&strm);
    return out;
}

std::expected<QMap<QString, QByteArray>, QString>
PngTextChunkReader::readTextChunks(const QString &pngPath, bool stopAtIDAT)
{
    QMap<QString, QByteArray> result;

    QFile file(pngPath);
    if (!file.open(QIODevice::ReadOnly))
        return std::unexpected(
            QStringLiteral("Failed to open file: %1").arg(file.errorString()));

    static const char pngSig[8] = { '\x89','P','N','G','\r','\n','\x1a','\n' };
    QByteArray sig = file.read(8);
    if (sig.size() != 8 || memcmp(sig.constData(), pngSig, 8) != 0)
        return std::unexpected(QStringLiteral("File is not a PNG (invalid signature)"));

    while (!file.atEnd()) {
        QByteArray lenBuf = file.read(4);
        if (lenBuf.size() != 4)
            break;
        quint32 len = readBigEndianU32(reinterpret_cast<const uchar *>(lenBuf.constData()));

        QByteArray type = file.read(4);
        if (type.size() != 4)
            break;

        if (type == "IEND")
            break;
        if (stopAtIDAT && type == "IDAT")
            break;

        // Guard against corrupted/truncated/malicious files: the chunk header's
        // length isn't validated by the PNG spec at this point, and a garbage
        // value (e.g. 0xFFFFFFFF) without bounds checking would try to allocate
        // ~4GB and crash with std::bad_alloc. Clamp against both the remaining
        // file length and a reasonable ceiling for a text chunk's size.
        constexpr quint32 kMaxTextChunkSize = 64u * 1024u * 1024u; // 64 MB, generous margin
        qint64 remaining = file.size() - file.pos();
        if (len > kMaxTextChunkSize || static_cast<qint64>(len) + 4 > remaining)
            break;

        QByteArray data = file.read(len);
        if (data.size() != static_cast<int>(len))
            break; // file ended in the middle of a chunk
        file.read(4); // CRC, not verified

        if (type == "tEXt") {
            int nullPos = data.indexOf('\0');
            if (nullPos < 0)
                continue;
            QString keyword = QString::fromLatin1(data.left(nullPos));
            QByteArray text  = data.mid(nullPos + 1);
            result.insert(keyword, text);
        } else if (type == "zTXt") {
            int nullPos = data.indexOf('\0');
            if (nullPos < 0)
                continue;
            QString keyword = QString::fromLatin1(data.left(nullPos));
            // the byte right after the null terminator is the compression
            // method (always 0 = zlib)
            QByteArray compressed = data.mid(nullPos + 2);
            QByteArray text = zlibInflate(compressed);
            result.insert(keyword, text);
        }
        // iTXt is virtually never used in comfy files; if needed it can be
        // added following the same approach (see the PNG spec).
    }

    return result;
}
