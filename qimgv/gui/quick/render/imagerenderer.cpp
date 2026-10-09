#include "imagerenderer.h"

#include <QMatrix4x4>
#include <QRectF>
#include <cmath>
#include <cstring>
#include <vector>

#include "gui/quick/render/imagerenderitem.h"
#include "gui/quick/render/rhipassuniforms.h"

namespace {
using namespace Qt::StringLiterals;

constexpr float kClearDepth = 1.0f;
constexpr quint32 kClearStencil = 0;
constexpr quint32 kQuadVertexStride = 2 * sizeof(float);

constexpr double kDegreesToRadians = 3.14159265358979323846 / 180.0;
constexpr double kHalf = 0.5;
// Panorama tile cores at the image border extend past [0, 1] so that rays
// exactly on the border (uv 0 or 1) still hit a tile.
constexpr double kOpenEdgeLow = -1.0;
constexpr double kOpenEdgeHigh = 2.0;

QColor premultiplied(const QColor &color) {
  const float alpha = color.alphaF();
  return QColor::fromRgbF(color.redF() * alpha, color.greenF() * alpha,
                          color.blueF() * alpha, alpha);
}

// Top-left corner of the image in device pixels, snapped to whole pixels so
// that a 1:1 scale maps texel centres onto pixel centres at any DPR.
QPointF imageOrigin(const RenderFrame &frame) {
  const qreal dpr = frame.devicePixelRatio;
  return QPointF(std::round(frame.placement.position.x() * dpr),
                 std::round(frame.placement.position.y() * dpr));
}

PanoramaUniforms panoramaUniforms(const RenderFrame &frame, QSize targetSize,
                                  const QMatrix4x4 &mvp) {
  PanoramaUniforms uniforms;
  std::memcpy(uniforms.mvp, mvp.constData(), sizeof(uniforms.mvp));
  uniforms.targetSize[0] = static_cast<float>(targetSize.width());
  uniforms.targetSize[1] = static_cast<float>(targetSize.height());
  const PanoramaCamera &camera = frame.panorama;
  uniforms.tanHalfFov =
      static_cast<float>(std::tan(camera.fov * kDegreesToRadians * kHalf));
  uniforms.aspect = static_cast<float>(
      static_cast<double>(targetSize.width()) / targetSize.height());
  const double yaw = camera.yaw * kDegreesToRadians;
  const double pitch = camera.pitch * kDegreesToRadians;
  uniforms.sinYaw = static_cast<float>(std::sin(yaw));
  uniforms.cosYaw = static_cast<float>(std::cos(yaw));
  uniforms.sinPitch = static_cast<float>(std::sin(pitch));
  uniforms.cosPitch = static_cast<float>(std::cos(pitch));
  if (frame.filter.colorAdjustments.hasAdjustments()) {
    const ColorMatrix matrix =
        colorAdjustmentMatrix(frame.filter.colorAdjustments);
    setUniformRow(uniforms.colorRow0, matrix.m[0]);
    setUniformRow(uniforms.colorRow1, matrix.m[1]);
    setUniformRow(uniforms.colorRow2, matrix.m[2]);
    uniforms.colorOffset = matrix.offset;
    uniforms.colorEnabled = PanoramaUniforms::kEnabled;
  }
  return uniforms;
}

// Normalized core of tile with the image border opened (kOpenEdge*).
QRectF panoramaCore(const ImageTile &tile, QSize imageSize) {
  const double width = imageSize.width();
  const double height = imageSize.height();
  const QRect &core = tile.core;
  const double left = core.left() == 0 ? kOpenEdgeLow : core.left() / width;
  const double top = core.top() == 0 ? kOpenEdgeLow : core.top() / height;
  const double right = core.right() + 1 == imageSize.width()
                           ? kOpenEdgeHigh
                           : (core.right() + 1) / width;
  const double bottom = core.bottom() + 1 == imageSize.height()
                            ? kOpenEdgeHigh
                            : (core.bottom() + 1) / height;
  return QRectF(QPointF(left, top), QPointF(right, bottom));
}
} // namespace

//------------------------------------------------------------------------------
ImageRenderer::ImageRenderer() = default;

// QRhi resources are released by their owners; QQuickRhiItem destroys the
// renderer on the render thread before its QRhi.
ImageRenderer::~ImageRenderer() = default;

//------------------------------------------------------------------------------
void ImageRenderer::synchronize(QQuickRhiItem *item) {
  auto *imageItem = qobject_cast<ImageRenderItem *>(item);
  if (!imageItem) {
    mErrors.report(u"ImageRenderer is attached to an item that is not an "
                   u"ImageRenderItem"_s);
    return;
  }
  mFrame = imageItem->frameSnapshot();
  mErrors.setChannel(imageItem->errorChannel());
  mDevice.setStatistics(imageItem->statisticsChannel());
}

//------------------------------------------------------------------------------
void ImageRenderer::initialize(QRhiCommandBuffer *cb) {
  if (rhi() != mRhi) {
    // First initialization, or the item moved to another window / the QRhi
    // was recreated: nothing created on the previous QRhi may be used.
    releaseResources();
    mRhi = rhi();
    if (!mDevice.create(mRhi, cb))
      return;
  }
  if (!mDevice.isReady())
    return;
  const RenderDevice::MainShaders &shaders = mDevice.shaders();
  if (!ensurePipeline(mImagePipeline, shaders.imageVertex,
                      shaders.imageFragment, u"image"_s))
    mImagePipeline.pipeline.reset();
  if (!ensurePipeline(mPanoramaPipeline, shaders.panoramaVertex,
                      shaders.panoramaFragment, u"panorama"_s))
    mPanoramaPipeline.pipeline.reset();
}

bool ImageRenderer::ensurePipeline(MainPipeline &pipeline,
                                   const QShader &vertexShader,
                                   const QShader &fragmentShader,
                                   const QString &name) {
  QRhiRenderTarget *target = renderTarget();
  if (!target) {
    mErrors.report(u"The image item has no render target"_s);
    return false;
  }
  QRhiRenderPassDescriptor *pass = target->renderPassDescriptor();
  if (pipeline.pipeline && pipeline.renderPass &&
      pipeline.renderPass->isCompatible(pass) &&
      pipeline.sampleCount == target->sampleCount())
    return true;

  pipeline.pipeline.reset();
  pipeline.renderPass.reset(pass->newCompatibleRenderPassDescriptor());
  pipeline.sampleCount = target->sampleCount();

  auto created = std::unique_ptr<QRhiGraphicsPipeline>(
      mRhi->newGraphicsPipeline());
  // Premultiplied-alpha blending over the cleared background.
  QRhiGraphicsPipeline::TargetBlend blend;
  blend.enable = true;
  blend.srcColor = QRhiGraphicsPipeline::One;
  blend.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
  blend.srcAlpha = QRhiGraphicsPipeline::One;
  blend.dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;
  created->setTargetBlends({blend});
  created->setTopology(QRhiGraphicsPipeline::TriangleStrip);
  created->setSampleCount(pipeline.sampleCount);
  created->setShaderStages({{QRhiShaderStage::Vertex, vertexShader},
                            {QRhiShaderStage::Fragment, fragmentShader}});
  QRhiVertexInputLayout inputLayout;
  inputLayout.setBindings({{kQuadVertexStride}});
  inputLayout.setAttributes({{0, 0, QRhiVertexInputAttribute::Float2, 0}});
  created->setVertexInputLayout(inputLayout);
  created->setShaderResourceBindings(mDevice.layoutBindings());
  created->setRenderPassDescriptor(pipeline.renderPass.get());
  if (!created->create()) {
    mErrors.report(u"Cannot create the %1 graphics pipeline"_s.arg(name));
    return false;
  }
  pipeline.pipeline = std::move(created);
  return true;
}

void ImageRenderer::releaseResources() {
  mCrop.release();
  mImage.release();
  mPanoramaPipeline = {};
  mImagePipeline = {};
  mDevice.release();
}

int ImageRenderer::effectiveTileSizeLimit() const {
  const int gpuLimit = mRhi->resourceLimit(QRhi::TextureSizeMax);
  const int settingsLimit = mFrame.settings.maxTileSize;
  return settingsLimit == RenderSettings::kNoTileSizeLimit
             ? gpuLimit
             : qMin(gpuLimit, settingsLimit);
}

//------------------------------------------------------------------------------
void ImageRenderer::prepareFlatDraws(
    QRhiCommandBuffer *cb, QSize targetSize, const QMatrix4x4 &mvp,
    QRhiResourceUpdateBatch *updates,
    std::vector<QRhiShaderResourceBindings *> &draws) {
  const QPointF origin = imageOrigin(mFrame);
  const qreal scale = mFrame.placement.scale;
  mImage.prepareDraws(cb, mFrame,
                      ImageLayer::DrawParams{origin, scale, targetSize,
                                             origin, mvp},
                      updates, draws);
  if (mCrop.isEmpty() || mImage.isEmpty())
    return;
  // The crop covers sourceRect of the image: its corner lands on the
  // image's (snapped to whole device pixels, like the widget viewer's
  // sceneRoundPos()), and its pixels are scaled by the net upscale factor.
  const QRect &sourceRect = mFrame.crop.sourceRect;
  const QSize cropSize = mCrop.imageSize();
  if (sourceRect.isEmpty() || cropSize.isEmpty())
    return;
  const QPointF cropCorner = origin + QPointF(sourceRect.topLeft()) * scale;
  const QPointF cropOrigin(std::round(cropCorner.x()),
                           std::round(cropCorner.y()));
  const qreal cropScale = scale * sourceRect.width() / cropSize.width();
  mCrop.prepareDraws(cb, mFrame,
                     ImageLayer::DrawParams{cropOrigin, cropScale, targetSize,
                                            origin, mvp},
                     updates, draws);
}

void ImageRenderer::preparePanoramaDraws(
    QSize targetSize, const QMatrix4x4 &mvp, QRhiResourceUpdateBatch *updates,
    std::vector<QRhiShaderResourceBindings *> &draws) {
  if (mImage.isEmpty() || targetSize.isEmpty())
    return;
  const QSize imageSize = mImage.imageSize();
  PanoramaUniforms uniforms = panoramaUniforms(mFrame, targetSize, mvp);
  for (ImageLayer::GpuTile &tile : mImage.tiles()) {
    const QRect &texture = tile.region.texture;
    const bool wraps = texture.width() == imageSize.width();
    QRhiShaderResourceBindings *bindings =
        wraps ? mImage.wrappingBindings(tile, mFrame.settings.sampling)
              : tile.bindings[samplingIndex(mFrame.settings.sampling)].get();
    if (!bindings)
      continue;
    const QRectF core = panoramaCore(tile.region, imageSize);
    uniforms.core[0] = static_cast<float>(core.left());
    uniforms.core[1] = static_cast<float>(core.top());
    uniforms.core[2] = static_cast<float>(core.right());
    uniforms.core[3] = static_cast<float>(core.bottom());
    setUniformRect(uniforms.texRect,
                   QRectF(double(texture.x()) / imageSize.width(),
                          double(texture.y()) / imageSize.height(),
                          double(texture.width()) / imageSize.width(),
                          double(texture.height()) / imageSize.height()));
    uniforms.wrapsHorizontally =
        wraps ? PanoramaUniforms::kEnabled : PanoramaUniforms::kDisabled;
    updates->updateDynamicBuffer(tile.uniforms.get(), 0,
                                 sizeof(PanoramaUniforms), &uniforms);
    draws.push_back(bindings);
  }
}

//------------------------------------------------------------------------------
void ImageRenderer::render(QRhiCommandBuffer *cb) {
  QRhiRenderTarget *target = renderTarget();
  if (!target) {
    mErrors.report(u"The image item has no render target"_s);
    return;
  }
  const bool panorama =
      mFrame.projection == RenderEnums::Projection::Equirectangular;
  const bool pipelineReady = panorama ? mPanoramaPipeline.pipeline != nullptr
                                      : mImagePipeline.pipeline != nullptr;
  if (pipelineReady) {
    const int tileSizeLimit = effectiveTileSizeLimit();
    mImage.prepare(cb, mFrame.image, tileSizeLimit);
    // The crop is dropped in panorama mode, like in the widget viewer.
    if (!panorama && mFrame.crop.source.image)
      mCrop.prepare(cb, mFrame.crop.source, tileSizeLimit);
    else
      mCrop.release();
  }

  QRhiResourceUpdateBatch *updates = mRhi->nextResourceUpdateBatch();
  if (!updates) {
    mErrors.report(u"No QRhi resource update batch is available"_s);
    return;
  }
  const QSize targetSize = target->pixelSize();
  QMatrix4x4 mvp = mRhi->clipSpaceCorrMatrix();
  mvp.ortho(0.0f, static_cast<float>(targetSize.width()),
            static_cast<float>(targetSize.height()), 0.0f, -1.0f, 1.0f);
  std::vector<QRhiShaderResourceBindings *> draws;
  if (pipelineReady) {
    if (panorama)
      preparePanoramaDraws(targetSize, mvp, updates, draws);
    else
      prepareFlatDraws(cb, targetSize, mvp, updates, draws);
  }

  cb->beginPass(target, premultiplied(mFrame.settings.backgroundColor),
                {kClearDepth, kClearStencil}, updates);
  if (!draws.empty()) {
    cb->setGraphicsPipeline(panorama ? mPanoramaPipeline.pipeline.get()
                                     : mImagePipeline.pipeline.get());
    cb->setViewport(QRhiViewport(0.0f, 0.0f,
                                 static_cast<float>(targetSize.width()),
                                 static_cast<float>(targetSize.height())));
    for (QRhiShaderResourceBindings *bindings : draws) {
      cb->setShaderResources(bindings);
      mDevice.drawQuad(cb);
    }
  }
  cb->endPass();
}
