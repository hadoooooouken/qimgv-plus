/*
    PNG image format plugin for qimgv-plus, decoding via libspng.

    Read-only: capabilities() only ever reports CanRead. Saving PNG files
    (e.g. from the batch converter) continues to go through the built-in
    Qt QPngHandler.
*/

#include "png_p.h"
#include "util_p.h"

#include <QColorSpace>
#include <QIODevice>
#include <QImage>
#include <QLoggingCategory>

#include <spng.h>

#include <cstring>
#include <memory>

#ifdef QT_DEBUG
Q_LOGGING_CATEGORY(LOG_PNGPLUGIN, "kf.imageformats.plugins.png", QtDebugMsg)
#else
Q_LOGGING_CATEGORY(LOG_PNGPLUGIN, "kf.imageformats.plugins.png", QtWarningMsg)
#endif

/* *** PNG_MAX_IMAGE_WIDTH and PNG_MAX_IMAGE_HEIGHT ***
 * The maximum size in pixels allowed by the plugin, enforced via
 * spng_set_image_limits() so decompression-bomb PNGs are rejected before
 * any pixel data is decoded.
 */
#ifndef PNG_MAX_IMAGE_WIDTH
#define PNG_MAX_IMAGE_WIDTH KIF_LARGE_IMAGE_PIXEL_LIMIT
#endif
#ifndef PNG_MAX_IMAGE_HEIGHT
#define PNG_MAX_IMAGE_HEIGHT PNG_MAX_IMAGE_WIDTH
#endif

namespace // Private
{

constexpr uchar kPngSignature[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
constexpr qint64 kPngSignatureSize = qint64(sizeof(kPngSignature));

// Signature (8) + one complete IHDR chunk (4 length + 4 "IHDR" + 13 data +
// 4 CRC = 25) = 33 bytes. That is all libspng needs to populate spng_ihdr,
// so Size/ImageFormat queries can be answered from a non-destructive
// device->peek() without decoding any pixel data.
constexpr qint64 kPngIhdrPeekSize = kPngSignatureSize + 25;

struct SpngCtxDeleter {
    void operator()(spng_ctx *ctx) const noexcept
    {
        if (ctx) {
            spng_ctx_free(ctx);
        }
    }
};
using SpngCtxPtr = std::unique_ptr<spng_ctx, SpngCtxDeleter>;

bool matchesSignature(const QByteArray &head)
{
    return head.size() >= kPngSignatureSize && std::memcmp(head.constData(), kPngSignature, size_t(kPngSignatureSize)) == 0;
}

// spng read callback bridging into a QIODevice for streaming decode.
//
// QIODevice::read() is not guaranteed to fill the buffer in a single call
// even when there is no real end-of-stream (this is especially likely for
// large chunk reads, e.g. big IDAT chunks). Treating any short read as EOF
// truncates decoding of otherwise-valid files, so we loop until either the
// full length has been read or we hit a genuine end/error.
int qiodeviceReadFn(spng_ctx *, void *user, void *dest, size_t length)
{
    auto device = reinterpret_cast<QIODevice *>(user);
    auto *buf = reinterpret_cast<char *>(dest);
    size_t total = 0;

    while (total < length) {
        qint64 n = device->read(buf + total, qint64(length - total));
        if (n < 0) {
            return SPNG_IO_ERROR;
        }
        if (n == 0) {
            if (device->atEnd()) {
                return SPNG_IO_EOF;
            }
            if (!device->waitForReadyRead(-1)) {
                return SPNG_IO_EOF;
            }
            continue;
        }
        total += size_t(n);
    }
    return 0;
}

// Reads just the IHDR chunk (via peek, without moving the device's read
// cursor) to answer Size/ImageFormat queries cheaply.
bool peekIhdr(QIODevice *device, spng_ihdr &ihdr)
{
    if (!device) {
        return false;
    }

    auto head = device->peek(kPngIhdrPeekSize);
    if (head.size() < kPngIhdrPeekSize || !matchesSignature(head)) {
        return false;
    }

    SpngCtxPtr ctx(spng_ctx_new(0));
    if (!ctx) {
        return false;
    }

    if (spng_set_png_buffer(ctx.get(), head.constData(), size_t(head.size())) != 0) {
        return false;
    }

    return spng_get_ihdr(ctx.get(), &ihdr) == 0;
}

QImage::Format targetImageFormat()
{
    // Decision: always decode to 8-bit RGBA for v1, matching the QOI plugin.
    return QImage::Format_RGBA8888;
}

} // namespace

class SpngHandlerPrivate
{
public:
    spng_ihdr m_ihdr{};
    bool m_ihdrValid = false;
};

SpngHandler::SpngHandler()
    : QImageIOHandler()
    , d(new SpngHandlerPrivate)
{
}

bool SpngHandler::canRead() const
{
    if (canRead(device())) {
        setFormat("png");
        return true;
    }
    return false;
}

bool SpngHandler::canRead(QIODevice *device)
{
    if (!device) {
        qCWarning(LOG_PNGPLUGIN) << "SpngHandler::canRead() called with no device";
        return false;
    }

    return matchesSignature(device->peek(kPngSignatureSize));
}

bool SpngHandler::read(QImage *image)
{
    auto dev = device();
    if (!dev) {
        return false;
    }

    SpngCtxPtr ctx(spng_ctx_new(0));
    if (!ctx) {
        qCWarning(LOG_PNGPLUGIN) << "spng_ctx_new failed";
        return false;
    }

    if (int ret = spng_set_image_limits(ctx.get(), PNG_MAX_IMAGE_WIDTH, PNG_MAX_IMAGE_HEIGHT); ret != 0) {
        qCWarning(LOG_PNGPLUGIN) << "spng_set_image_limits failed:" << spng_strerror(ret);
        return false;
    }
    if (int ret = spng_set_png_stream(ctx.get(), qiodeviceReadFn, dev); ret != 0) {
        qCWarning(LOG_PNGPLUGIN) << "spng_set_png_stream failed:" << spng_strerror(ret);
        return false;
    }

    spng_ihdr ihdr{};
    if (int ret = spng_get_ihdr(ctx.get(), &ihdr); ret != 0) {
        qCWarning(LOG_PNGPLUGIN) << "spng_get_ihdr failed:" << spng_strerror(ret);
        return false;
    }

    // Cache for any option() call that might follow (e.g. a re-query of
    // ImageFormat after read()).
    d->m_ihdr = ihdr;
    d->m_ihdrValid = true;

    size_t outSize = 0;
    if (int ret = spng_decoded_image_size(ctx.get(), SPNG_FMT_RGBA8, &outSize); ret != 0) {
        qCWarning(LOG_PNGPLUGIN) << "spng_decoded_image_size failed:" << spng_strerror(ret);
        return false;
    }

    QImage img = imageAlloc(ihdr.width, ihdr.height, targetImageFormat());
    if (img.isNull() || size_t(img.sizeInBytes()) < outSize) {
        qCWarning(LOG_PNGPLUGIN) << "image allocation failed or too small for decoded size" << outSize;
        return false;
    }

    // SPNG_DECODE_TRNS is required whenever the source color type has no
    // native alpha channel (grayscale/indexed, e.g. 1-bit optimized icons)
    // but conveys transparency via a tRNS chunk instead. Without this flag
    // libspng leaves such pixels fully opaque when converting to RGBA8.
    if (int ret = spng_decode_image(ctx.get(), img.bits(), outSize, SPNG_FMT_RGBA8, SPNG_DECODE_TRNS); ret != 0) {
        qCWarning(LOG_PNGPLUGIN) << "spng_decode_image failed:" << spng_strerror(ret);
        return false;
    }

    img.setColorSpace(QColorSpace(QColorSpace::SRgb));

    *image = img;
    return true;
}

bool SpngHandler::supportsOption(ImageOption option) const
{
    return option == QImageIOHandler::Size || option == QImageIOHandler::ImageFormat;
}

QVariant SpngHandler::option(ImageOption option) const
{
    QVariant v;

    if (option != QImageIOHandler::Size && option != QImageIOHandler::ImageFormat) {
        return v;
    }

    bool haveIhdr = d->m_ihdrValid;
    spng_ihdr ihdr = d->m_ihdr;

    if (!haveIhdr && peekIhdr(device(), ihdr)) {
        d->m_ihdr = ihdr;
        d->m_ihdrValid = true;
        haveIhdr = true;
    }

    if (!haveIhdr) {
        return v;
    }

    if (option == QImageIOHandler::Size) {
        v = QVariant::fromValue(QSize(int(ihdr.width), int(ihdr.height)));
    } else if (option == QImageIOHandler::ImageFormat) {
        v = QVariant::fromValue(targetImageFormat());
    }

    return v;
}

QImageIOPlugin::Capabilities SpngPlugin::capabilities(QIODevice *device, const QByteArray &format) const
{
    if (format == "png" || format == "PNG") {
        return Capabilities(CanRead);
    }
    if (!format.isEmpty()) {
        return {};
    }
    if (!device || !device->isOpen() || !device->isReadable()) {
        return {};
    }

    Capabilities cap;
    if (SpngHandler::canRead(device)) {
        cap |= CanRead;
    }
    return cap;
}

QImageIOHandler *SpngPlugin::create(QIODevice *device, const QByteArray &format) const
{
    QImageIOHandler *handler = new SpngHandler;
    handler->setDevice(device);
    handler->setFormat(format);
    return handler;
}

#include "moc_png_p.cpp"
