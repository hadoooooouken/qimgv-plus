#include "imagerenderer.h"

#include <QFile>
#include <QImage>
#include <QLatin1StringView>
#include <QMatrix4x4>
#include <QRectF>
#include <QVarLengthArray>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>
#include <vector>

#include "gui/quick/render/imagerenderitem.h"
#include "gui/quick/render/resamplegrid.h"
#include "gui/quick/render/textureuploadformat.h"

namespace {
using namespace Qt::StringLiterals;

constexpr QLatin1StringView kVertexShaderPath =
    ":/qimgv/render/shaders/image.vert.qsb"_L1;
constexpr QLatin1StringView kFragmentShaderPath =
    ":/qimgv/render/shaders/image.frag.qsb"_L1;
constexpr QLatin1StringView kReduceVertexShaderPath =
    ":/qimgv/render/shaders/boxreduce.vert.qsb"_L1;
constexpr QLatin1StringView kReduceFragmentShaderPath =
    ":/qimgv/render/shaders/boxreduce.frag.qsb"_L1;
constexpr QLatin1StringView kResampleVertexShaderPath =
    ":/qimgv/render/shaders/resample.vert.qsb"_L1;
constexpr QLatin1StringView kResampleFragmentShaderPath =
    ":/qimgv/render/shaders/resample.frag.qsb"_L1;

// Unit quad as a triangle strip; image.vert / boxreduce.vert stretch it over
// the target.
constexpr std::array<float, 8> kQuadCorners = {0.0f, 0.0f, 1.0f, 0.0f,
                                               0.0f, 1.0f, 1.0f, 1.0f};
constexpr quint32 kQuadVertexCount = 4;
constexpr quint32 kQuadVertexStride = 2 * sizeof(float);
constexpr int kUniformBinding = 0;
constexpr int kTextureBinding = 1;
constexpr int kWeightTableBinding = 2;

// Transparency checkerboard, identical to ImageViewerV2's: 16 logical px
// square tiles of 2 x 2 cells, light cells #999999, dark cells #666666.
constexpr qreal kCheckerboardTileSizePx = 16.0;
constexpr int kCheckerboardCellsPerAxis = 2;
constexpr float kCheckerboardLight = 0x99 / 255.0f;
constexpr float kCheckerboardDark = 0x66 / 255.0f;
constexpr qreal kMinimumDevicePixelRatio = 1.0;

// Same thresholds as FilterPixmapItem: below kDownscaleThreshold the image is
// minified (mip chain, downscale sharpening taps, exact downsample when
// settled); within kOneToOneScaleTolerance of 1 no sharpening is applied.
constexpr qreal kDownscaleThreshold = 0.999;
constexpr qreal kOneToOneScaleTolerance = 0.001;

// Each exact-downsample pass at most halves a side (rounding up), so its
// ratio stays within [0.5, 1]; boxreduce.frag also covers the mip chain's
// floor halving of odd sizes (down to 1/3).
constexpr int kReduceDivisor = 2;
constexpr int kMinimumReducedSide = 1;
// Intermediate targets are fully overwritten.
const QColor kReduceClearColor = Qt::transparent;

constexpr QSize kLayoutTextureSize(1, 1);
constexpr float kClearDepth = 1.0f;
constexpr quint32 kClearStencil = 0;

// Value of the shader's sharpenMode for each RenderEnums::Sharpening.
static_assert(static_cast<int>(RenderEnums::Sharpening::None) == 0);
static_assert(static_cast<int>(RenderEnums::Sharpening::Cas) == 1);
static_assert(static_cast<int>(RenderEnums::Sharpening::Smart) == 2);

// std140 mirror of the TileParams block in res/shaders/rhi/image.vert/.frag.
// Defaults describe an unfiltered, unadjusted draw without checkerboard.
struct TileUniforms {
  static constexpr float kIdentityRow0[4] = {1.0f, 0.0f, 0.0f, 0.0f};
  static constexpr float kIdentityRow1[4] = {0.0f, 1.0f, 0.0f, 0.0f};
  static constexpr float kIdentityRow2[4] = {0.0f, 0.0f, 1.0f, 0.0f};
  static constexpr float kNeutralColorOffset = 0.0f;
  static constexpr qint32 kDisabled = 0;
  static constexpr qint32 kEnabled = 1;

  float mvp[16]{};
  float targetRect[4]{};
  float texRect[4]{};
  float checkerLight[4]{};
  float checkerDark[4]{};
  float colorRow0[4] = {kIdentityRow0[0], kIdentityRow0[1], kIdentityRow0[2],
                        kIdentityRow0[3]};
  float colorRow1[4] = {kIdentityRow1[0], kIdentityRow1[1], kIdentityRow1[2],
                        kIdentityRow1[3]};
  float colorRow2[4] = {kIdentityRow2[0], kIdentityRow2[1], kIdentityRow2[2],
                        kIdentityRow2[3]};
  float checkerOrigin[2]{};
  float texelStep[2]{};
  float checkerTile = 0.0f;
  float checkerFirstCell = 0.0f;
  float colorOffset = kNeutralColorOffset;
  float casSharpening = ImageFilter::kDefaultCasSharpening;
  float casContrast = ImageFilter::kDefaultCasContrast;
  qint32 checkerEnabled = kDisabled;
  qint32 colorEnabled = kDisabled;
  qint32 sharpenMode = static_cast<qint32>(RenderEnums::Sharpening::None);
  qint32 downscaleTaps = kDisabled;
  qint32 padding[3]{};
};
static_assert(offsetof(TileUniforms, targetRect) == 64);
static_assert(offsetof(TileUniforms, texRect) == 80);
static_assert(offsetof(TileUniforms, checkerLight) == 96);
static_assert(offsetof(TileUniforms, checkerDark) == 112);
static_assert(offsetof(TileUniforms, colorRow0) == 128);
static_assert(offsetof(TileUniforms, colorRow1) == 144);
static_assert(offsetof(TileUniforms, colorRow2) == 160);
static_assert(offsetof(TileUniforms, checkerOrigin) == 176);
static_assert(offsetof(TileUniforms, texelStep) == 184);
static_assert(offsetof(TileUniforms, checkerTile) == 192);
static_assert(offsetof(TileUniforms, checkerFirstCell) == 196);
static_assert(offsetof(TileUniforms, colorOffset) == 200);
static_assert(offsetof(TileUniforms, casSharpening) == 204);
static_assert(offsetof(TileUniforms, casContrast) == 208);
static_assert(offsetof(TileUniforms, checkerEnabled) == 212);
static_assert(offsetof(TileUniforms, colorEnabled) == 216);
static_assert(offsetof(TileUniforms, sharpenMode) == 220);
static_assert(offsetof(TileUniforms, downscaleTaps) == 224);
static_assert(sizeof(TileUniforms) % 16 == 0);

// std140 mirror of the ReduceParams block in res/shaders/rhi/boxreduce.*.
struct ReduceUniforms {
  float mvp[16]{};
  float srcTexelSize[2]{};
  float dstSize[2]{};
  float ratio[2]{};
  float padding[2]{};
};
static_assert(offsetof(ReduceUniforms, srcTexelSize) == 64);
static_assert(offsetof(ReduceUniforms, dstSize) == 72);
static_assert(offsetof(ReduceUniforms, ratio) == 80);
static_assert(sizeof(ReduceUniforms) % 16 == 0);

// std140 mirror of the ResampleParams block in res/shaders/rhi/resample.*.
struct ResampleUniforms {
  // Values of the shader's axis.
  static constexpr qint32 kAxisHorizontal = 0;
  static constexpr qint32 kAxisVertical = 1;
  static constexpr qint32 kIntermediatePass = 0;
  static constexpr qint32 kFinalPass = 1;

  float mvp[16]{};
  float dstSize[2]{};
  qint32 axis = kAxisHorizontal;
  qint32 clampMin = 0;
  qint32 clampMax = 0;
  qint32 crossOffset = 0;
  qint32 finalPass = kIntermediatePass;
  qint32 tapCount = 0;
};
static_assert(offsetof(ResampleUniforms, dstSize) == 64);
static_assert(offsetof(ResampleUniforms, axis) == 72);
static_assert(offsetof(ResampleUniforms, clampMin) == 76);
static_assert(offsetof(ResampleUniforms, clampMax) == 80);
static_assert(offsetof(ResampleUniforms, crossOffset) == 84);
static_assert(offsetof(ResampleUniforms, finalPass) == 88);
static_assert(offsetof(ResampleUniforms, tapCount) == 92);
static_assert(sizeof(ResampleUniforms) % 16 == 0);

// Weight table of one resampling pass (resample.frag): column d describes
// destination index d = output firstOutput + d, with its first source texel
// (relative to the source texture's texel 0, source index sourceBase) in row
// 0 and its normalized weights in rows 1..taps, zero-padded.
constexpr int kWeightTableHeaderRows = 1;

struct ResampleWeights {
  QSize size;
  int taps = 0;
  // Row-major, size.width() floats per row.
  std::vector<float> values;
};

ResampleWeights resampleWeights(const Mks2021Axis &axis, int firstOutput,
                                int count, int sourceBase) {
  std::vector<std::vector<double>> weights(static_cast<std::size_t>(count));
  int taps = 0;
  for (int d = 0; d < count; ++d) {
    weights[d] = axis.weights(firstOutput + d);
    taps = qMax(taps, static_cast<int>(weights[d].size()));
  }
  ResampleWeights table;
  table.taps = taps;
  table.size = QSize(count, kWeightTableHeaderRows + taps);
  table.values.assign(
      static_cast<std::size_t>(count) * table.size.height(), 0.0f);
  for (int d = 0; d < count; ++d) {
    table.values[d] =
        static_cast<float>(axis.firstTap(firstOutput + d) - sourceBase);
    for (std::size_t t = 0; t < weights[d].size(); ++t) {
      const std::size_t row = kWeightTableHeaderRows + t;
      table.values[row * count + d] = static_cast<float>(weights[d][t]);
    }
  }
  return table;
}

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

void setRow(float (&target)[4], const float (&row)[3]) {
  target[0] = row[0];
  target[1] = row[1];
  target[2] = row[2];
  target[3] = 0.0f;
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

// Filtering decisions of one frame, shared by all tiles.
struct FrameFilter {
  bool downscaling = false;
  bool sharpen = false;
  // Draw the exact-ratio downsample instead of the mip chain.
  bool exactReduce = false;
  // Draw the MKS2021 resampling instead of the mip chain.
  bool resample = false;
};

FrameFilter frameFilter(const RenderFrame &frame, qreal scale,
                        bool reduceAvailable, bool resampleAvailable) {
  FrameFilter filter;
  filter.downscaling = scale < kDownscaleThreshold;
  const bool oneToOne = std::abs(scale - 1.0) < kOneToOneScaleTolerance;
  filter.sharpen =
      frame.filter.sharpening != RenderEnums::Sharpening::None && !oneToOne;
  const bool settledSmooth =
      frame.settled &&
      frame.settings.sampling != RenderEnums::TextureSampling::Nearest;
  filter.resample =
      resampleAvailable && settledSmooth && !oneToOne &&
      frame.filter.resampling == RenderEnums::Resampling::Mks2021;
  filter.exactReduce = reduceAvailable && settledSmooth &&
                       filter.downscaling && !filter.resample;
  return filter;
}

// Uniforms shared by every tile of the frame: projection, checkerboard,
// colour matrix and sharpening parameters.
TileUniforms frameUniforms(const RenderFrame &frame,
                           const ImageGeometry &geometry, bool imageHasAlpha,
                           const FrameFilter &filter,
                           const QMatrix4x4 &mvp) {
  TileUniforms uniforms;
  std::memcpy(uniforms.mvp, mvp.constData(), sizeof(uniforms.mvp));
  const qreal dpr = qMax(frame.devicePixelRatio, kMinimumDevicePixelRatio);
  const int checkerTile = qRound(kCheckerboardTileSizePx * dpr);
  uniforms.checkerEnabled = frame.settings.transparencyGrid && imageHasAlpha
                                ? TileUniforms::kEnabled
                                : TileUniforms::kDisabled;
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

  const ImageFilter &imageFilter = frame.filter;
  if (imageFilter.colorAdjustments.hasAdjustments()) {
    const ColorMatrix matrix =
        colorAdjustmentMatrix(imageFilter.colorAdjustments);
    setRow(uniforms.colorRow0, matrix.m[0]);
    setRow(uniforms.colorRow1, matrix.m[1]);
    setRow(uniforms.colorRow2, matrix.m[2]);
    uniforms.colorOffset = matrix.offset;
    uniforms.colorEnabled = TileUniforms::kEnabled;
  }
  if (filter.sharpen) {
    uniforms.sharpenMode = static_cast<qint32>(imageFilter.sharpening);
    uniforms.casSharpening = imageFilter.casSharpening;
    uniforms.casContrast = imageFilter.casContrast;
  }
  return uniforms;
}

// Visible part of one tile in the colour buffer.
struct TileRegion {
  QRectF target;
  // Normalized texture coordinates of target in the tile's texture.
  QRectF texture;
};

// Returns an empty optional when the tile lies outside the colour buffer.
std::optional<TileRegion> visibleTileRegion(const ImageTile &tile,
                                            const ImageGeometry &geometry,
                                            QSize targetSize) {
  const QRectF core(tile.core);
  const QRectF deviceRect(geometry.origin + core.topLeft() * geometry.scale,
                          core.size() * geometry.scale);
  // Clip on the CPU in double precision: at high zoom levels the unclipped
  // corners of a large image are too far away for float vertex positions.
  const QRectF visible =
      deviceRect.intersected(QRectF(QPointF(0.0, 0.0), QSizeF(targetSize)));
  if (visible.isEmpty())
    return std::nullopt;

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
  return TileRegion{visible, texRect};
}

// Size of a texture of textureSize source pixels reduced to scale.
QSize reducedSize(QSize textureSize, qreal scale) {
  return QSize(qMax(kMinimumReducedSide, qRound(textureSize.width() * scale)),
               qMax(kMinimumReducedSide, qRound(textureSize.height() * scale)));
}

// Next step of the reduce chain from current towards target, per axis.
int nextReduceStep(int current, int target) {
  return current == target
             ? target
             : qMax(target, (current + kReduceDivisor - 1) / kReduceDivisor);
}

// Projection of a box-reduce pass: destination texel row 0 lands in the
// texture row that is sampled at v = 0 on every backend.
QMatrix4x4 reduceProjection(QRhi *rhi, QSize size) {
  QMatrix4x4 mvp = rhi->clipSpaceCorrMatrix();
  const float width = static_cast<float>(size.width());
  const float height = static_cast<float>(size.height());
  if (rhi->isYUpInFramebuffer())
    mvp.ortho(0.0f, width, 0.0f, height, -1.0f, 1.0f);
  else
    mvp.ortho(0.0f, width, height, 0.0f, -1.0f, 1.0f);
  return mvp;
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
    mReduceReady = mDeviceResourcesReady && createReduceResources();
    mResampleReady = mReduceReady && createResampleResources();
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

bool ImageRenderer::createReduceResources() {
  if (!loadShader(kReduceVertexShaderPath, mReduceVertexShader) ||
      !loadShader(kReduceFragmentShaderPath, mReduceFragmentShader))
    return false;

  // Reads exact texels of mip level 0; boxreduce.frag does the weighting.
  mReduceSampler.reset(mRhi->newSampler(
      QRhiSampler::Nearest, QRhiSampler::Nearest, QRhiSampler::None,
      QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge));
  mReduceLayoutUniforms.reset(mRhi->newBuffer(
      QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(ReduceUniforms)));
  if (!mReduceSampler->create() || !mReduceLayoutUniforms->create()) {
    reportError(u"Cannot create the exact downsample resources"_s);
    return false;
  }
  mReduceLayoutBindings.reset(mRhi->newShaderResourceBindings());
  mReduceLayoutBindings->setBindings(
      {QRhiShaderResourceBinding::uniformBuffer(
           kUniformBinding,
           QRhiShaderResourceBinding::VertexStage |
               QRhiShaderResourceBinding::FragmentStage,
           mReduceLayoutUniforms.get()),
       QRhiShaderResourceBinding::sampledTexture(
           kTextureBinding, QRhiShaderResourceBinding::FragmentStage,
           mLayoutTexture.get(), mReduceSampler.get())});
  if (!mReduceLayoutBindings->create()) {
    reportError(u"Cannot create the exact downsample layout bindings"_s);
    return false;
  }
  return true;
}

bool ImageRenderer::createResampleResources() {
  if (!loadShader(kResampleVertexShaderPath, mResampleVertexShader) ||
      !loadShader(kResampleFragmentShaderPath, mResampleFragmentShader))
    return false;

  // The weight tables hold one float per entry.
  if (!mRhi->isTextureFormatSupported(QRhiTexture::R32F)) {
    reportError(u"The GPU has no R32F textures; MKS2021 resampling is not "
                u"available"_s);
    return false;
  }
  mResampleLayoutUniforms.reset(mRhi->newBuffer(QRhiBuffer::Dynamic,
                                                QRhiBuffer::UniformBuffer,
                                                sizeof(ResampleUniforms)));
  if (!mResampleLayoutUniforms->create()) {
    reportError(u"Cannot create the resampling resources"_s);
    return false;
  }
  // resample.frag reads exact texels with texelFetch(); the sampler of the
  // exact downsample only completes the combined image sampler binding.
  mResampleLayoutBindings.reset(mRhi->newShaderResourceBindings());
  mResampleLayoutBindings->setBindings(
      {QRhiShaderResourceBinding::uniformBuffer(
           kUniformBinding,
           QRhiShaderResourceBinding::VertexStage |
               QRhiShaderResourceBinding::FragmentStage,
           mResampleLayoutUniforms.get()),
       QRhiShaderResourceBinding::sampledTexture(
           kTextureBinding, QRhiShaderResourceBinding::FragmentStage,
           mLayoutTexture.get(), mReduceSampler.get()),
       QRhiShaderResourceBinding::sampledTexture(
           kWeightTableBinding, QRhiShaderResourceBinding::FragmentStage,
           mLayoutTexture.get(), mReduceSampler.get())});
  if (!mResampleLayoutBindings->create()) {
    reportError(u"Cannot create the resampling layout bindings"_s);
    return false;
  }
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

QRhiGraphicsPipeline *
ImageRenderer::passPipeline(PassKind kind, QRhiTexture::Format format,
                            QRhiRenderPassDescriptor *compatiblePass) {
  for (const PassPipeline &entry : mPassPipelines) {
    if (entry.kind == kind && entry.format == format)
      return entry.pipeline.get();
  }

  const bool reduce = kind == PassKind::BoxReduce;
  PassPipeline entry;
  entry.kind = kind;
  entry.format = format;
  entry.renderPass.reset(compatiblePass->newCompatibleRenderPassDescriptor());
  auto pipeline = std::unique_ptr<QRhiGraphicsPipeline>(
      mRhi->newGraphicsPipeline());
  // Every destination texel is written once; no blending.
  pipeline->setTopology(QRhiGraphicsPipeline::TriangleStrip);
  pipeline->setShaderStages(
      {{QRhiShaderStage::Vertex,
        reduce ? mReduceVertexShader : mResampleVertexShader},
       {QRhiShaderStage::Fragment,
        reduce ? mReduceFragmentShader : mResampleFragmentShader}});
  QRhiVertexInputLayout inputLayout;
  inputLayout.setBindings({{kQuadVertexStride}});
  inputLayout.setAttributes(
      {{0, 0, QRhiVertexInputAttribute::Float2, 0}});
  pipeline->setVertexInputLayout(inputLayout);
  pipeline->setShaderResourceBindings(reduce ? mReduceLayoutBindings.get()
                                             : mResampleLayoutBindings.get());
  pipeline->setRenderPassDescriptor(entry.renderPass.get());
  if (pipeline->create())
    entry.pipeline = std::move(pipeline);
  else
    reportError(reduce ? u"Cannot create the exact downsample pipeline"_s
                       : u"Cannot create the resampling pipeline"_s);
  mPassPipelines.push_back(std::move(entry));
  return mPassPipelines.back().pipeline.get();
}

void ImageRenderer::releaseDeviceResources() {
  releaseTiles();
  mUploadedGeneration.reset();
  mPassPipelines.clear();
  mResampleLayoutBindings.reset();
  mResampleLayoutUniforms.reset();
  mResampleReady = false;
  mReduceLayoutBindings.reset();
  mReduceLayoutUniforms.reset();
  mReduceSampler.reset();
  mReduceReady = false;
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

bool ImageRenderer::generatesMipsByBoxReduce() const {
  // Qt 6.12's Direct3D 12 generateMips() computes mip levels in batches of
  // four; every level after the first batch (level 5 and below) comes out
  // wrong. The box-reduce pipeline produces the same 2 x 2 averages.
  return mRhi->backend() == QRhi::D3D12;
}

QRhiTexture::Flags ImageRenderer::tileTextureFlags() const {
  return generatesMipsByBoxReduce()
             ? QRhiTexture::Flags(QRhiTexture::MipMapped)
             : QRhiTexture::MipMapped | QRhiTexture::UsedWithGenerateMips;
}

bool ImageRenderer::createTile(GpuTile &tile, QRhiTexture::Format format) {
  tile.texture.reset(mRhi->newTexture(format, tile.region.texture.size(), 1,
                                      tileTextureFlags()));
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

  // The mip chain is generated by the GPU, or rendered by the box-reduce
  // pipeline (generatesMipsByBoxReduce()), so the format must support that.
  const TextureFormatSupport support{
      mRhi->isTextureFormatSupported(QRhiTexture::BGRA8),
      mRhi->isTextureFormatSupported(QRhiTexture::RGBA16F,
                                     tileTextureFlags()) &&
          (!generatesMipsByBoxReduce() ||
           mRhi->isTextureFormatSupported(QRhiTexture::RGBA16F,
                                          QRhiTexture::RenderTarget))};
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
    if (!generatesMipsByBoxReduce())
      updates->generateMips(tile.texture.get());
  }
  mTiles = std::move(tiles);
  mImageHasAlpha = image.hasAlphaChannel();
  mLastError.clear();
}

//------------------------------------------------------------------------------
// Records one offscreen pass into a new single-level texture of size `size`.
// The pass resources are released with deleteLater() as they are used by the
// frame being recorded; so is the returned texture unless the caller keeps
// it.
ImageRenderer::FrameTexture ImageRenderer::recordOffscreenPass(
    QRhiCommandBuffer *cb, const OffscreenPass &pass) {
  QRhiTexture *source = pass.source;
  const QSize size = pass.size;
  const QRhiTexture::Format format = source->format();
  FrameTexture texture(mRhi->newTexture(
      format, size, 1, QRhiTexture::RenderTarget | pass.extraFlags));
  if (!texture->create()) {
    reportError(u"Cannot create a %1 x %2 intermediate texture"_s.arg(
        size.width()).arg(size.height()));
    return {};
  }
  // Declared in dependency order, so that the release requests are issued
  // in reverse (bindings and target before the render pass descriptor).
  FrameResource<QRhiTextureRenderTarget> renderTarget(
      mRhi->newTextureRenderTarget(
          QRhiTextureRenderTargetDescription(QRhiColorAttachment(texture.get()))));
  FrameResource<QRhiRenderPassDescriptor> renderPass(
      renderTarget->newCompatibleRenderPassDescriptor());
  renderTarget->setRenderPassDescriptor(renderPass.get());
  FrameResource<QRhiBuffer> uniformBuffer(mRhi->newBuffer(
      QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, pass.uniformSize));
  if (!renderTarget->create() || !uniformBuffer->create()) {
    reportError(u"Cannot create an intermediate render target"_s);
    return {};
  }
  FrameResource<QRhiShaderResourceBindings> bindings(
      mRhi->newShaderResourceBindings());
  QVarLengthArray<QRhiShaderResourceBinding, 3> passBindings = {
      QRhiShaderResourceBinding::uniformBuffer(
          kUniformBinding,
          QRhiShaderResourceBinding::VertexStage |
              QRhiShaderResourceBinding::FragmentStage,
          uniformBuffer.get()),
      QRhiShaderResourceBinding::sampledTexture(
          kTextureBinding, QRhiShaderResourceBinding::FragmentStage, source,
          mReduceSampler.get())};
  if (pass.weights) {
    passBindings.append(QRhiShaderResourceBinding::sampledTexture(
        kWeightTableBinding, QRhiShaderResourceBinding::FragmentStage,
        pass.weights, mReduceSampler.get()));
  }
  bindings->setBindings(passBindings.cbegin(), passBindings.cend());
  if (!bindings->create()) {
    reportError(u"Cannot create intermediate pass shader bindings"_s);
    return {};
  }
  QRhiGraphicsPipeline *pipeline =
      passPipeline(pass.kind, format, renderPass.get());
  if (!pipeline)
    return {};

  QRhiResourceUpdateBatch *updates = mRhi->nextResourceUpdateBatch();
  if (!updates) {
    reportError(u"No QRhi resource update batch is available"_s);
    return {};
  }
  updates->updateDynamicBuffer(uniformBuffer.get(), 0, pass.uniformSize,
                               pass.uniforms);

  cb->beginPass(renderTarget.get(), kReduceClearColor,
                {kClearDepth, kClearStencil}, updates);
  cb->setGraphicsPipeline(pipeline);
  cb->setViewport(QRhiViewport(0.0f, 0.0f, static_cast<float>(size.width()),
                               static_cast<float>(size.height())));
  cb->setShaderResources(bindings.get());
  const QRhiCommandBuffer::VertexInput vertexInput(mVertexBuffer.get(), 0);
  cb->setVertexInput(0, 1, &vertexInput);
  cb->draw(kQuadVertexCount);
  cb->endPass();
  return texture;
}

// Records one exact-area box pass that reduces level 0 of source
// (sourceSize) into a new single-level texture of size `size`.
ImageRenderer::FrameTexture
ImageRenderer::recordBoxPass(QRhiCommandBuffer *cb, QRhiTexture *source,
                             QSize sourceSize, QSize size,
                             QRhiTexture::Flags extraFlags) {
  ReduceUniforms uniforms;
  const QMatrix4x4 mvp = reduceProjection(mRhi, size);
  std::memcpy(uniforms.mvp, mvp.constData(), sizeof(uniforms.mvp));
  uniforms.srcTexelSize[0] = 1.0f / static_cast<float>(sourceSize.width());
  uniforms.srcTexelSize[1] = 1.0f / static_cast<float>(sourceSize.height());
  uniforms.dstSize[0] = static_cast<float>(size.width());
  uniforms.dstSize[1] = static_cast<float>(size.height());
  uniforms.ratio[0] =
      static_cast<float>(size.width()) / static_cast<float>(sourceSize.width());
  uniforms.ratio[1] = static_cast<float>(size.height()) /
                      static_cast<float>(sourceSize.height());
  return recordOffscreenPass(
      cb, OffscreenPass{PassKind::BoxReduce, source, nullptr, size, &uniforms,
                        sizeof(uniforms), extraFlags});
}

//------------------------------------------------------------------------------
// Uploads a weight table of one resampling pass into a new R32F texture; the
// upload is recorded on cb before the pass that reads it.
ImageRenderer::FrameTexture
ImageRenderer::uploadWeightTable(QRhiCommandBuffer *cb, QSize size,
                                 const std::vector<float> &values) {
  FrameTexture texture(mRhi->newTexture(QRhiTexture::R32F, size));
  if (!texture->create()) {
    reportError(u"Cannot create a %1 x %2 resampling weight table"_s.arg(
        size.width()).arg(size.height()));
    return {};
  }
  QRhiResourceUpdateBatch *updates = mRhi->nextResourceUpdateBatch();
  if (!updates) {
    reportError(u"No QRhi resource update batch is available"_s);
    return {};
  }
  const QRhiTextureSubresourceUploadDescription data(
      values.data(), static_cast<quint32>(values.size() * sizeof(float)));
  updates->uploadTexture(texture.get(), QRhiTextureUploadEntry(0, 0, data));
  cb->resourceUpdate(updates);
  return texture;
}

//------------------------------------------------------------------------------
// Builds tile.reduced as a chain of box-reduce passes: each pass reduces an
// axis that has not reached its target by at most half, so the last pass per
// axis resamples the remaining fractional ratio.
void ImageRenderer::buildReducedTile(QRhiCommandBuffer *cb, GpuTile &tile,
                                     qreal scale) {
  tile.reducedScale = scale;
  tile.reduced.reset();
  tile.reducedBindings.reset();

  const QSize target = reducedSize(tile.region.texture.size(), scale);
  QSize current = tile.region.texture.size();
  if (target == current)
    return;

  FrameTexture result;
  while (current != target) {
    const QSize next(nextReduceStep(current.width(), target.width()),
                     nextReduceStep(current.height(), target.height()));
    QRhiTexture *source = result ? result.get() : tile.texture.get();
    FrameTexture reduced = recordBoxPass(cb, source, current, next);
    if (!reduced)
      return;
    // The previous step is released once the frame no longer uses it.
    result = std::move(reduced);
    current = next;
  }

  auto bindings = std::unique_ptr<QRhiShaderResourceBindings>(
      mRhi->newShaderResourceBindings());
  bindings->setBindings(
      {QRhiShaderResourceBinding::uniformBuffer(
           kUniformBinding,
           QRhiShaderResourceBinding::VertexStage |
               QRhiShaderResourceBinding::FragmentStage,
           tile.uniforms.get()),
       QRhiShaderResourceBinding::sampledTexture(
           kTextureBinding, QRhiShaderResourceBinding::FragmentStage,
           result.get(),
           mSamplers[samplingIndex(RenderEnums::TextureSampling::Bilinear)]
               .get())});
  if (!bindings->create()) {
    reportError(u"Cannot create downsample shader bindings"_s);
    return;
  }
  tile.reduced.reset(result.release());
  tile.reducedBindings = std::move(bindings);
}

//------------------------------------------------------------------------------
// Builds tile.resampled for the outputs `outputs` of the image shown at
// scale: a horizontal MKS2021 pass from the tile texture into an
// intermediate texture that holds every source row the vertical pass reads,
// then the vertical pass. Taps are clamped to the tile's texture, which
// matches the CPU's clamp to the image at the image edges.
void ImageRenderer::buildResampledTile(QRhiCommandBuffer *cb, GpuTile &tile,
                                       qreal scale, const QRect &outputs) {
  tile.resampledScale = scale;
  tile.resampledOutputs = outputs;
  tile.resampled.reset();
  tile.resampledBindings.reset();

  const QSize imageSize = mFrame.image->size();
  const QSize outputSize = ResampleGrid::outputSize(imageSize, scale);
  const Mks2021Axis xAxis{imageSize.width(), outputSize.width()};
  const Mks2021Axis yAxis{imageSize.height(), outputSize.height()};
  const QRect &texture = tile.region.texture;
  const int firstRow = std::clamp(yAxis.firstTap(outputs.top()),
                                  texture.top(), texture.bottom());
  const int lastRow = std::clamp(yAxis.lastTap(outputs.bottom()),
                                 texture.top(), texture.bottom());

  const QSize horizontalSize(outputs.width(), lastRow - firstRow + 1);
  const ResampleWeights columns =
      resampleWeights(xAxis, outputs.left(), outputs.width(), texture.left());
  // Every vertical tap clamped to the texture lies in [firstRow, lastRow]
  // (the clamp is monotonic), so clamping to the intermediate rows is the
  // same clamp.
  const ResampleWeights rowWeights =
      resampleWeights(yAxis, outputs.top(), outputs.height(), firstRow);
  // Far below 1:1 the kernel spans more source pixels than a texture may
  // be high; the mip chain is drawn instead.
  const int textureSizeMax = mRhi->resourceLimit(QRhi::TextureSizeMax);
  if (columns.size.height() > textureSizeMax ||
      rowWeights.size.height() > textureSizeMax)
    return;
  const FrameTexture columnTable =
      uploadWeightTable(cb, columns.size, columns.values);
  const FrameTexture rowTable =
      uploadWeightTable(cb, rowWeights.size, rowWeights.values);
  if (!columnTable || !rowTable)
    return;

  ResampleUniforms horizontal;
  const QMatrix4x4 horizontalMvp = reduceProjection(mRhi, horizontalSize);
  std::memcpy(horizontal.mvp, horizontalMvp.constData(),
              sizeof(horizontal.mvp));
  horizontal.dstSize[0] = static_cast<float>(horizontalSize.width());
  horizontal.dstSize[1] = static_cast<float>(horizontalSize.height());
  horizontal.axis = ResampleUniforms::kAxisHorizontal;
  horizontal.clampMin = 0;
  horizontal.clampMax = texture.width() - 1;
  horizontal.crossOffset = firstRow - texture.top();
  horizontal.tapCount = columns.taps;
  FrameTexture rows = recordOffscreenPass(
      cb, OffscreenPass{PassKind::Resample, tile.texture.get(),
                        columnTable.get(), horizontalSize, &horizontal,
                        sizeof(horizontal)});
  if (!rows)
    return;

  ResampleUniforms vertical;
  const QMatrix4x4 verticalMvp = reduceProjection(mRhi, outputs.size());
  std::memcpy(vertical.mvp, verticalMvp.constData(), sizeof(vertical.mvp));
  vertical.dstSize[0] = static_cast<float>(outputs.width());
  vertical.dstSize[1] = static_cast<float>(outputs.height());
  vertical.axis = ResampleUniforms::kAxisVertical;
  vertical.clampMin = 0;
  vertical.clampMax = lastRow - firstRow;
  vertical.crossOffset = 0;
  vertical.finalPass = ResampleUniforms::kFinalPass;
  vertical.tapCount = rowWeights.taps;
  FrameTexture result = recordOffscreenPass(
      cb, OffscreenPass{PassKind::Resample, rows.get(), rowTable.get(),
                        outputs.size(), &vertical, sizeof(vertical)});
  if (!result)
    return;

  auto bindings = std::unique_ptr<QRhiShaderResourceBindings>(
      mRhi->newShaderResourceBindings());
  bindings->setBindings(
      {QRhiShaderResourceBinding::uniformBuffer(
           kUniformBinding,
           QRhiShaderResourceBinding::VertexStage |
               QRhiShaderResourceBinding::FragmentStage,
           tile.uniforms.get()),
       QRhiShaderResourceBinding::sampledTexture(
           kTextureBinding, QRhiShaderResourceBinding::FragmentStage,
           result.get(),
           mSamplers[samplingIndex(RenderEnums::TextureSampling::Nearest)]
               .get())});
  if (!bindings->create()) {
    reportError(u"Cannot create resampling shader bindings"_s);
    return;
  }
  tile.resampled.reset(result.release());
  tile.resampledBindings = std::move(bindings);
}

//------------------------------------------------------------------------------
// Fills mip levels 1..n of tile.texture with exact-area box reductions of
// the previous level (a 2 x 2 average for even sizes, like generateMips()).
// Each level is rendered into a separate target and copied into the mip
// level through copies; a texture cannot be sampled and rendered to in the
// same pass.
void ImageRenderer::generateTileMips(QRhiCommandBuffer *cb, GpuTile &tile,
                                     QRhiResourceUpdateBatch *copies) {
  const QSize baseSize = tile.texture->pixelSize();
  const int levels = mRhi->mipLevelsForSize(baseSize);
  QRhiTexture *source = tile.texture.get();
  QSize sourceSize = baseSize;
  FrameTexture previous;
  for (int level = 1; level < levels; ++level) {
    const QSize size = mRhi->sizeForMipLevel(level, baseSize);
    FrameTexture reduced = recordBoxPass(cb, source, sourceSize, size,
                                         QRhiTexture::UsedAsTransferSource);
    if (!reduced)
      return;
    QRhiTextureCopyDescription copy;
    copy.setDestinationLevel(level);
    copies->copyTexture(tile.texture.get(), reduced.get(), copy);
    source = reduced.get();
    sourceSize = size;
    previous = std::move(reduced);
  }
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
  // Passes recorded before the main pass (mip levels, exact downsample) read
  // the uploaded tiles, so pending uploads are submitted before them.
  bool updatesHaveUploads = false;
  const auto submitUploads = [&]() {
    if (!updatesHaveUploads)
      return true;
    cb->resourceUpdate(updates);
    updatesHaveUploads = false;
    updates = mRhi->nextResourceUpdateBatch();
    if (!updates)
      reportError(u"No QRhi resource update batch is available"_s);
    return updates != nullptr;
  };

  if (mPipeline && needsUpload()) {
    uploadImage(updates);
    updatesHaveUploads = !mTiles.empty();
    if (generatesMipsByBoxReduce() && !mTiles.empty()) {
      if (!submitUploads())
        return;
      QRhiResourceUpdateBatch *copies = mRhi->nextResourceUpdateBatch();
      if (!copies) {
        reportError(u"No QRhi resource update batch is available"_s);
        return;
      }
      for (GpuTile &tile : mTiles)
        generateTileMips(cb, tile, copies);
      cb->resourceUpdate(copies);
    }
  }

  const QSize targetSize = target->pixelSize();
  std::vector<TileDraw> draws;
  if (mPipeline && !mTiles.empty() && mFrame.placement.scale > 0.0) {
    const ImageGeometry geometry = imageGeometry(mFrame);
    const FrameFilter filter =
        frameFilter(mFrame, geometry.scale, mReduceReady, mResampleReady);
    const QSize imageSize = mFrame.image ? mFrame.image->size() : QSize();
    const QSize outputSize = ResampleGrid::outputSize(imageSize, geometry.scale);
    const QPoint outputOrigin = geometry.origin.toPoint();

    struct VisibleTile {
      GpuTile *tile;
      TileRegion region;
      // Visible outputs of the tile in the resampling grid; empty unless the
      // frame is resampled.
      QRect outputs;
    };
    std::vector<VisibleTile> visible;
    for (GpuTile &tile : mTiles) {
      const auto region = visibleTileRegion(tile.region, geometry, targetSize);
      if (!region)
        continue;
      QRect outputs;
      if (filter.resample) {
        outputs = ResampleGrid::visibleTileOutputs(
            tile.region, imageSize, outputSize, outputOrigin, targetSize);
        // The tile owns no visible output centre; its neighbours draw the
        // pixels it touches.
        if (outputs.isEmpty())
          continue;
      }
      visible.push_back({&tile, *region, outputs});
    }
    for (VisibleTile &entry : visible) {
      GpuTile &tile = *entry.tile;
      if (filter.resample) {
        if (tile.resampledScale == geometry.scale &&
            tile.resampledOutputs == entry.outputs)
          continue;
        if (!submitUploads())
          return;
        buildResampledTile(cb, tile, geometry.scale, entry.outputs);
        continue;
      }
      if (!filter.exactReduce || tile.reducedScale == geometry.scale)
        continue;
      if (!submitUploads())
        return;
      buildReducedTile(cb, tile, geometry.scale);
    }

    QMatrix4x4 mvp = mRhi->clipSpaceCorrMatrix();
    mvp.ortho(0.0f, static_cast<float>(targetSize.width()),
              static_cast<float>(targetSize.height()), 0.0f, -1.0f, 1.0f);
    TileUniforms uniforms =
        frameUniforms(mFrame, geometry, mImageHasAlpha, filter, mvp);
    for (const VisibleTile &entry : visible) {
      const GpuTile &tile = *entry.tile;
      DrawSource source = DrawSource::MipChain;
      if (filter.resample && tile.resampled &&
          tile.resampledScale == geometry.scale &&
          tile.resampledOutputs == entry.outputs)
        source = DrawSource::Resampled;
      else if (filter.exactReduce && tile.reduced &&
               tile.reducedScale == geometry.scale)
        source = DrawSource::Reduced;

      QSizeF stepTexels;
      if (source == DrawSource::Resampled) {
        // The resampled outputs are whole device pixels: drawn 1:1.
        setRect(uniforms.targetRect,
                QRectF(entry.outputs.translated(outputOrigin)));
        setRect(uniforms.texRect, QRectF(0.0, 0.0, 1.0, 1.0));
        stepTexels = QSizeF(tile.resampled->pixelSize());
      } else {
        setRect(uniforms.targetRect, entry.region.target);
        setRect(uniforms.texRect, entry.region.texture);
        // One device pixel in texture coordinates: the reduced texture is
        // already at device resolution; the mip chain is at source
        // resolution.
        stepTexels = source == DrawSource::Reduced
                         ? QSizeF(tile.reduced->pixelSize())
                         : QSizeF(tile.region.texture.size()) * geometry.scale;
      }
      uniforms.texelStep[0] = static_cast<float>(1.0 / stepTexels.width());
      uniforms.texelStep[1] = static_cast<float>(1.0 / stepTexels.height());
      uniforms.downscaleTaps =
          filter.downscaling && source == DrawSource::MipChain
              ? TileUniforms::kEnabled
              : TileUniforms::kDisabled;
      updates->updateDynamicBuffer(tile.uniforms.get(), 0,
                                   sizeof(TileUniforms), &uniforms);
      draws.push_back({&tile, source});
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
    for (const TileDraw &draw : draws) {
      QRhiShaderResourceBindings *bindings = draw.tile->bindings[sampling].get();
      if (draw.source == DrawSource::Reduced)
        bindings = draw.tile->reducedBindings.get();
      else if (draw.source == DrawSource::Resampled)
        bindings = draw.tile->resampledBindings.get();
      cb->setShaderResources(bindings);
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
