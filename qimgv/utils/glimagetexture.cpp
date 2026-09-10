#include "glimagetexture.h"

#include <QOpenGLPixelTransferOptions>

namespace GlImageTexture {

namespace {

// Bytes per pixel for the FP16 formats this module handles: 4 channels
// (R, G, B, A) x 2 bytes (qfloat16) each, tightly packed - the same layout
// utils/imagelib.cpp's fp16 scanline helpers and HdrToneMapper's FP16 write
// path both already assume, and the layout a standalone validation harness
// confirmed (against a real Qt 6 / Mesa build) never has QImage row padding
// beyond width * kBytesPerFp16Pixel, for either even or odd image widths.
constexpr int kBytesPerFp16Pixel = 8;

QOpenGLPixelTransferOptions fp16TransferOptionsFor(const QImage &image) {
    QOpenGLPixelTransferOptions options;
    // The row length is set explicitly rather than relied upon as a
    // defensive, essentially free safeguard against any future QImage
    // internal change - not a workaround for an observed bug (none was
    // observed; see the class-level comment above).
    options.setRowLength(static_cast<int>(image.bytesPerLine()) / kBytesPerFp16Pixel);
    options.setAlignment(1);
    return options;
}

void allocateAndUploadFp16(QOpenGLTexture &texture, const QImage &image, bool generateMips) {
    texture.setFormat(QOpenGLTexture::RGBA16F);
    texture.setSize(image.width(), image.height());
    texture.setMipLevels(generateMips ? texture.maximumMipLevels() : 1);

    // The explicit (PixelFormat, PixelType) overload is used for both
    // allocateStorage() and setData() - rather than the no-arg
    // allocateStorage() overload - because Qt's own internally-inferred
    // pixel type for some texture formats does not match the type setData()
    // is given here; passing the same explicit pair to both calls removes
    // that mismatch risk entirely instead of relying on Qt's default.
    texture.allocateStorage(QOpenGLTexture::RGBA, QOpenGLTexture::Float16);

    const QOpenGLPixelTransferOptions options = fp16TransferOptionsFor(image);
    texture.setData(QOpenGLTexture::RGBA, QOpenGLTexture::Float16,
                     image.constBits(), &options);

    if (generateMips) {
        texture.generateMipMaps();
    }
}

} // namespace

bool isFp16Format(QImage::Format format) {
    switch (format) {
    case QImage::Format_RGBX16FPx4:
    case QImage::Format_RGBA16FPx4:
    case QImage::Format_RGBA16FPx4_Premultiplied:
        return true;
    default:
        return false;
    }
}

bool isCompatible(const QOpenGLTexture *existing, const QImage &image, bool needMips) {
    if (!existing) {
        return false;
    }
    if (existing->width() != image.width() || existing->height() != image.height()) {
        return false;
    }
    if (needMips && existing->mipLevels() <= 1) {
        return false;
    }
    const bool existingIsFp16 = (existing->format() == QOpenGLTexture::RGBA16F);
    if (existingIsFp16 != isFp16Format(image.format())) {
        return false;
    }
    return true;
}

std::unique_ptr<QOpenGLTexture> create(const QImage &image, bool generateMips) {
    Q_ASSERT(!image.isNull());

    if (isFp16Format(image.format())) {
        auto texture = std::make_unique<QOpenGLTexture>(QOpenGLTexture::Target2D);
        allocateAndUploadFp16(*texture, image, generateMips);
        return texture;
    }

    // Ordinary SDR path: byte-for-byte unchanged from the pre-existing
    // behavior this replaces - Qt's convenience constructor converts to
    // Format_RGBA8888 / RGBA8_UNorm internally regardless of the input
    // QImage::Format, exactly as it did before this helper existed.
    return std::make_unique<QOpenGLTexture>(
        image, generateMips ? QOpenGLTexture::GenerateMipMaps : QOpenGLTexture::DontGenerateMipMaps);
}

void update(QOpenGLTexture *existing, const QImage &image, bool generateMips) {
    Q_ASSERT(existing);
    Q_ASSERT(!image.isNull());

    if (isFp16Format(image.format())) {
        const QOpenGLPixelTransferOptions options = fp16TransferOptionsFor(image);
        existing->setData(QOpenGLTexture::RGBA, QOpenGLTexture::Float16,
                           image.constBits(), &options);
        if (generateMips) {
            existing->generateMipMaps();
        }
        return;
    }

    existing->setData(image, generateMips ? QOpenGLTexture::GenerateMipMaps : QOpenGLTexture::DontGenerateMipMaps);
}

} // namespace GlImageTexture
