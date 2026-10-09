#pragma once

#include <QMatrix4x4>
#include <QPointF>
#include <QRect>
#include <QSize>
#include <array>
#include <memory>
#include <optional>
#include <rhi/qrhi.h>
#include <vector>

#include "gui/quick/render/renderdevice.h"
#include "gui/quick/render/renderframe.h"
#include "gui/quick/render/tilegrid.h"

struct TileUniforms;

// Textures of one image of ImageRenderer (the image itself, or the upscaled
// crop drawn over it).
//
// The image is uploaded as premultiplied alpha with a full mip chain into one
// texture per TileGrid tile. A new image of the same size and texture format
// (the next frame of an animation) is uploaded into the existing textures
// instead of new ones.
//
// With an active SourceConversion each tile is instead uploaded with
// straight alpha into a source texture and rendered by the conversion pass
// (convert.frag: HDR decode and tone mapping, colour transform) into the
// tile's displayed texture, whose mip chain is then built. Sources of HDR
// images and of animation frames are kept, so that a new conversion (or the
// next frame) needs no new texture; other sources are released after the
// conversion, and a new conversion re-uploads them from the retained image.
//
// For drawing, the layer reduces each visible tile to its on-screen size by
// exact-area box passes (settled, below 1:1) or resamples its visible outputs
// with MKS2021 (settled, Resampling::Mks2021), and caches the result until
// the scale or the visible part changes.
//
// Render thread only.
class ImageLayer {
public:
  struct GpuTile {
    ImageTile region;
    std::unique_ptr<QRhiTexture> texture;
    // Straight-alpha source of the conversion pass; null when the image is
    // drawn unconverted and after an SDR source was converted.
    std::unique_ptr<QRhiTexture> source;
    // Large enough for TileUniforms and PanoramaUniforms.
    std::unique_ptr<QRhiBuffer> uniforms;
    // One binding set per RenderEnums::TextureSampling.
    std::array<std::unique_ptr<QRhiShaderResourceBindings>, kSamplingCount>
        bindings;
    // Same with horizontally repeating samplers (panorama); created on use.
    std::array<std::unique_ptr<QRhiShaderResourceBindings>, kSamplingCount>
        wrappingBindings;
    // Exact-ratio downsample of `texture` for reducedScale, sampled
    // bilinearly. reducedScale is set even when the build failed or needed
    // no pass (texture null), so that it is not retried every frame.
    std::optional<qreal> reducedScale;
    std::unique_ptr<QRhiTexture> reduced;
    std::unique_ptr<QRhiShaderResourceBindings> reducedBindings;
    // MKS2021 resampling of the tile's outputs resampledOutputs (output
    // grid coordinates) at resampledScale, drawn 1:1 with nearest sampling.
    // Like reducedScale, the key is set even when the build failed.
    std::optional<qreal> resampledScale;
    QRect resampledOutputs;
    std::unique_ptr<QRhiTexture> resampled;
    std::unique_ptr<QRhiShaderResourceBindings> resampledBindings;
  };

  // Where and how the layer is drawn in one frame.
  struct DrawParams {
    // Top-left corner of the image in device pixels, whole pixels.
    QPointF origin;
    // Device pixels per source pixel of this layer's image.
    qreal scale = 1.0;
    QSize targetSize;
    // Origin of the transparency checkerboard pattern in device pixels (the
    // main image's corner, so that the pattern continues under a crop).
    QPointF checkerOrigin;
    // Projection of the item's colour buffer.
    QMatrix4x4 mvp;
  };

  explicit ImageLayer(RenderDevice &device);
  ImageLayer(const ImageLayer &) = delete;
  ImageLayer &operator=(const ImageLayer &) = delete;

  // Brings the textures up to date with source: uploads a new image
  // (tileSizeLimit: TileGrid limit) and runs the conversion pass when the
  // conversion changed. Records on cb.
  void prepare(QRhiCommandBuffer *cb, const LayerSource &source,
               int tileSizeLimit);
  // Drops every texture; the next prepare() uploads again.
  void release();

  [[nodiscard]] bool isEmpty() const;
  // Size of the uploaded image.
  [[nodiscard]] QSize imageSize() const;
  [[nodiscard]] bool hasAlpha() const;
  [[nodiscard]] std::vector<GpuTile> &tiles();

  // Builds the filtered textures of the tiles visible with params, writes
  // each visible tile's TileUniforms through updates and appends the binding
  // set of its draw (image.vert/.frag pipeline) to draws.
  void prepareDraws(QRhiCommandBuffer *cb, const RenderFrame &frame,
                    const DrawParams &params,
                    QRhiResourceUpdateBatch *updates,
                    std::vector<QRhiShaderResourceBindings *> &draws);

  // Binding set of tile's mip chain with the horizontally repeating sampler
  // of sampling, created on first use; null on failure (reported).
  [[nodiscard]] QRhiShaderResourceBindings *
  wrappingBindings(GpuTile &tile, RenderEnums::TextureSampling sampling);

private:
  [[nodiscard]] bool needsUpload(const LayerSource &source,
                                 int tileSizeLimit) const;
  [[nodiscard]] bool wantsConversion(const SourceConversion &conversion) const;
  [[nodiscard]] bool needsConversion(const SourceConversion &conversion) const;
  // Uploads the image through updates, into the existing textures when they
  // fit; false on failure (reported), with the tiles released.
  [[nodiscard]] bool upload(QRhiResourceUpdateBatch *updates,
                            const LayerSource &source, int tileSizeLimit);
  [[nodiscard]] QRhiTexture *createTexture(QRhiTexture::Format format,
                                           QSize size,
                                           QRhiTexture::Flags flags);
  [[nodiscard]] bool createTile(GpuTile &tile, QRhiTexture::Format format,
                                QRhiTexture::Flags extraFlags);
  void convertTiles(QRhiCommandBuffer *cb, const SourceConversion &conversion);
  void invalidateFilteredTiles();
  void buildReducedTile(QRhiCommandBuffer *cb, GpuTile &tile, qreal scale);
  void buildResampledTile(QRhiCommandBuffer *cb, GpuTile &tile, qreal scale,
                          const QRect &outputs);

  RenderDevice &mDevice;
  std::vector<GpuTile> mTiles;

  // Generation and tile size limit of the uploaded image; empty until the
  // first upload and after release().
  std::optional<quint64> mUploadedGeneration;
  int mUploadedTileSizeLimit = 0;
  QSize mUploadedSize;
  // Texture formats of the uploaded tiles: the uploaded texels (displayed
  // texture, or conversion source) and the displayed texture.
  QRhiTexture::Format mUploadFormat = QRhiTexture::UnknownFormat;
  QRhiTexture::Format mDisplayFormat = QRhiTexture::UnknownFormat;
  bool mImageHasAlpha = false;
  // The tiles were uploaded for the conversion pass, and their sources are
  // kept after it (HDR images, animation frames).
  bool mUploadedConverted = false;
  bool mSourcesRetained = false;
  // Conversion the displayed textures hold; empty until the uploaded sources
  // were converted.
  std::optional<SourceConversion> mConvertedWith;
};
