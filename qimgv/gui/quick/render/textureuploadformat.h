#pragma once

#include <QImage>
#include <optional>
#include <rhi/qrhi.h>

// Optional texture formats of the current QRhi that the upload can use.
struct TextureFormatSupport {
  bool bgra8 = false;
  // RGBA16F textures that can also generate mipmaps.
  bool rgba16f = false;
  // RGBA32F textures (read with texelFetch() only).
  bool rgba32f = false;
};

// How an image is uploaded: the texture format and the QImage format the
// pixels must be in. Textures always hold premultiplied alpha (the renderer
// blends with ONE, ONE_MINUS_SRC_ALPHA), like FilterPixmapItem's upload.
struct TextureUploadFormat {
  QRhiTexture::Format textureFormat = QRhiTexture::RGBA8;
  QImage::Format imageFormat = QImage::Format_RGBA8888_Premultiplied;

  friend bool operator==(const TextureUploadFormat &,
                         const TextureUploadFormat &) = default;
};

// Chooses the upload format of an image in sourceFormat.
//  - 8-bit formats whose memory layout already matches a texture format
//    (RGB32, ARGB32_Premultiplied as BGRA8; RGBA8888_Premultiplied, RGBX8888
//    as RGBA8) are uploaded as they are;
//  - formats with more than 8 bits per channel are converted to
//    RGBA16FPx4_Premultiplied for an RGBA16F texture when supported;
//  - everything else is converted to RGBA8888_Premultiplied.
[[nodiscard]] TextureUploadFormat
chooseTextureUploadFormat(QImage::Format sourceFormat,
                          TextureFormatSupport support);

// Chooses the upload format of the source of the conversion pass
// (SourceConversion). Unlike the display upload it keeps straight alpha: the
// conversion works on straight colour and writes premultiplied texels.
//  - HDR images in float formats go to an RGBA16F texture (RGBA16FPx4 /
//    RGBX16FPx4 as they are, 32-bit floats converted to RGBA16FPx4): the HDR
//    tone mapper quantizes them to the same half floats on the CPU;
//  - HDR images in integer formats (16-bit PQ / HLG codes) go to an RGBA32F
//    texture (converted to RGBA32FPx4), which holds every code exactly; half
//    floats would shift dark PQ codes by several 8-bit levels after tone
//    mapping. Without RGBA32F they fall back to RGBA16F;
//  - SDR formats with more than 8 bits per channel go to RGBA16F;
//  - without RGBA16F an HDR image cannot be shown (empty result), a deep SDR
//    image falls back to 8 bits;
//  - RGB32 / ARGB32 as BGRA8, RGBA8888 / RGBX8888 as RGBA8 without
//    conversion; everything else is converted to RGBA8888.
[[nodiscard]] std::optional<TextureUploadFormat>
chooseConversionSourceFormat(QImage::Format sourceFormat, bool hdr,
                             TextureFormatSupport support);

// Format of the displayed texture the conversion pass renders into: RGBA16F
// for a float source, RGBA8 otherwise.
[[nodiscard]] QRhiTexture::Format
convertedTextureFormat(const TextureUploadFormat &source);
