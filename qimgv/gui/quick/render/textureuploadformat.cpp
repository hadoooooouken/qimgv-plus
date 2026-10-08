#include "textureuploadformat.h"

namespace {
constexpr TextureUploadFormat kRgba8Converted{
    QRhiTexture::RGBA8, QImage::Format_RGBA8888_Premultiplied};
constexpr TextureUploadFormat kRgba16fConverted{
    QRhiTexture::RGBA16F, QImage::Format_RGBA16FPx4_Premultiplied};

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
