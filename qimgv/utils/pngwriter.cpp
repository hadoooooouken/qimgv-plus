#include "pngwriter.h"
#include <QLibrary>
#include <QFile>
#include <QtEndian>
#include <cstring>
#include <QCoreApplication>
#include <mutex>

namespace {

// Opaque struct for compressor pointer
struct libdeflate_compressor;

// Function pointer signatures
typedef libdeflate_compressor* (*Fn_alloc_compressor)(int);
typedef size_t (*Fn_zlib_compress_bound)(libdeflate_compressor*, size_t);
typedef size_t (*Fn_zlib_compress)(libdeflate_compressor*, const void*, size_t, void*, size_t);
typedef void (*Fn_free_compressor)(libdeflate_compressor*);
typedef uint32_t (*Fn_crc32)(uint32_t, const void*, size_t);

class LibDeflateLoader {
public:
    static LibDeflateLoader& instance() {
        static LibDeflateLoader loader;
        return loader;
    }

    bool isAvailable() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_initialized) {
            return m_loaded;
        }

        m_initialized = true;

        // Try looking in the application directory first
        QString appDirPath = QCoreApplication::applicationDirPath() + "/deflate.dll";
        m_library.setFileName(appDirPath);
        if (!m_library.load()) {
            m_library.setFileName("deflate");
            if (!m_library.load()) {
                return false;
            }
        }

        m_alloc = reinterpret_cast<Fn_alloc_compressor>(m_library.resolve("libdeflate_alloc_compressor"));
        m_bound = reinterpret_cast<Fn_zlib_compress_bound>(m_library.resolve("libdeflate_zlib_compress_bound"));
        m_compress = reinterpret_cast<Fn_zlib_compress>(m_library.resolve("libdeflate_zlib_compress"));
        m_free = reinterpret_cast<Fn_free_compressor>(m_library.resolve("libdeflate_free_compressor"));
        m_crc32 = reinterpret_cast<Fn_crc32>(m_library.resolve("libdeflate_crc32"));

        m_loaded = (m_alloc && m_bound && m_compress && m_free && m_crc32);
        return m_loaded;
    }

    libdeflate_compressor* alloc_compressor(int compression_level) {
        return m_alloc ? m_alloc(compression_level) : nullptr;
    }

    size_t zlib_compress_bound(libdeflate_compressor* compressor, size_t in_nbytes) {
        return m_bound ? m_bound(compressor, in_nbytes) : 0;
    }

    size_t zlib_compress(libdeflate_compressor* compressor, const void* in, size_t in_nbytes, void* out, size_t out_nbytes_avail) {
        return m_compress ? m_compress(compressor, in, in_nbytes, out, out_nbytes_avail) : 0;
    }

    void free_compressor(libdeflate_compressor* compressor) {
        if (m_free && compressor) {
            m_free(compressor);
        }
    }

    uint32_t crc32(uint32_t crc, const void* buffer, size_t len) {
        return m_crc32 ? m_crc32(crc, buffer, len) : 0;
    }

private:
    LibDeflateLoader() = default;
    ~LibDeflateLoader() {
        if (m_library.isLoaded()) {
            m_library.unload();
        }
    }

    std::mutex m_mutex;
    QLibrary m_library;
    bool m_initialized = false;
    bool m_loaded = false;

    Fn_alloc_compressor m_alloc = nullptr;
    Fn_zlib_compress_bound m_bound = nullptr;
    Fn_zlib_compress m_compress = nullptr;
    Fn_free_compressor m_free = nullptr;
    Fn_crc32 m_crc32 = nullptr;
};

#pragma pack(push, 1)
struct IhdrChunk {
    uint32_t width;
    uint32_t height;
    uint8_t bitDepth;
    uint8_t colorType;
    uint8_t compressionMethod;
    uint8_t filterMethod;
    uint8_t interlaceMethod;
};
#pragma pack(pop)

void writeChunk(QFile &file, const char type[4], const void *data, uint32_t size) {
    auto &loader = LibDeflateLoader::instance();

    // 1. Chunk Length (4 bytes, big endian)
    uint32_t lenBE = qToBigEndian<uint32_t>(size);
    file.write(reinterpret_cast<const char*>(&lenBE), 4);

    // 2. Chunk Type (4 bytes)
    file.write(type, 4);

    // 3. Chunk Data (len bytes)
    if (size > 0 && data) {
        file.write(reinterpret_cast<const char*>(data), size);
    }

    // 4. CRC-32 (4 bytes, big endian) of Type + Data
    uint32_t crc = loader.crc32(0, type, 4);
    if (size > 0 && data) {
        crc = loader.crc32(crc, data, size);
    }
    uint32_t crcBE = qToBigEndian<uint32_t>(crc);
    file.write(reinterpret_cast<const char*>(&crcBE), 4);
}

} // namespace

bool savePngWithLibdeflate(const QImage &image, const QString &filePath, int compressionLevel) {
    auto &loader = LibDeflateLoader::instance();
    if (!loader.isAvailable()) {
        return false;
    }

    if (image.isNull()) {
        return false;
    }

    QImage src = image;
    bool hasAlpha = src.hasAlphaChannel();
    if (hasAlpha) {
        if (src.format() != QImage::Format_RGBA8888) {
            src = src.convertToFormat(QImage::Format_RGBA8888);
        }
    } else {
        if (src.format() != QImage::Format_RGB888) {
            src = src.convertToFormat(QImage::Format_RGB888);
        }
    }

    if (src.isNull()) {
        return false;
    }

    int width = src.width();
    int height = src.height();
    int channels = hasAlpha ? 4 : 3;
    int rowBytes = width * channels;

    // Prepare uncompressed scanline data: each scanline starts with a filter byte (0)
    size_t uncompressedSize = static_cast<size_t>(height) * (1 + rowBytes);
    QByteArray uncompressedData;
    uncompressedData.resize(uncompressedSize);
    char *dest = uncompressedData.data();

    for (int y = 0; y < height; ++y) {
        *dest++ = 0; // Filter byte 0 (None)
        memcpy(dest, src.constScanLine(y), rowBytes);
        dest += rowBytes;
    }

    // Allocate compressor
    libdeflate_compressor *compressor = loader.alloc_compressor(compressionLevel);
    if (!compressor) {
        return false;
    }

    size_t maxCompressedSize = loader.zlib_compress_bound(compressor, uncompressedSize);
    QByteArray compressedData;
    compressedData.resize(maxCompressedSize);

    size_t actualSize = loader.zlib_compress(
        compressor,
        uncompressedData.constData(), uncompressedSize,
        compressedData.data(), maxCompressedSize
    );

    loader.free_compressor(compressor);

    if (actualSize == 0) {
        return false;
    }
    compressedData.resize(actualSize);

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    // Write PNG signature
    const char signature[8] = {
        static_cast<char>(0x89),
        'P', 'N', 'G',
        '\r', '\n',
        0x1a, '\n'
    };
    if (file.write(signature, 8) != 8) {
        return false;
    }

    // Write IHDR Chunk
    IhdrChunk ihdr;
    ihdr.width = qToBigEndian<uint32_t>(width);
    ihdr.height = qToBigEndian<uint32_t>(height);
    ihdr.bitDepth = 8;
    ihdr.colorType = hasAlpha ? 6 : 2; // 6 = RGBA, 2 = RGB
    ihdr.compressionMethod = 0;
    ihdr.filterMethod = 0;
    ihdr.interlaceMethod = 0;

    writeChunk(file, "IHDR", &ihdr, sizeof(ihdr));

    // Write IDAT Chunk
    writeChunk(file, "IDAT", compressedData.constData(), compressedData.size());

    // Write IEND Chunk
    writeChunk(file, "IEND", nullptr, 0);

    return true;
}
