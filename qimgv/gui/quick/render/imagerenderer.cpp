#include "imagerenderer.h"

#include <QFile>
#include <QImage>
#include <QLatin1StringView>
#include <QMatrix4x4>
#include <QRectF>
#include <cmath>
#include <cstring>
#include <utility>

#include "gui/quick/render/imagerenderitem.h"
#include "gui/quick/render/textureuploadformat.h"

namespace {
using namespace Qt::StringLiterals;

constexpr QLatin1StringView kVertexShaderPath =
    ":/qimgv/render/shaders/image.vert.qsb"_L1;
constexpr QLatin1StringView kFragmentShaderPath =
    ":/qimgv/render/shaders/image.frag.qsb"_L1;

// Unit quad as a triangle strip; image.vert stretches it over the target.
constexpr std::array<float, 8> kQuadCorners = {0.0f, 0.0f, 1.0f, 0.0f,
                                               0.0f, 1.0f, 1.0f, 1.0f};
constexpr quint32 kQuadVertexCount = 4;
constexpr quint32 kQuadVertexStride = 2 * sizeof(float);
constexpr int kUniformBinding = 0;
constexpr int kTextureBinding = 1;

// Transparency checkerboard, identical to ImageViewerV2's: 16 logical px
// square tiles of 2 x 2 cells, light cells #999999, dark cells #666666.
constexpr qreal kCheckerboardTileSizePx = 16.0;
constexpr int kCheckerboardCellsPerAxis = 2;
constexpr float kCheckerboardLight = 0x99 / 255.0f;
constexpr float kCheckerboardDark = 0x66 / 255.0f;
constexpr qreal kMinimumDevicePixelRatio = 1.0;

constexpr QSize kLayoutTextureSize(1, 1);
constexpr float kClearDepth = 1.0f;
constexpr quint32 kClearStencil = 0;

// std140 mirror of the TileParams block in res/shaders/rhi/image.vert/.frag.
struct TileUniforms {
  float mvp[16];
  float targetRect[4];
  float texRect[4];
  float checkerLight[4];
  float checkerDark[4];
  float checkerOrigin[2];
  float checkerTile;
  float checkerFirstCell;
  qint32 checkerEnabled;
  qint32 padding[3];
};
static_assert(offsetof(TileUniforms, targetRect) == 64);
static_assert(offsetof(TileUniforms, texRect) == 80);
static_assert(offsetof(TileUniforms, checkerLight) == 96);
static_assert(offsetof(TileUniforms, checkerDark) == 112);
static_assert(offsetof(TileUniforms, checkerOrigin) == 128);
static_assert(offsetof(TileUniforms, checkerTile) == 136);
static_assert(offsetof(TileUniforms, checkerFirstCell) == 140);
static_assert(offsetof(TileUniforms, checkerEnabled) == 144);
static_assert(sizeof(TileUniforms) % 16 == 0);

std::size_t samplingIndex(RenderEnums::TextureSampling sampling) {
  return static_cast<std::size_t>(sampling);
}

QColor premultiplied(const QColor &color) {
  const float alpha = color.alphaF();
  return QColor::fromRgbF(color.redF() * alpha, color.greenF() * alpha,
                          color.blueF() * alpha, alpha);
}

void setRect(float (&target)[4], const QRectF &rect) {
  target[0] = static_cast<float>(rect.x());
  target[1] = static_cast<float>(rect.y());
  target[2] = static_cast<float>(rect.width());
  target[3] = static_cast<float>(rect.height());
}

void setGray(float (&target)[4], float value) {
  target[0] = value;
  target[1] = value;
  target[2] = value;
  target[3] = 1.0f;
}

// Device-pixel geometry of the image in the colour buffer.
struct ImageGeometry {
  // Top-left corner of the image, snapped to whole device pixels so that a
  // 1:1 scale maps texel centres onto pixel centres at any DPR.
  QPointF origin;
  qreal scale = 1.0;
};

ImageGeometry imageGeometry(const RenderFrame &frame) {
  const qreal dpr = frame.devicePixelRatio;
  return ImageGeometry{
      QPointF(std::round(frame.placement.position.x() * dpr),
              std::round(frame.placement.position.y() * dpr)),
      frame.placement.scale};
}

// Fills the per-draw part of the uniforms for one tile. Returns false when
// the tile lies outside the colour buffer.
bool tileDraw(const ImageTile &tile, const ImageGeometry &geometry,
              QSize targetSize, TileUniforms &uniforms) {
  const QRectF core(tile.core);
  const QRectF deviceRect(geometry.origin + core.topLeft() * geometry.scale,
                          core.size() * geometry.scale);
  // Clip on the CPU in double precision: at high zoom levels the unclipped
  // corners of a large image are too far away for float vertex positions.
  const QRectF visible =
      deviceRect.intersected(QRectF(QPointF(0.0, 0.0), QSizeF(targetSize)));
  if (visible.isEmpty())
    return false;

  const QRectF texture(tile.texture);
  const QPointF sourceTopLeft =
      core.topLeft() + (visible.topLeft() - deviceRect.topLeft()) /
                           geometry.scale;
  const QSizeF sourceSize = visible.size() / geometry.scale;
  const QRectF texRect(
      QPointF((sourceTopLeft.x() - texture.x()) / texture.width(),
              (sourceTopLeft.y() - texture.y()) / texture.height()),
      QSizeF(sourceSize.width() / texture.width(),
             sourceSize.height() / texture.height()));
  setRect(uniforms.targetRect, visible);
  setRect(uniforms.texRect, texRect);
  return true;
}
} // namespace

//------------------------------------------------------------------------------
ImageRenderer::ImageRenderer() = default;

// QRhi resources are released by their unique_ptr members; QQuickRhiItem
// destroys the renderer on the render thread before its QRhi.
ImageRenderer::~ImageRenderer() = default;

//------------------------------------------------------------------------------
void ImageRenderer::synchronize(QQuickRhiItem *item) {
  auto *imageItem = qobject_cast<ImageRenderItem *>(item);
  if (!imageItem) {
    reportError(u"ImageRenderer is attached to an item that is not an "
                u"ImageRenderItem"_s);
    return;
  }
  mFrame = imageItem->frameSnapshot();
  mErrorChannel = imageItem->errorChannel();
  if (mErrorChannel) {
    for (const QString &message : std::exchange(mUnsentErrors, {}))
      mErrorChannel->post(message);
  }
}

//------------------------------------------------------------------------------
void ImageRenderer::initialize(QRhiCommandBuffer *cb) {
  if (rhi() != mRhi) {
    // First initialization, or the item moved to another window / the QRhi
    // was recreated: nothing created on the previous QRhi may be used.
    releaseDeviceResources();
    mRhi = rhi();
    mDeviceResourcesReady = createDeviceResources(cb);
  }
  if (mDeviceResourcesReady && !ensurePipeline())
    mPipeline.reset();
}

//------------------------------------------------------------------------------
bool ImageRenderer::loadShader(const QString &path, QShader &shader) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    reportError(u"Cannot open shader %1: %2"_s.arg(path, file.errorString()));
    return false;
  }
  shader = QShader::fromSerialized(file.readAll());
  if (!shader.isValid()) {
    reportError(u"Shader %1 is not a valid .qsb file"_s.arg(path));
    return false;
  }
  return true;
}

bool ImageRenderer::createDeviceResources(QRhiCommandBuffer *cb) {
  if (!mRhi) {
    reportError(u"No QRhi is available for the image renderer"_s);
    return false;
  }
  if (!loadShader(kVertexShaderPath, mVertexShader) ||
      !loadShader(kFragmentShaderPath, mFragmentShader))
    return false;

  mVertexBuffer.reset(mRhi->newBuffer(QRhiBuffer::Immutable,
                                      QRhiBuffer::VertexBuffer,
                                      sizeof(kQuadCorners)));
  if (!mVertexBuffer->create()) {
    reportError(u"Cannot create the image quad vertex buffer"_s);
    return false;
  }

  using Filter = QRhiSampler::Filter;
  const auto createSampler = [this](RenderEnums::TextureSampling sampling,
                                    Filter filter, Filter mipmapMode) {
    auto &sampler = mSamplers[samplingIndex(sampling)];
    sampler.reset(mRhi->newSampler(filter, filter, mipmapMode,
                                   QRhiSampler::ClampToEdge,
                                   QRhiSampler::ClampToEdge));
    return sampler->create();
  };
  if (!createSampler(RenderEnums::TextureSampling::Nearest, Filter::Nearest,
                     Filter::None) ||
      !createSampler(RenderEnums::TextureSampling::Bilinear, Filter::Linear,
                     Filter::None) ||
      !createSampler(RenderEnums::TextureSampling::Trilinear, Filter::Linear,
                     Filter::Linear)) {
    reportError(u"Cannot create the image samplers"_s);
    return false;
  }

  mLayoutTexture.reset(mRhi->newTexture(QRhiTexture::RGBA8, kLayoutTextureSize));
  mLayoutUniforms.reset(mRhi->newBuffer(
      QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(TileUniforms)));
  if (!mLayoutTexture->create() || !mLayoutUniforms->create()) {
    reportError(u"Cannot create the image pipeline layout resources"_s);
    return false;
  }
  mLayoutBindings.reset(mRhi->newShaderResourceBindings());
  mLayoutBindings->setBindings(
      {QRhiShaderResourceBinding::uniformBuffer(
           kUniformBinding,
           QRhiShaderResourceBinding::VertexStage |
               QRhiShaderResourceBinding::FragmentStage,
           mLayoutUniforms.get()),
       QRhiShaderResourceBinding::sampledTexture(
           kTextureBinding, QRhiShaderResourceBinding::FragmentStage,
           mLayoutTexture.get(),
           mSamplers[samplingIndex(RenderEnums::TextureSampling::Nearest)]
               .get())});
  if (!mLayoutBindings->create()) {
    reportError(u"Cannot create the image pipeline layout bindings"_s);
    return false;
  }

  QRhiResourceUpdateBatch *updates = mRhi->nextResourceUpdateBatch();
  if (!updates) {
    reportError(u"No QRhi resource update batch is available"_s);
    return false;
  }
  updates->uploadStaticBuffer(mVertexBuffer.get(), kQuadCorners.data());
  cb->resourceUpdate(updates);
  return true;
}

bool ImageRenderer::ensurePipeline() {
  QRhiRenderTarget *target = renderTarget();
  if (!target) {
    reportError(u"The image item has no render target"_s);
    return false;
  }
  QRhiRenderPassDescriptor *pass = target->renderPassDescriptor();
  if (mPipeline && mPipelineRenderPass &&
      mPipelineRenderPass->isCompatible(pass) &&
      mPipelineSampleCount == target->sampleCount())
    return true;

  mPipeline.reset();
  mPipelineRenderPass.reset(pass->newCompatibleRenderPassDescriptor());
  mPipelineSampleCount = target->sampleCount();

  auto pipeline = std::unique_ptr<QRhiGraphicsPipeline>(
      mRhi->newGraphicsPipeline());
  // Premultiplied-alpha blending over the cleared background.
  QRhiGraphicsPipeline::TargetBlend blend;
  blend.enable = true;
  blend.srcColor = QRhiGraphicsPipeline::One;
  blend.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
  blend.srcAlpha = QRhiGraphicsPipeline::One;
  blend.dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;
  pipeline->setTargetBlends({blend});
  pipeline->setTopology(QRhiGraphicsPipeline::TriangleStrip);
  pipeline->setSampleCount(mPipelineSampleCount);
  pipeline->setShaderStages(
      {{QRhiShaderStage::Vertex, mVertexShader},
       {QRhiShaderStage::Fragment, mFragmentShader}});
  QRhiVertexInputLayout inputLayout;
  inputLayout.setBindings({{kQuadVertexStride}});
  inputLayout.setAttributes(
      {{0, 0, QRhiVertexInputAttribute::Float2, 0}});
  pipeline->setVertexInputLayout(inputLayout);
  pipeline->setShaderResourceBindings(mLayoutBindings.get());
  pipeline->setRenderPassDescriptor(mPipelineRenderPass.get());
  if (!pipeline->create()) {
    reportError(u"Cannot create the image graphics pipeline"_s);
    return false;
  }
  mPipeline = std::move(pipeline);
  return true;
}

void ImageRenderer::releaseDeviceResources() {
  releaseTiles();
  mUploadedGeneration.reset();
  mPipeline.reset();
  mPipelineRenderPass.reset();
  mPipelineSampleCount = 0;
  mLayoutBindings.reset();
  mLayoutUniforms.reset();
  mLayoutTexture.reset();
  for (auto &sampler : mSamplers)
    sampler.reset();
  mVertexBuffer.reset();
  mDeviceResourcesReady = false;
}

//------------------------------------------------------------------------------
int ImageRenderer::effectiveTileSizeLimit() const {
  const int gpuLimit = mRhi->resourceLimit(QRhi::TextureSizeMax);
  const int settingsLimit = mFrame.settings.maxTileSize;
  return settingsLimit == RenderSettings::kNoTileSizeLimit
             ? gpuLimit
             : qMin(gpuLimit, settingsLimit);
}

bool ImageRenderer::needsUpload() const {
  return !mUploadedGeneration ||
         *mUploadedGeneration != mFrame.imageGeneration ||
         (mFrame.image && mUploadedTileSizeLimit != effectiveTileSizeLimit());
}

void ImageRenderer::releaseTiles() {
  // QRhi defers the release of native resources still used by frames in
  // flight, so the tiles can be dropped immediately.
  mTiles.clear();
  mImageHasAlpha = false;
}

bool ImageRenderer::createTile(GpuTile &tile, QRhiTexture::Format format) {
  tile.texture.reset(mRhi->newTexture(
      format, tile.region.texture.size(), 1,
      QRhiTexture::MipMapped | QRhiTexture::UsedWithGenerateMips));
  if (!tile.texture->create()) {
    reportError(u"Cannot create a %1 x %2 image texture"_s.arg(
        tile.region.texture.width()).arg(tile.region.texture.height()));
    return false;
  }
  tile.uniforms.reset(mRhi->newBuffer(
      QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(TileUniforms)));
  if (!tile.uniforms->create()) {
    reportError(u"Cannot create an image tile uniform buffer"_s);
    return false;
  }
  for (std::size_t index = 0; index < kSamplingCount; ++index) {
    auto &bindings = tile.bindings[index];
    bindings.reset(mRhi->newShaderResourceBindings());
    bindings->setBindings(
        {QRhiShaderResourceBinding::uniformBuffer(
             kUniformBinding,
             QRhiShaderResourceBinding::VertexStage |
                 QRhiShaderResourceBinding::FragmentStage,
             tile.uniforms.get()),
         QRhiShaderResourceBinding::sampledTexture(
             kTextureBinding, QRhiShaderResourceBinding::FragmentStage,
             tile.texture.get(), mSamplers[index].get())});
    if (!bindings->create()) {
      reportError(u"Cannot create image tile shader bindings"_s);
      return false;
    }
  }
  return true;
}

void ImageRenderer::uploadImage(QRhiResourceUpdateBatch *updates) {
  releaseTiles();
  mUploadedGeneration = mFrame.imageGeneration;
  mUploadedTileSizeLimit = effectiveTileSizeLimit();
  if (!mFrame.image)
    return;

  const QImage &image = *mFrame.image;
  const QList<ImageTile> layout =
      TileGrid::layout(image.size(), mUploadedTileSizeLimit);
  if (layout.isEmpty()) {
    reportError(u"Cannot split a %1 x %2 image into textures of at most %3 "
                u"pixels"_s.arg(image.width())
                    .arg(image.height())
                    .arg(mUploadedTileSizeLimit));
    return;
  }

  const TextureFormatSupport support{
      mRhi->isTextureFormatSupported(QRhiTexture::BGRA8),
      mRhi->isTextureFormatSupported(QRhiTexture::RGBA16F,
                                     QRhiTexture::MipMapped |
                                         QRhiTexture::UsedWithGenerateMips)};
  const TextureUploadFormat format =
      chooseTextureUploadFormat(image.format(), support);
  // Formats that match a texture format are uploaded straight from the
  // shared image; everything else is converted one tile at a time, so that a
  // huge image is never converted as a whole.
  const bool direct = format.imageFormat == image.format();

  std::vector<GpuTile> tiles(layout.size());
  for (qsizetype index = 0; index < layout.size(); ++index) {
    GpuTile &tile = tiles[index];
    tile.region = layout[index];
    if (!createTile(tile, format.textureFormat))
      return;

    QRhiTextureSubresourceUploadDescription source;
    if (direct) {
      source.setImage(image);
      source.setSourceTopLeft(tile.region.texture.topLeft());
      source.setSourceSize(tile.region.texture.size());
    } else {
      source.setImage(
          image.copy(tile.region.texture).convertToFormat(format.imageFormat));
    }
    if (source.image().isNull()) {
      reportError(u"Cannot convert a %1 x %2 image tile for upload"_s.arg(
          tile.region.texture.width()).arg(tile.region.texture.height()));
      return;
    }
    updates->uploadTexture(tile.texture.get(),
                           QRhiTextureUploadEntry(0, 0, source));
    updates->generateMips(tile.texture.get());
  }
  mTiles = std::move(tiles);
  mImageHasAlpha = image.hasAlphaChannel();
  mLastError.clear();
}

//------------------------------------------------------------------------------
void ImageRenderer::render(QRhiCommandBuffer *cb) {
  QRhiRenderTarget *target = renderTarget();
  if (!target) {
    reportError(u"The image item has no render target"_s);
    return;
  }
  QRhiResourceUpdateBatch *updates = mRhi->nextResourceUpdateBatch();
  if (!updates) {
    reportError(u"No QRhi resource update batch is available"_s);
    return;
  }
  if (mPipeline && needsUpload())
    uploadImage(updates);

  const QSize targetSize = target->pixelSize();
  std::vector<const GpuTile *> draws;
  if (mPipeline && !mTiles.empty()) {
    const ImageGeometry geometry = imageGeometry(mFrame);
    QMatrix4x4 mvp = mRhi->clipSpaceCorrMatrix();
    mvp.ortho(0.0f, static_cast<float>(targetSize.width()),
              static_cast<float>(targetSize.height()), 0.0f, -1.0f, 1.0f);

    TileUniforms uniforms{};
    std::memcpy(uniforms.mvp, mvp.constData(), sizeof(uniforms.mvp));
    const qreal dpr =
        qMax(mFrame.devicePixelRatio, kMinimumDevicePixelRatio);
    const int checkerTile = qRound(kCheckerboardTileSizePx * dpr);
    uniforms.checkerEnabled =
        mFrame.settings.transparencyGrid && mImageHasAlpha ? 1 : 0;
    uniforms.checkerTile = static_cast<float>(checkerTile);
    uniforms.checkerFirstCell =
        static_cast<float>(checkerTile / kCheckerboardCellsPerAxis);
    // The pattern starts at the image's top-left corner; reduced to one tile
    // period to keep the shader arithmetic small.
    uniforms.checkerOrigin[0] = static_cast<float>(
        std::fmod(geometry.origin.x(), static_cast<qreal>(checkerTile)));
    uniforms.checkerOrigin[1] = static_cast<float>(
        std::fmod(geometry.origin.y(), static_cast<qreal>(checkerTile)));
    setGray(uniforms.checkerLight, kCheckerboardLight);
    setGray(uniforms.checkerDark, kCheckerboardDark);

    if (geometry.scale > 0.0) {
      for (const GpuTile &tile : mTiles) {
        if (!tileDraw(tile.region, geometry, targetSize, uniforms))
          continue;
        updates->updateDynamicBuffer(tile.uniforms.get(), 0,
                                     sizeof(TileUniforms), &uniforms);
        draws.push_back(&tile);
      }
    }
  }

  cb->beginPass(target, premultiplied(mFrame.settings.backgroundColor),
                {kClearDepth, kClearStencil}, updates);
  if (!draws.empty()) {
    cb->setGraphicsPipeline(mPipeline.get());
    cb->setViewport(QRhiViewport(0.0f, 0.0f,
                                 static_cast<float>(targetSize.width()),
                                 static_cast<float>(targetSize.height())));
    const QRhiCommandBuffer::VertexInput vertexInput(mVertexBuffer.get(), 0);
    const std::size_t sampling = samplingIndex(mFrame.settings.sampling);
    for (const GpuTile *tile : draws) {
      cb->setShaderResources(tile->bindings[sampling].get());
      cb->setVertexInput(0, 1, &vertexInput);
      cb->draw(kQuadVertexCount);
    }
  }
  cb->endPass();
}

//------------------------------------------------------------------------------
void ImageRenderer::reportError(const QString &message) {
  // A persistent failure repeats every frame; report it once.
  if (message == mLastError)
    return;
  mLastError = message;
  if (mErrorChannel)
    mErrorChannel->post(message);
  else
    mUnsentErrors.append(message);
}
