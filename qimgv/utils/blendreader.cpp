#include "blendreader.h"

#include <QByteArray>
#include <QFile>

#include <zlib.h>
#include <zstd.h>

#include <algorithm>
#include <limits>

namespace {

constexpr qsizetype kLegacyHeaderSize = 12;
constexpr qsizetype kCurrentHeaderSize = 17;
constexpr quint32 kMaxPreviewDimension = 16384;
constexpr quint64 kMaxPreviewBytes = 64ull * 1024ull * 1024ull;
constexpr qsizetype kSkipBufferSize = 64 * 1024;

enum class BHeadLayout {
    Legacy32,
    Legacy64,
    Current64,
};

struct BlendHeader {
    bool littleEndian = true;
    BHeadLayout bheadLayout = BHeadLayout::Legacy64;
};

void setError(QString *error, const QString &message)
{
    if (error)
        *error = message;
}

quint32 readU32(const char *data, bool littleEndian)
{
    const auto *p = reinterpret_cast<const uchar *>(data);
    if (littleEndian) {
        return quint32(p[0]) |
               (quint32(p[1]) << 8) |
               (quint32(p[2]) << 16) |
               (quint32(p[3]) << 24);
    }
    return (quint32(p[0]) << 24) |
           (quint32(p[1]) << 16) |
           (quint32(p[2]) << 8) |
           quint32(p[3]);
}

quint64 readU64(const char *data, bool littleEndian)
{
    const auto *p = reinterpret_cast<const uchar *>(data);
    quint64 value = 0;
    if (littleEndian) {
        for (int i = 7; i >= 0; --i)
            value = (value << 8) | quint64(p[i]);
    } else {
        for (int i = 0; i < 8; ++i)
            value = (value << 8) | quint64(p[i]);
    }
    return value;
}

class BlendInput {
public:
    virtual ~BlendInput() = default;
    virtual bool readExact(char *dst, qsizetype size, QString *error) = 0;
    virtual bool skip(quint64 size, QString *error) = 0;
};

class FileBlendInput final : public BlendInput {
public:
    explicit FileBlendInput(QFile &file) : mFile(file) {}

    bool readExact(char *dst, qsizetype size, QString *error) override
    {
        qsizetype done = 0;
        while (done < size) {
            const qint64 n = mFile.read(dst + done, qint64(size - done));
            if (n <= 0) {
                if (mFile.error() != QFileDevice::NoError) {
                    setError(error, QStringLiteral("cannot read Blender file: %1")
                                        .arg(mFile.errorString()));
                } else {
                    setError(error, QStringLiteral("truncated Blender file"));
                }
                return false;
            }
            done += qsizetype(n);
        }
        return true;
    }

    bool skip(quint64 size, QString *error) override
    {
        if (size > quint64(std::numeric_limits<qint64>::max()) ||
            mFile.pos() > std::numeric_limits<qint64>::max() - qint64(size)) {
            setError(error, QStringLiteral("invalid Blender block length"));
            return false;
        }
        const qint64 target = mFile.pos() + qint64(size);
        if (target > mFile.size()) {
            setError(error, QStringLiteral("Blender block extends past end of file"));
            return false;
        }
        if (!mFile.seek(target)) {
            setError(error, QStringLiteral("cannot seek past Blender block"));
            return false;
        }
        return true;
    }

private:
    QFile &mFile;
};

class GzipBlendInput final : public BlendInput {
public:
    explicit GzipBlendInput(QFile &file) : mFile(file) {}

    ~GzipBlendInput() override
    {
        if (mInitialized)
            inflateEnd(&mStream);
    }

    bool init(QString *error)
    {
        mStream = {};
        const int result = inflateInit2(&mStream, 16 + MAX_WBITS);
        if (result != Z_OK) {
            setError(error, zlibError(QStringLiteral("cannot initialize gzip decompression"),
                                      result));
            return false;
        }

        mInitialized = true;
        mCompressed.resize(kSkipBufferSize);
        return true;
    }

    bool readExact(char *dst, qsizetype size, QString *error) override
    {
        qsizetype done = 0;
        while (done < size) {
            if (mStream.avail_in == 0) {
                const qint64 n = mFile.read(mCompressed.data(), mCompressed.size());
                if (n <= 0) {
                    if (mFile.error() != QFileDevice::NoError) {
                        setError(error,
                                 QStringLiteral("cannot read gzip-compressed .blend: %1")
                                     .arg(mFile.errorString()));
                    } else {
                        setError(error, QStringLiteral("truncated gzip-compressed .blend"));
                    }
                    return false;
                }

                mStream.next_in = reinterpret_cast<Bytef *>(mCompressed.data());
                mStream.avail_in = uInt(n);
            }

            const qsizetype remaining = size - done;
            const uInt outputCapacity = uInt(std::min<quint64>(
                quint64(remaining), quint64(std::numeric_limits<uInt>::max())));
            mStream.next_out = reinterpret_cast<Bytef *>(dst + done);
            mStream.avail_out = outputCapacity;

            const uInt inputBefore = mStream.avail_in;
            const int result = inflate(&mStream, Z_NO_FLUSH);
            const qsizetype produced = qsizetype(outputCapacity - mStream.avail_out);
            done += produced;

            if (result == Z_STREAM_END) {
                if (done == size)
                    return true;
                setError(error, QStringLiteral("truncated gzip-compressed .blend"));
                return false;
            }

            if (result != Z_OK && result != Z_BUF_ERROR) {
                setError(error, zlibError(QStringLiteral("cannot decompress gzip .blend"),
                                          result));
                return false;
            }

            if (produced == 0 && mStream.avail_in == inputBefore) {
                setError(error, zlibError(QStringLiteral("gzip decompressor made no progress"),
                                          result));
                return false;
            }
        }
        return true;
    }

    bool skip(quint64 size, QString *error) override
    {
        QByteArray scratch(kSkipBufferSize, Qt::Uninitialized);
        quint64 remaining = size;
        while (remaining != 0) {
            const qsizetype chunk = qsizetype(std::min<quint64>(
                remaining, quint64(scratch.size())));
            if (!readExact(scratch.data(), chunk, error))
                return false;
            remaining -= quint64(chunk);
        }
        return true;
    }

private:
    QString zlibError(const QString &prefix, int code) const
    {
        if (mStream.msg)
            return QStringLiteral("%1: %2").arg(prefix, QString::fromLatin1(mStream.msg));
        return QStringLiteral("%1 (zlib error %2)").arg(prefix).arg(code);
    }

    QFile &mFile;
    z_stream mStream{};
    bool mInitialized = false;
    QByteArray mCompressed;
};

class ZstdBlendInput final : public BlendInput {
public:
    explicit ZstdBlendInput(QFile &file) : mFile(file) {}

    ~ZstdBlendInput() override
    {
        if (mStream)
            ZSTD_freeDStream(mStream);
    }

    bool init(QString *error)
    {
        mStream = ZSTD_createDStream();
        if (!mStream) {
            setError(error, QStringLiteral("cannot create Zstandard decompression stream"));
            return false;
        }

        const size_t initResult = ZSTD_initDStream(mStream);
        if (ZSTD_isError(initResult)) {
            setError(error,
                     QStringLiteral("cannot initialize Zstandard decompression: %1")
                         .arg(QString::fromLatin1(ZSTD_getErrorName(initResult))));
            return false;
        }

        const size_t inputSize = ZSTD_DStreamInSize();
        if (inputSize > size_t(std::numeric_limits<qsizetype>::max())) {
            setError(error, QStringLiteral("invalid Zstandard input buffer size"));
            return false;
        }
        mCompressed.resize(qsizetype(inputSize));
        return true;
    }

    bool readExact(char *dst, qsizetype size, QString *error) override
    {
        qsizetype done = 0;
        while (done < size) {
            if (mInput.pos == mInput.size) {
                const qint64 n = mFile.read(mCompressed.data(), mCompressed.size());
                if (n <= 0) {
                    if (mFile.error() != QFileDevice::NoError) {
                        setError(error,
                                 QStringLiteral("cannot read Zstandard-compressed .blend: %1")
                                     .arg(mFile.errorString()));
                    } else {
                        setError(error, QStringLiteral("truncated Zstandard-compressed .blend"));
                    }
                    return false;
                }
                mInput.src = mCompressed.constData();
                mInput.size = size_t(n);
                mInput.pos = 0;
            }

            ZSTD_outBuffer output{
                dst + done,
                size_t(size - done),
                0,
            };
            const size_t inputBefore = mInput.pos;
            const size_t result = ZSTD_decompressStream(mStream, &output, &mInput);
            if (ZSTD_isError(result)) {
                setError(error,
                         QStringLiteral("cannot decompress Zstandard .blend: %1")
                             .arg(QString::fromLatin1(ZSTD_getErrorName(result))));
                return false;
            }

            done += qsizetype(output.pos);
            if (output.pos == 0 && mInput.pos == inputBefore) {
                setError(error, QStringLiteral("Zstandard decompressor made no progress"));
                return false;
            }
        }
        return true;
    }

    bool skip(quint64 size, QString *error) override
    {
        QByteArray scratch(kSkipBufferSize, Qt::Uninitialized);
        quint64 remaining = size;
        while (remaining != 0) {
            const qsizetype chunk = qsizetype(std::min<quint64>(
                remaining, quint64(scratch.size())));
            if (!readExact(scratch.data(), chunk, error))
                return false;
            remaining -= quint64(chunk);
        }
        return true;
    }

private:
    QFile &mFile;
    ZSTD_DStream *mStream = nullptr;
    QByteArray mCompressed;
    ZSTD_inBuffer mInput{nullptr, 0, 0};
};

bool readBlendHeader(BlendInput &input, BlendHeader *header, QString *error)
{
    QByteArray base(kLegacyHeaderSize, Qt::Uninitialized);
    if (!input.readExact(base.data(), base.size(), error))
        return false;

    if (!base.startsWith("BLENDER")) {
        setError(error, QStringLiteral("invalid Blender file header"));
        return false;
    }

    // Blender 5.0+ file-format header v1, e.g. BLENDER17-01v0500.
    // This format always uses little-endian LargeBHead8 blocks.
    if (base[7] == '1' && base[8] == '7') {
        if (base[9] != '-' || base[10] != '0' || base[11] != '1') {
            setError(error, QStringLiteral("unsupported Blender file-format header"));
            return false;
        }

        QByteArray extra(kCurrentHeaderSize - kLegacyHeaderSize, Qt::Uninitialized);
        if (!input.readExact(extra.data(), extra.size(), error))
            return false;
        if (extra[0] != 'v') {
            setError(error, QStringLiteral("unsupported Blender file-format header"));
            return false;
        }
        for (qsizetype i = 1; i < extra.size(); ++i) {
            if (extra[i] < '0' || extra[i] > '9') {
                setError(error, QStringLiteral("invalid Blender version field"));
                return false;
            }
        }

        header->littleEndian = true;
        header->bheadLayout = BHeadLayout::Current64;
        return true;
    }

    // Historic pre-5.0 fixed 12-byte header.
    if (base[7] == '-') {
        header->bheadLayout = BHeadLayout::Legacy64;
    } else if (base[7] == '_') {
        header->bheadLayout = BHeadLayout::Legacy32;
    } else {
        setError(error, QStringLiteral("invalid Blender pointer-size marker"));
        return false;
    }

    if (base[8] == 'v') {
        header->littleEndian = true;
    } else if (base[8] == 'V') {
        header->littleEndian = false;
    } else {
        setError(error, QStringLiteral("invalid Blender endianness marker"));
        return false;
    }

    for (qsizetype i = 9; i < kLegacyHeaderSize; ++i) {
        if (base[i] < '0' || base[i] > '9') {
            setError(error, QStringLiteral("invalid Blender version field"));
            return false;
        }
    }
    return true;
}

bool readBHead(BlendInput &input,
               const BlendHeader &blendHeader,
               QByteArray *code,
               quint64 *payloadLength,
               QString *error)
{
    qsizetype bheadSize = 0;
    qsizetype lengthOffset = 0;
    bool lengthIs64Bit = false;

    switch (blendHeader.bheadLayout) {
    case BHeadLayout::Legacy32:
        bheadSize = 20; // BHead4
        lengthOffset = 4;
        break;
    case BHeadLayout::Legacy64:
        bheadSize = 24; // SmallBHead8
        lengthOffset = 4;
        break;
    case BHeadLayout::Current64:
        bheadSize = 32; // LargeBHead8
        lengthOffset = 16;
        lengthIs64Bit = true;
        break;
    }

    QByteArray bhead(bheadSize, Qt::Uninitialized);
    if (!input.readExact(bhead.data(), bhead.size(), error)) {
        if (error && error->startsWith(QStringLiteral("truncated Blender")))
            *error = QStringLiteral("truncated Blender BHead");
        return false;
    }

    *code = bhead.left(4);
    if (lengthIs64Bit) {
        const quint64 length =
            readU64(bhead.constData() + lengthOffset, blendHeader.littleEndian);
        if (length > quint64(std::numeric_limits<qint64>::max())) {
            setError(error, QStringLiteral("invalid Blender block length"));
            return false;
        }
        *payloadLength = length;
    } else {
        const quint32 length =
            readU32(bhead.constData() + lengthOffset, blendHeader.littleEndian);
        if (length > quint32(std::numeric_limits<qint32>::max())) {
            setError(error, QStringLiteral("invalid Blender block length"));
            return false;
        }
        *payloadLength = length;
    }
    return true;
}

QImage readPreviewFromInput(BlendInput &input, QString *error)
{
    BlendHeader blendHeader;
    if (!readBlendHeader(input, &blendHeader, error))
        return {};

    for (;;) {
        QByteArray code;
        quint64 payloadLength = 0;
        if (!readBHead(input, blendHeader, &code, &payloadLength, error))
            return {};

        if (code == "REND") {
            if (!input.skip(payloadLength, error))
                return {};
            continue;
        }

        // Blender stores TEST immediately after the initial REND block(s).
        // Match Blender's own lightweight thumbnail reader and stop here
        // instead of scanning an arbitrarily large scene file.
        if (code != "TEST")
            return {};

        if (payloadLength < 8) {
            setError(error, QStringLiteral("truncated Blender TEST preview header"));
            return {};
        }

        char dimensions[8];
        if (!input.readExact(dimensions, qsizetype(sizeof(dimensions)), error)) {
            setError(error, QStringLiteral("truncated Blender TEST preview dimensions"));
            return {};
        }

        const quint32 width = readU32(dimensions, blendHeader.littleEndian);
        const quint32 height = readU32(dimensions + 4, blendHeader.littleEndian);
        if (width == 0 || height == 0 ||
            width > kMaxPreviewDimension || height > kMaxPreviewDimension) {
            setError(error, QStringLiteral("invalid Blender preview dimensions"));
            return {};
        }

        const quint64 pixelBytes = quint64(width) * quint64(height) * 4ull;
        if (pixelBytes > kMaxPreviewBytes ||
            pixelBytes > quint64(std::numeric_limits<int>::max()) ||
            payloadLength < 8ull + pixelBytes) {
            setError(error, QStringLiteral("invalid Blender preview payload size"));
            return {};
        }

        QByteArray pixels(qsizetype(pixelBytes), Qt::Uninitialized);
        if (!input.readExact(pixels.data(), pixels.size(), error)) {
            setError(error, QStringLiteral("truncated Blender preview pixel data"));
            return {};
        }

        QImage view(reinterpret_cast<const uchar *>(pixels.constData()),
                    int(width), int(height), int(width * 4),
                    QImage::Format_RGBA8888);
        if (view.isNull()) {
            setError(error, QStringLiteral("cannot construct Blender preview image"));
            return {};
        }

        // Blender's stored thumbnail uses bottom-up scanline order. Its
        // official thumbnailer reverses the rows when writing the image.
        QImage result = view.copy().mirrored(false, true);
        if (result.isNull())
            setError(error, QStringLiteral("cannot copy Blender preview image"));
        return result;
    }
}

} // namespace

QImage BlendReader::readPreview(const QString &path, QString *error)
{
    if (error)
        error->clear();

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(error, QStringLiteral("cannot open file: %1").arg(file.errorString()));
        return {};
    }

    const QByteArray magic = file.read(4);
    if (magic.size() != 4) {
        setError(error, QStringLiteral("truncated Blender file"));
        return {};
    }
    if (!file.seek(0)) {
        setError(error, QStringLiteral("cannot seek to beginning of Blender file"));
        return {};
    }

    const auto *p = reinterpret_cast<const uchar *>(magic.constData());
    const bool isZstd = p[0] == 0x28 && p[1] == 0xB5 && p[2] == 0x2F && p[3] == 0xFD;
    const bool isGzip = p[0] == 0x1F && p[1] == 0x8B;

    if (isZstd) {
        ZstdBlendInput input(file);
        if (!input.init(error))
            return {};
        return readPreviewFromInput(input, error);
    }

    if (isGzip) {
        GzipBlendInput input(file);
        if (!input.init(error))
            return {};
        return readPreviewFromInput(input, error);
    }

    FileBlendInput input(file);
    return readPreviewFromInput(input, error);
}
