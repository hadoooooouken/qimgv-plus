#pragma once

#include <QQuickRhiItemRenderer>
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
    std::unique_ptr<QRhiBuffer> uniforms;
    // One binding set per RenderEnums::TextureSampling.
    std::array<std::unique_ptr<QRhiShaderResourceBindings>, kSamplingCount>
        bindings;
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
  [[nodiscard]] bool createTile(GpuTile &tile, QRhiTexture::Format format);
  void releaseTiles();

  void reportError(const QString &message);

  QRhi *mRhi = nullptr;
  bool mDeviceResourcesReady = false;
  RenderFrame mFrame;

  // Generation and tile size limit of the uploaded image; empty until the
  // first upload and after the device resources were released.
  std::optional<quint64> mUploadedGeneration;
  int mUploadedTileSizeLimit = 0;
  bool mImageHasAlpha = false;

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

  std::shared_ptr<RenderErrorChannel> mErrorChannel;
  // Errors reported before the first synchronize() supplied the channel.
  QStringList mUnsentErrors;
  QString mLastError;
};
