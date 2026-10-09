#pragma once

#include <QQuickRhiItemRenderer>
#include <QRect>
#include <QStringList>
#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <rhi/qrhi.h>
#include <rhi/qshader.h>
#include <vector>

#include "gui/quick/render/renderframe.h"
#include "gui/quick/render/rendererrorchannel.h"
#include "gui/quick/render/tilegrid.h"

// Render-thread half of ImageRenderItem.
//
// Draws the RenderFrame taken in synchronize() into the item's colour buffer
// in one render pass: a clear to the background colour, then one textured
// quad per image tile (TileGrid), clipped to the item. The image is uploaded
// as premultiplied alpha with a full mip chain whenever the item's image
// generation or the effective tile size limit changes.
//
// With an active SourceConversion (HDR images, or a display colour space
// that differs from the image's), each tile is instead uploaded with straight
// alpha into a source texture and rendered by the conversion pass
// (convert.frag: HDR decode and tone mapping, colour transform) into the
// tile's displayed texture, whose mip chain is then built; everything after
// that sees display-encoded, premultiplied texels as before. HDR sources are
// kept so that a new tone mapping or colour transform only re-runs the
// conversion; SDR sources are released after the conversion, and a new
// transform re-uploads them from the retained image.
//
// The tile shader applies the ImageFilter of the frame: CAS or smart
// sharpening and the colour adjustment matrix (ports of the widget viewer's
// filter.frag). While the frame is settled and the image is shown below 1:1,
// each visible tile is first reduced to its on-screen size by a chain of
// exact-area box passes into intermediate render targets (boxreduce.frag);
// the result is drawn instead of the mip chain and reused until the scale or
// the image changes. On Direct3D 12 the same box passes also build the mip
// chain (see generatesMipsByBoxReduce()).
//
// With RenderEnums::Resampling::Mks2021 selected, a settled frame at any
// scale other than 1:1 instead resamples the visible part of every tile with
// the Magic Kernel Sharp 2021 kernel (resample.frag, a horizontal and a
// vertical pass, see ResampleGrid) and draws it 1:1; the result is reused
// until the scale or the visible part changes.
//
// Never touches Core, Settings or GUI-thread objects outside synchronize().
// Failures are posted to the item's RenderErrorChannel; every failing step
// leaves the renderer in a state that still clears to the background.
class ImageRenderer : public QQuickRhiItemRenderer {
public:
  ImageRenderer();
  ~ImageRenderer() override;
  ImageRenderer(const ImageRenderer &) = delete;
  ImageRenderer &operator=(const ImageRenderer &) = delete;

protected:
  void initialize(QRhiCommandBuffer *cb) override;
  void synchronize(QQuickRhiItem *item) override;
  void render(QRhiCommandBuffer *cb) override;

private:
  static constexpr std::size_t kSamplingCount = 3;

  struct GpuTile {
    ImageTile region;
    std::unique_ptr<QRhiTexture> texture;
    // Straight-alpha source of the conversion pass; null when the image is
    // drawn unconverted and after an SDR source was converted.
    std::unique_ptr<QRhiTexture> source;
    std::unique_ptr<QRhiBuffer> uniforms;
    // One binding set per RenderEnums::TextureSampling.
    std::array<std::unique_ptr<QRhiShaderResourceBindings>, kSamplingCount>
        bindings;
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

  // Releases a QRhi resource once the frame being recorded no longer uses
  // it (QRhiResource::deleteLater()).
  struct DeferredRhiRelease {
    void operator()(QRhiResource *resource) const { resource->deleteLater(); }
  };
  template <typename T>
  using FrameResource = std::unique_ptr<T, DeferredRhiRelease>;
  using FrameTexture = FrameResource<QRhiTexture>;

  // Offscreen passes recorded before the main pass.
  enum class PassKind { BoxReduce, Resample, Convert };

  // Pipeline of one kind of offscreen pass for one render target format.
  struct PassPipeline {
    PassKind kind = PassKind::BoxReduce;
    QRhiTexture::Format format = QRhiTexture::RGBA8;
    std::unique_ptr<QRhiRenderPassDescriptor> renderPass;
    // Null when its creation failed; not retried for this QRhi.
    std::unique_ptr<QRhiGraphicsPipeline> pipeline;
  };

  // Source of one tile draw in the main pass.
  enum class DrawSource { MipChain, Reduced, Resampled };

  // One draw of the main pass.
  struct TileDraw {
    const GpuTile *tile = nullptr;
    DrawSource source = DrawSource::MipChain;
  };

  // Resources that live as long as the QRhi: shaders, the quad vertex
  // buffer, the samplers and the layout bindings of the pipeline.
  [[nodiscard]] bool createDeviceResources(QRhiCommandBuffer *cb);
  [[nodiscard]] bool loadShader(const QString &path, QShader &shader);
  // (Re)creates the pipeline when the render target's pass is not
  // compatible with the one it was created for.
  [[nodiscard]] bool ensurePipeline();
  void releaseDeviceResources();

  [[nodiscard]] int effectiveTileSizeLimit() const;
  [[nodiscard]] bool needsUpload() const;
  void uploadImage(QRhiResourceUpdateBatch *updates);
  // True when the mip chain is rendered with box-reduce passes instead of
  // QRhiResourceUpdateBatch::generateMips().
  [[nodiscard]] bool generatesMipsByBoxReduce() const;
  [[nodiscard]] QRhiTexture::Flags tileTextureFlags() const;
  [[nodiscard]] bool createTile(GpuTile &tile, QRhiTexture::Format format,
                                QRhiTexture::Flags extraFlags = {});
  void releaseTiles();
  // Drops every tile's exact downsample and resampling results.
  void invalidateFilteredTiles();

  // The frame's conversion is active and the conversion pass is available.
  [[nodiscard]] bool wantsConversion() const;
  // The uploaded sources must be (re)converted for the frame's conversion.
  [[nodiscard]] bool needsConversion() const;
  // Records the conversion pass of every tile and the mip chains of the
  // results on cb.
  void convertTiles(QRhiCommandBuffer *cb);
  // The 3D lookup table of the frame's conversion, uploaded on first use;
  // the layout placeholder when the conversion uses none; null on failure
  // (reported).
  [[nodiscard]] QRhiTexture *conversionLut(QRhiCommandBuffer *cb);

  // Resources of the exact-ratio downsample; optional: on failure the
  // renderer keeps drawing from the mip chain.
  [[nodiscard]] bool createReduceResources();
  // Resources of the MKS2021 resampling; optional like the exact
  // downsample, and built on its sampler.
  [[nodiscard]] bool createResampleResources();
  // Resources of the conversion pass, built on the exact downsample's
  // sampler; without them images are drawn unconverted (reported).
  [[nodiscard]] bool createConvertResources();
  [[nodiscard]] QRhiGraphicsPipeline *
  passPipeline(PassKind kind, QRhiTexture::Format format,
               QRhiRenderPassDescriptor *compatiblePass);
  // One offscreen pass: reads source (and, for resampling passes, the
  // weight table weights; for conversion passes, the colour lookup table
  // colorLut) and renders into a new single-level texture of the source's
  // format and of size `size`; uniforms (uniformSize bytes) fill the pass's
  // uniform buffer.
  struct OffscreenPass {
    PassKind kind = PassKind::BoxReduce;
    QRhiTexture *source = nullptr;
    QRhiTexture *weights = nullptr;
    QRhiTexture *colorLut = nullptr;
    QSize size;
    const void *uniforms = nullptr;
    quint32 uniformSize = 0;
    QRhiTexture::Flags extraFlags;
  };
  // Records pass on cb into a new texture; null on failure (reported).
  [[nodiscard]] FrameTexture recordOffscreenPass(QRhiCommandBuffer *cb,
                                                 const OffscreenPass &pass);
  // Records pass on cb into level 0 of target (pass.size and
  // pass.extraFlags are not used); false on failure (reported).
  [[nodiscard]] bool recordPassInto(QRhiCommandBuffer *cb,
                                    const OffscreenPass &pass,
                                    QRhiTexture *target);
  // Uploads a resampling weight table (size.width() floats per row) into a
  // new R32F texture, recorded on cb; null on failure (reported).
  [[nodiscard]] FrameTexture uploadWeightTable(QRhiCommandBuffer *cb,
                                               QSize size,
                                               const std::vector<float> &values);
  // Records one exact-area pass from level 0 of source into a new texture;
  // null on failure (reported).
  [[nodiscard]] FrameTexture recordBoxPass(QRhiCommandBuffer *cb,
                                           QRhiTexture *source,
                                           QSize sourceSize, QSize size,
                                           QRhiTexture::Flags extraFlags = {});
  // Records the passes that reduce tile.texture to scale on cb.
  void buildReducedTile(QRhiCommandBuffer *cb, GpuTile &tile, qreal scale);
  // Records the two passes that resample outputs (output grid coordinates)
  // of an image shown at scale from tile.texture on cb.
  void buildResampledTile(QRhiCommandBuffer *cb, GpuTile &tile, qreal scale,
                          const QRect &outputs);
  // Records the passes of mip levels 1..n of tile.texture on cb and their
  // copies into the texture on copies, which must be submitted before the
  // texture is sampled.
  void generateTileMips(QRhiCommandBuffer *cb, GpuTile &tile,
                        QRhiResourceUpdateBatch *copies);

  void reportError(const QString &message);

  QRhi *mRhi = nullptr;
  bool mDeviceResourcesReady = false;
  RenderFrame mFrame;

  // Generation and tile size limit of the uploaded image; empty until the
  // first upload and after the device resources were released.
  std::optional<quint64> mUploadedGeneration;
  int mUploadedTileSizeLimit = 0;
  bool mImageHasAlpha = false;
  // The tiles were uploaded for the conversion pass, and their sources are
  // kept after it (HDR images).
  bool mUploadedConverted = false;
  bool mSourcesRetained = false;
  // Conversion the displayed textures hold; empty until the uploaded sources
  // were converted.
  std::optional<SourceConversion> mConvertedWith;

  QShader mVertexShader;
  QShader mFragmentShader;
  std::unique_ptr<QRhiBuffer> mVertexBuffer;
  std::array<std::unique_ptr<QRhiSampler>, kSamplingCount> mSamplers;
  // Placeholder resources that describe the binding layout of the pipeline.
  std::unique_ptr<QRhiTexture> mLayoutTexture;
  std::unique_ptr<QRhiBuffer> mLayoutUniforms;
  std::unique_ptr<QRhiShaderResourceBindings> mLayoutBindings;
  std::unique_ptr<QRhiRenderPassDescriptor> mPipelineRenderPass;
  int mPipelineSampleCount = 0;
  std::unique_ptr<QRhiGraphicsPipeline> mPipeline;

  std::vector<GpuTile> mTiles;

  bool mReduceReady = false;
  QShader mReduceVertexShader;
  QShader mReduceFragmentShader;
  std::unique_ptr<QRhiSampler> mReduceSampler;
  std::unique_ptr<QRhiBuffer> mReduceLayoutUniforms;
  std::unique_ptr<QRhiShaderResourceBindings> mReduceLayoutBindings;

  bool mResampleReady = false;
  QShader mResampleVertexShader;
  QShader mResampleFragmentShader;
  std::unique_ptr<QRhiBuffer> mResampleLayoutUniforms;
  std::unique_ptr<QRhiShaderResourceBindings> mResampleLayoutBindings;

  bool mConvertReady = false;
  QShader mConvertVertexShader;
  QShader mConvertFragmentShader;
  std::unique_ptr<QRhiSampler> mLutSampler;
  // 1 x 1 x 1 placeholder bound when the conversion uses no lookup table.
  std::unique_ptr<QRhiTexture> mLayoutLut;
  std::unique_ptr<QRhiBuffer> mConvertLayoutUniforms;
  std::unique_ptr<QRhiShaderResourceBindings> mConvertLayoutBindings;
  // Uploaded lookup table and the data it was uploaded from.
  std::unique_ptr<QRhiTexture> mLutTexture;
  std::shared_ptr<const ColorLut> mLutData;

  std::vector<PassPipeline> mPassPipelines;

  std::shared_ptr<RenderErrorChannel> mErrorChannel;
  // Errors reported before the first synchronize() supplied the channel.
  QStringList mUnsentErrors;
  QString mLastError;
};
