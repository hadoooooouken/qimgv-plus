#include "textureuploadformat.h"

namespace {
constexpr TextureUploadFormat kRgba8Converted{
    QRhiTexture::RGBA8, QImage::Format_RGBA8888_Premultiplied};
constexpr TextureUploadFormat kRgba16fConverted{
    QRhiTexture::RGBA16F, QImage::Format_RGBA16FPx4_Premultiplied};
constexpr TextureUploadFormat kStraightRgba8Converted{
    QRhiTexture::RGBA8, QImage::Format_RGBA8888};
constexpr TextureUploadFormat kStraightRgba16fConverted{
    QRhiTexture::RGBA16F, QImage::Format_RGBA16FPx4};
constexpr TextureUploadFormat kStraightRgba32fConverted{
    QRhiTexture::RGBA32F, QImage::Format_RGBA32FPx4};

bool isFloatFormat(QImage::Format format) {
  switch (format) {
  case QImage::Format_RGBX16FPx4:
  case QImage::Format_RGBA16FPx4:
  case QImage::Format_RGBA16FPx4_Premultiplied:
  case QImage::Format_RGBX32FPx4:
  case QImage::Format_RGBA32FPx4:
  case QImage::Format_RGBA32FPx4_Premultiplied:
    return true;
  default:
    return false;
  }
}

bool hasHighPrecision(QImage::Format format) {
  switch (format) {
  case QImage::Format_BGR30:
  case QImage::Format_A2BGR30_Premultiplied:
  case QImage::Format_RGB30:
  case QImage::Format_A2RGB30_Premultiplied:
  case QImage::Format_RGBX64:
  case QImage::Format_RGBA64:
  case QImage::Format_RGBA64_Premultiplied:
  case QImage::Format_Grayscale16:
  case QImage::Format_RGBX16FPx4:
  case QImage::Format_RGBA16FPx4:
  case QImage::Format_RGBA16FPx4_Premultiplied:
  case QImage::Format_RGBX32FPx4:
  case QImage::Format_RGBA32FPx4:
  case QImage::Format_RGBA32FPx4_Premultiplied:
    return true;
  default:
    return false;
  }
}
} // namespace

TextureUploadFormat chooseTextureUploadFormat(QImage::Format sourceFormat,
                                              TextureFormatSupport support) {
  switch (sourceFormat) {
  // 0xffRRGGBB / premultiplied 0xAARRGGBB words: B, G, R, A bytes in memory.
  case QImage::Format_RGB32:
  case QImage::Format_ARGB32_Premultiplied:
    return support.bgra8
               ? TextureUploadFormat{QRhiTexture::BGRA8, sourceFormat}
               : kRgba8Converted;
  // R, G, B, A bytes; RGBX8888 stores 255 in the unused byte.
  case QImage::Format_RGBA8888_Premultiplied:
  case QImage::Format_RGBX8888:
    return TextureUploadFormat{QRhiTexture::RGBA8, sourceFormat};
  // Alpha is 1.0 in the unused channel of RGBX16FPx4.
  case QImage::Format_RGBA16FPx4_Premultiplied:
  case QImage::Format_RGBX16FPx4:
    return support.rgba16f
               ? TextureUploadFormat{QRhiTexture::RGBA16F, sourceFormat}
               : kRgba8Converted;
  default:
    break;
  }
  if (hasHighPrecision(sourceFormat) && support.rgba16f)
    return kRgba16fConverted;
  return kRgba8Converted;
}

std::optional<TextureUploadFormat>
chooseConversionSourceFormat(QImage::Format sourceFormat, bool hdr,
                             TextureFormatSupport support) {
  if (hdr && !isFloatFormat(sourceFormat) && support.rgba32f)
    return kStraightRgba32fConverted;
  if (hdr || hasHighPrecision(sourceFormat)) {
    if (support.rgba16f) {
      // Alpha is 1.0 in the unused channel of RGBX16FPx4.
      if (sourceFormat == QImage::Format_RGBA16FPx4 ||
          sourceFormat == QImage::Format_RGBX16FPx4)
        return TextureUploadFormat{QRhiTexture::RGBA16F, sourceFormat};
      return kStraightRgba16fConverted;
    }
    if (hdr)
      return std::nullopt;
    return kStraightRgba8Converted;
  }
  switch (sourceFormat) {
  // 0xffRRGGBB / straight 0xAARRGGBB words: B, G, R, A bytes in memory.
  case QImage::Format_RGB32:
  case QImage::Format_ARGB32:
    return support.bgra8
               ? TextureUploadFormat{QRhiTexture::BGRA8, sourceFormat}
               : kStraightRgba8Converted;
  case QImage::Format_RGBA8888:
  case QImage::Format_RGBX8888:
    return TextureUploadFormat{QRhiTexture::RGBA8, sourceFormat};
  default:
    return kStraightRgba8Converted;
  }
}

QRhiTexture::Format convertedTextureFormat(const TextureUploadFormat &source) {
  return source.textureFormat == QRhiTexture::RGBA16F ||
                 source.textureFormat == QRhiTexture::RGBA32F
             ? QRhiTexture::RGBA16F
             : QRhiTexture::RGBA8;
}
