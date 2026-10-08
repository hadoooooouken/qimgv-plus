#pragma once

#include <QImage>
#include <rhi/qrhi.h>

// Optional texture formats of the current QRhi that the upload can use.
struct TextureFormatSupport {
  bool bgra8 = false;
  // RGBA16F textures that can also generate mipmaps.
  bool rgba16f = false;
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
