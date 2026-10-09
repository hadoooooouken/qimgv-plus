#pragma once

#include <QQuickRhiItemRenderer>
#include <memory>
#include <rhi/qrhi.h>

#include "gui/quick/render/imagelayer.h"
#include "gui/quick/render/renderdevice.h"
#include "gui/quick/render/rendererrorreporter.h"
#include "gui/quick/render/renderframe.h"

// Render-thread half of ImageRenderItem.
//
// Draws the RenderFrame taken in synchronize() into the item's colour buffer
// in one render pass, cleared to the background colour:
//
// - Flat projection: the image (an ImageLayer: tiles, conversion pass, exact
//   downsample, MKS2021 resampling, see there) as one textured quad per
//   visible tile through image.vert/.frag (sharpening, colour adjustments,
//   transparency checkerboard), then the upscaled crop, a second ImageLayer
//   drawn the same way over the area of the image it covers.
// - Equirectangular projection: the image's tiles through
//   panorama.vert/.frag, one full-item quad per tile; the crop is not drawn.
//
// Resources that only depend on the QRhi live in RenderDevice; the two
// graphics pipelines of the item's render pass are created here for its
// render target.
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
  // Pipeline of the item's render pass with premultiplied-alpha blending.
  struct MainPipeline {
    std::unique_ptr<QRhiRenderPassDescriptor> renderPass;
    int sampleCount = 0;
    std::unique_ptr<QRhiGraphicsPipeline> pipeline;
  };

  // (Re)creates pipeline when the render target's pass is not compatible
  // with the one it was created for; false on failure (reported).
  [[nodiscard]] bool ensurePipeline(MainPipeline &pipeline,
                                    const QShader &vertexShader,
                                    const QShader &fragmentShader,
                                    const QString &name);
  void releaseResources();
  [[nodiscard]] int effectiveTileSizeLimit() const;

  // Draw lists of the flat projection, uniforms written through updates.
  void prepareFlatDraws(QRhiCommandBuffer *cb, QSize targetSize,
                        const QMatrix4x4 &mvp,
                        QRhiResourceUpdateBatch *updates,
                        std::vector<QRhiShaderResourceBindings *> &draws);
  // Draw list of the panorama projection.
  void preparePanoramaDraws(QSize targetSize, const QMatrix4x4 &mvp,
                            QRhiResourceUpdateBatch *updates,
                            std::vector<QRhiShaderResourceBindings *> &draws);

  // Declared before the objects that report through it.
  RenderErrorReporter mErrors;
  RenderDevice mDevice{mErrors};
  ImageLayer mImage{mDevice};
  ImageLayer mCrop{mDevice};

  QRhi *mRhi = nullptr;
  RenderFrame mFrame;
  MainPipeline mImagePipeline;
  MainPipeline mPanoramaPipeline;
};
