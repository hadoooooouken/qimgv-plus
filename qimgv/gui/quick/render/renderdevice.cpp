#include "renderdevice.h"

#include <QFile>
#include <QLatin1StringView>
#include <QVarLengthArray>
#include <cstring>
#include <utility>

#include "gui/quick/render/rendererrorreporter.h"
#include "gui/quick/render/renderstatistics.h"
#include "gui/quick/render/rhipassuniforms.h"

namespace {
using namespace Qt::StringLiterals;

constexpr QLatin1StringView kImageVertexShaderPath =
    ":/qimgv/render/shaders/image.vert.qsb"_L1;
constexpr QLatin1StringView kImageFragmentShaderPath =
    ":/qimgv/render/shaders/image.frag.qsb"_L1;
constexpr QLatin1StringView kPanoramaVertexShaderPath =
    ":/qimgv/render/shaders/panorama.vert.qsb"_L1;
constexpr QLatin1StringView kPanoramaFragmentShaderPath =
    ":/qimgv/render/shaders/panorama.frag.qsb"_L1;
constexpr QLatin1StringView kReduceVertexShaderPath =
    ":/qimgv/render/shaders/boxreduce.vert.qsb"_L1;
constexpr QLatin1StringView kReduceFragmentShaderPath =
    ":/qimgv/render/shaders/boxreduce.frag.qsb"_L1;
constexpr QLatin1StringView kResampleVertexShaderPath =
    ":/qimgv/render/shaders/resample.vert.qsb"_L1;
constexpr QLatin1StringView kResampleFragmentShaderPath =
    ":/qimgv/render/shaders/resample.frag.qsb"_L1;
constexpr QLatin1StringView kConvertVertexShaderPath =
    ":/qimgv/render/shaders/convert.vert.qsb"_L1;
constexpr QLatin1StringView kConvertFragmentShaderPath =
    ":/qimgv/render/shaders/convert.frag.qsb"_L1;

// Unit quad as a triangle strip; every vertex shader stretches it over its
// target.
constexpr std::array<float, 8> kQuadCorners = {0.0f, 0.0f, 1.0f, 0.0f,
                                               0.0f, 1.0f, 1.0f, 1.0f};
constexpr quint32 kQuadVertexCount = 4;
constexpr quint32 kQuadVertexStride = 2 * sizeof(float);

// Intermediate targets are fully overwritten.
const QColor kOffscreenClearColor = Qt::transparent;
constexpr float kClearDepth = 1.0f;
constexpr quint32 kClearStencil = 0;

constexpr QSize kLayoutTextureSize(1, 1);
constexpr int kLayoutLutDepth = 1;
constexpr int kNoLut = 0;
} // namespace

std::size_t samplingIndex(RenderEnums::TextureSampling sampling) {
  return static_cast<std::size_t>(sampling);
}

//------------------------------------------------------------------------------
RenderDevice::RenderDevice(RenderErrorReporter &errors) : mErrors(errors) {}

// QRhi resources are released by their unique_ptr members on the render
// thread, before the QRhi.
RenderDevice::~RenderDevice() = default;

bool RenderDevice::create(QRhi *rhi, QRhiCommandBuffer *cb) {
  release();
  mRhi = rhi;
  if (!mRhi) {
    reportError(u"No QRhi is available for the image renderer"_s);
    return false;
  }
  mReady = createBaseResources(cb);
  mReduceReady = mReady && createReduceResources();
  mResampleReady = mReduceReady && createResampleResources();
  mConvertReady = mReduceReady && createConvertResources();
  return mReady;
}

void RenderDevice::release() {
  mPassPipelines.clear();
  mLutTexture.reset();
  mLutData.reset();
  mConvertLayoutBindings.reset();
  mConvertLayoutUniforms.reset();
  mLayoutLut.reset();
  mLutSampler.reset();
  mConvertReady = false;
  mResampleLayoutBindings.reset();
  mResampleLayoutUniforms.reset();
  mResampleReady = false;
  mReduceLayoutBindings.reset();
  mReduceLayoutUniforms.reset();
  mExactSampler.reset();
  mReduceReady = false;
  mLayoutBindings.reset();
  mLayoutUniforms.reset();
  mLayoutTexture.reset();
  for (auto &sampler : mSamplers)
    sampler.reset();
  for (auto &sampler : mWrappingSamplers)
    sampler.reset();
  mVertexBuffer.reset();
  mReady = false;
  mRhi = nullptr;
}

QRhi *RenderDevice::rhi() const { return mRhi; }
bool RenderDevice::isReady() const { return mReady; }
bool RenderDevice::hasReducePass() const { return mReduceReady; }
bool RenderDevice::hasResamplePass() const { return mResampleReady; }
bool RenderDevice::hasConvertPass() const { return mConvertReady; }

const RenderDevice::MainShaders &RenderDevice::shaders() const {
  return mShaders;
}

QRhiShaderResourceBindings *RenderDevice::layoutBindings() const {
  return mLayoutBindings.get();
}

QRhiSampler *
RenderDevice::sampler(RenderEnums::TextureSampling sampling) const {
  return mSamplers[samplingIndex(sampling)].get();
}

QRhiSampler *
RenderDevice::wrappingSampler(RenderEnums::TextureSampling sampling) const {
  return mWrappingSamplers[samplingIndex(sampling)].get();
}

QRhiSampler *RenderDevice::exactSampler() const { return mExactSampler.get(); }

QRhiTexture *RenderDevice::placeholderLut() const { return mLayoutLut.get(); }

void RenderDevice::drawQuad(QRhiCommandBuffer *cb) const {
  const QRhiCommandBuffer::VertexInput vertexInput(mVertexBuffer.get(), 0);
  cb->setVertexInput(0, 1, &vertexInput);
  cb->draw(kQuadVertexCount);
}

void RenderDevice::reportError(const QString &message) {
  mErrors.report(message);
}

void RenderDevice::clearErrorRepeatGuard() { mErrors.clearRepeatGuard(); }

void RenderDevice::setStatistics(std::shared_ptr<RenderStatistics> statistics) {
  mStatistics = std::move(statistics);
}

void RenderDevice::countImageTextureCreated() {
  if (mStatistics)
    mStatistics->imageTexturesCreated.fetch_add(1, std::memory_order_relaxed);
}

void RenderDevice::countImageUpload() {
  if (mStatistics)
    mStatistics->imageUploads.fetch_add(1, std::memory_order_relaxed);
}

//------------------------------------------------------------------------------
bool RenderDevice::loadShader(const QString &path, QShader &shader) {
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

bool RenderDevice::createBaseResources(QRhiCommandBuffer *cb) {
  if (!loadShader(kImageVertexShaderPath, mShaders.imageVertex) ||
      !loadShader(kImageFragmentShaderPath, mShaders.imageFragment) ||
      !loadShader(kPanoramaVertexShaderPath, mShaders.panoramaVertex) ||
      !loadShader(kPanoramaFragmentShaderPath, mShaders.panoramaFragment))
    return false;

  mVertexBuffer.reset(mRhi->newBuffer(QRhiBuffer::Immutable,
                                      QRhiBuffer::VertexBuffer,
                                      sizeof(kQuadCorners)));
  if (!mVertexBuffer->create()) {
    reportError(u"Cannot create the image quad vertex buffer"_s);
    return false;
  }

  using Filter = QRhiSampler::Filter;
  const auto createSamplers = [this](RenderEnums::TextureSampling sampling,
                                     Filter filter, Filter mipmapMode) {
    auto &clamping = mSamplers[samplingIndex(sampling)];
    clamping.reset(mRhi->newSampler(filter, filter, mipmapMode,
                                    QRhiSampler::ClampToEdge,
                                    QRhiSampler::ClampToEdge));
    auto &wrapping = mWrappingSamplers[samplingIndex(sampling)];
    wrapping.reset(mRhi->newSampler(filter, filter, mipmapMode,
                                    QRhiSampler::Repeat,
                                    QRhiSampler::ClampToEdge));
    return clamping->create() && wrapping->create();
  };
  if (!createSamplers(RenderEnums::TextureSampling::Nearest, Filter::Nearest,
                      Filter::None) ||
      !createSamplers(RenderEnums::TextureSampling::Bilinear, Filter::Linear,
                      Filter::None) ||
      !createSamplers(RenderEnums::TextureSampling::Trilinear, Filter::Linear,
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
           sampler(RenderEnums::TextureSampling::Nearest))});
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

bool RenderDevice::createReduceResources() {
  if (!loadShader(kReduceVertexShaderPath, mReduceVertexShader) ||
      !loadShader(kReduceFragmentShaderPath, mReduceFragmentShader))
    return false;

  // Reads exact texels of mip level 0; boxreduce.frag does the weighting.
  mExactSampler.reset(mRhi->newSampler(
      QRhiSampler::Nearest, QRhiSampler::Nearest, QRhiSampler::None,
      QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge));
  mReduceLayoutUniforms.reset(mRhi->newBuffer(
      QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(ReduceUniforms)));
  if (!mExactSampler->create() || !mReduceLayoutUniforms->create()) {
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
           mLayoutTexture.get(), mExactSampler.get())});
  if (!mReduceLayoutBindings->create()) {
    reportError(u"Cannot create the exact downsample layout bindings"_s);
    return false;
  }
  return true;
}

bool RenderDevice::createResampleResources() {
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
  // resample.frag reads exact texels with texelFetch(); the exact sampler
  // only completes the combined image sampler binding.
  mResampleLayoutBindings.reset(mRhi->newShaderResourceBindings());
  mResampleLayoutBindings->setBindings(
      {QRhiShaderResourceBinding::uniformBuffer(
           kUniformBinding,
           QRhiShaderResourceBinding::VertexStage |
               QRhiShaderResourceBinding::FragmentStage,
           mResampleLayoutUniforms.get()),
       QRhiShaderResourceBinding::sampledTexture(
           kTextureBinding, QRhiShaderResourceBinding::FragmentStage,
           mLayoutTexture.get(), mExactSampler.get()),
       QRhiShaderResourceBinding::sampledTexture(
           kWeightTableBinding, QRhiShaderResourceBinding::FragmentStage,
           mLayoutTexture.get(), mExactSampler.get())});
  if (!mResampleLayoutBindings->create()) {
    reportError(u"Cannot create the resampling layout bindings"_s);
    return false;
  }
  return true;
}

bool RenderDevice::createConvertResources() {
  if (!loadShader(kConvertVertexShaderPath, mConvertVertexShader) ||
      !loadShader(kConvertFragmentShaderPath, mConvertFragmentShader))
    return false;
  if (!mRhi->isFeatureSupported(QRhi::ThreeDimensionalTextures) ||
      !mRhi->isTextureFormatSupported(QRhiTexture::RGBA16F,
                                      QRhiTexture::ThreeDimensional)) {
    reportError(u"The GPU has no RGBA16F 3D textures; HDR tone mapping and "
                u"colour management are not available"_s);
    return false;
  }

  // Trilinear interpolation between the lattice points of the lookup table.
  mLutSampler.reset(mRhi->newSampler(
      QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
      QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge,
      QRhiSampler::ClampToEdge));
  mLayoutLut.reset(mRhi->newTexture(
      QRhiTexture::RGBA16F, kLayoutTextureSize.width(),
      kLayoutTextureSize.height(), kLayoutLutDepth, 1,
      QRhiTexture::ThreeDimensional));
  mConvertLayoutUniforms.reset(mRhi->newBuffer(
      QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(ConvertUniforms)));
  if (!mLutSampler->create() || !mLayoutLut->create() ||
      !mConvertLayoutUniforms->create()) {
    reportError(u"Cannot create the colour conversion resources"_s);
    return false;
  }
  // convert.frag reads exact texels with texelFetch(); the exact sampler
  // only completes the combined image sampler binding.
  mConvertLayoutBindings.reset(mRhi->newShaderResourceBindings());
  mConvertLayoutBindings->setBindings(
      {QRhiShaderResourceBinding::uniformBuffer(
           kUniformBinding,
           QRhiShaderResourceBinding::VertexStage |
               QRhiShaderResourceBinding::FragmentStage,
           mConvertLayoutUniforms.get()),
       QRhiShaderResourceBinding::sampledTexture(
           kTextureBinding, QRhiShaderResourceBinding::FragmentStage,
           mLayoutTexture.get(), mExactSampler.get()),
       QRhiShaderResourceBinding::sampledTexture(
           kColorLutBinding, QRhiShaderResourceBinding::FragmentStage,
           mLayoutLut.get(), mLutSampler.get())});
  if (!mConvertLayoutBindings->create()) {
    reportError(u"Cannot create the colour conversion layout bindings"_s);
    return false;
  }
  return true;
}

QRhiGraphicsPipeline *
RenderDevice::passPipeline(PassKind kind, QRhiTexture::Format format,
                           QRhiRenderPassDescriptor *compatiblePass) {
  for (const PassPipeline &entry : mPassPipelines) {
    if (entry.kind == kind && entry.format == format)
      return entry.pipeline.get();
  }

  PassPipeline entry;
  entry.kind = kind;
  entry.format = format;
  entry.renderPass.reset(compatiblePass->newCompatibleRenderPassDescriptor());
  auto pipeline = std::unique_ptr<QRhiGraphicsPipeline>(
      mRhi->newGraphicsPipeline());
  // Every destination texel is written once; no blending.
  pipeline->setTopology(QRhiGraphicsPipeline::TriangleStrip);
  const QShader *vertexShader = &mReduceVertexShader;
  const QShader *fragmentShader = &mReduceFragmentShader;
  QRhiShaderResourceBindings *layoutBindings = mReduceLayoutBindings.get();
  QString failure = u"Cannot create the exact downsample pipeline"_s;
  if (kind == PassKind::Resample) {
    vertexShader = &mResampleVertexShader;
    fragmentShader = &mResampleFragmentShader;
    layoutBindings = mResampleLayoutBindings.get();
    failure = u"Cannot create the resampling pipeline"_s;
  } else if (kind == PassKind::Convert) {
    vertexShader = &mConvertVertexShader;
    fragmentShader = &mConvertFragmentShader;
    layoutBindings = mConvertLayoutBindings.get();
    failure = u"Cannot create the colour conversion pipeline"_s;
  }
  pipeline->setShaderStages({{QRhiShaderStage::Vertex, *vertexShader},
                             {QRhiShaderStage::Fragment, *fragmentShader}});
  QRhiVertexInputLayout inputLayout;
  inputLayout.setBindings({{kQuadVertexStride}});
  inputLayout.setAttributes(
      {{0, 0, QRhiVertexInputAttribute::Float2, 0}});
  pipeline->setVertexInputLayout(inputLayout);
  pipeline->setShaderResourceBindings(layoutBindings);
  pipeline->setRenderPassDescriptor(entry.renderPass.get());
  if (pipeline->create())
    entry.pipeline = std::move(pipeline);
  else
    reportError(failure);
  mPassPipelines.push_back(std::move(entry));
  return mPassPipelines.back().pipeline.get();
}

//------------------------------------------------------------------------------
bool RenderDevice::generatesMipsByBoxReduce() const {
  // Qt 6.12's Direct3D 12 generateMips() computes mip levels in batches of
  // four; every level after the first batch (level 5 and below) comes out
  // wrong. The box-reduce pipeline produces the same 2 x 2 averages.
  return mRhi->backend() == QRhi::D3D12;
}

QRhiTexture::Flags RenderDevice::imageTextureFlags() const {
  return generatesMipsByBoxReduce()
             ? QRhiTexture::Flags(QRhiTexture::MipMapped)
             : QRhiTexture::MipMapped | QRhiTexture::UsedWithGenerateMips;
}

// Box-reduce variant: fills mip levels 1..n with exact-area reductions of the
// previous level (a 2 x 2 average for even sizes, like generateMips()). Each
// level is rendered into a separate target and copied into the mip level; a
// texture cannot be sampled and rendered to in the same pass.
void RenderDevice::generateMips(QRhiCommandBuffer *cb, QRhiTexture *texture,
                                QRhiResourceUpdateBatch *updates) {
  if (!generatesMipsByBoxReduce()) {
    updates->generateMips(texture);
    return;
  }
  const QSize baseSize = texture->pixelSize();
  const int levels = mRhi->mipLevelsForSize(baseSize);
  QRhiTexture *source = texture;
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
    updates->copyTexture(texture, reduced.get(), copy);
    source = reduced.get();
    sourceSize = size;
    previous = std::move(reduced);
  }
}

//------------------------------------------------------------------------------
QRhiTexture *RenderDevice::colorLut(QRhiCommandBuffer *cb,
                                    const SourceConversion &conversion,
                                    int *lutSize) {
  *lutSize = kNoLut;
  if (conversion.color.kind != ColorTransformKind::Lut || !conversion.lut)
    return mLayoutLut.get();
  if (mLutTexture && mLutData == conversion.lut) {
    *lutSize = mLutData->size;
    return mLutTexture.get();
  }

  mLutTexture.reset();
  mLutData.reset();
  const ColorLut &lut = *conversion.lut;
  const std::size_t sliceValues =
      static_cast<std::size_t>(lut.size) * lut.size * ColorLut::kChannels;
  if (lut.size <= 0 ||
      lut.halfRgba.size() != sliceValues * static_cast<std::size_t>(lut.size)) {
    reportError(u"The colour lookup table has an invalid size"_s);
    return nullptr;
  }
  auto texture = std::unique_ptr<QRhiTexture>(
      mRhi->newTexture(QRhiTexture::RGBA16F, lut.size, lut.size, lut.size, 1,
                       QRhiTexture::ThreeDimensional));
  if (!texture->create()) {
    reportError(u"Cannot create a %1^3 colour lookup table"_s.arg(lut.size));
    return nullptr;
  }
  QRhiResourceUpdateBatch *updates = mRhi->nextResourceUpdateBatch();
  if (!updates) {
    reportError(u"No QRhi resource update batch is available"_s);
    return nullptr;
  }
  // One upload entry per blue slice (the layer of a 3D texture).
  QList<QRhiTextureUploadEntry> slices;
  slices.reserve(lut.size);
  const quint32 sliceBytes =
      static_cast<quint32>(sliceValues * sizeof(quint16));
  for (int slice = 0; slice < lut.size; ++slice) {
    slices.append(QRhiTextureUploadEntry(
        slice, 0,
        QRhiTextureSubresourceUploadDescription(
            lut.halfRgba.data() + slice * sliceValues, sliceBytes)));
  }
  QRhiTextureUploadDescription description;
  description.setEntries(slices.cbegin(), slices.cend());
  updates->uploadTexture(texture.get(), description);
  cb->resourceUpdate(updates);
  mLutTexture = std::move(texture);
  mLutData = conversion.lut;
  *lutSize = mLutData->size;
  return mLutTexture.get();
}

//------------------------------------------------------------------------------
QMatrix4x4 RenderDevice::offscreenProjection(QSize size) const {
  QMatrix4x4 mvp = mRhi->clipSpaceCorrMatrix();
  const float width = static_cast<float>(size.width());
  const float height = static_cast<float>(size.height());
  if (mRhi->isYUpInFramebuffer())
    mvp.ortho(0.0f, width, 0.0f, height, -1.0f, 1.0f);
  else
    mvp.ortho(0.0f, width, height, 0.0f, -1.0f, 1.0f);
  return mvp;
}

// Records one offscreen pass into a new single-level texture of size `size`.
// The pass resources are released with deleteLater() as they are used by the
// frame being recorded; so is the returned texture unless the caller keeps
// it.
FrameTexture RenderDevice::recordOffscreenPass(QRhiCommandBuffer *cb,
                                               const OffscreenPass &pass) {
  const QSize size = pass.size;
  FrameTexture texture(mRhi->newTexture(
      pass.source->format(), size, 1,
      QRhiTexture::RenderTarget | pass.extraFlags));
  if (!texture->create()) {
    reportError(u"Cannot create a %1 x %2 intermediate texture"_s.arg(
        size.width()).arg(size.height()));
    return {};
  }
  if (!recordPassInto(cb, pass, texture.get()))
    return {};
  return texture;
}

bool RenderDevice::recordPassInto(QRhiCommandBuffer *cb,
                                  const OffscreenPass &pass,
                                  QRhiTexture *target) {
  const QSize size = target->pixelSize();
  const QRhiTexture::Format format = target->format();
  // Declared in dependency order, so that the release requests are issued
  // in reverse (bindings and target before the render pass descriptor).
  FrameResource<QRhiTextureRenderTarget> renderTarget(
      mRhi->newTextureRenderTarget(
          QRhiTextureRenderTargetDescription(QRhiColorAttachment(target))));
  FrameResource<QRhiRenderPassDescriptor> renderPass(
      renderTarget->newCompatibleRenderPassDescriptor());
  renderTarget->setRenderPassDescriptor(renderPass.get());
  FrameResource<QRhiBuffer> uniformBuffer(mRhi->newBuffer(
      QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, pass.uniformSize));
  if (!renderTarget->create() || !uniformBuffer->create()) {
    reportError(u"Cannot create an intermediate render target"_s);
    return false;
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
          kTextureBinding, QRhiShaderResourceBinding::FragmentStage,
          pass.source, mExactSampler.get())};
  if (pass.weights) {
    passBindings.append(QRhiShaderResourceBinding::sampledTexture(
        kWeightTableBinding, QRhiShaderResourceBinding::FragmentStage,
        pass.weights, mExactSampler.get()));
  }
  if (pass.colorLut) {
    passBindings.append(QRhiShaderResourceBinding::sampledTexture(
        kColorLutBinding, QRhiShaderResourceBinding::FragmentStage,
        pass.colorLut, mLutSampler.get()));
  }
  bindings->setBindings(passBindings.cbegin(), passBindings.cend());
  if (!bindings->create()) {
    reportError(u"Cannot create intermediate pass shader bindings"_s);
    return false;
  }
  QRhiGraphicsPipeline *pipeline =
      passPipeline(pass.kind, format, renderPass.get());
  if (!pipeline)
    return false;

  QRhiResourceUpdateBatch *updates = mRhi->nextResourceUpdateBatch();
  if (!updates) {
    reportError(u"No QRhi resource update batch is available"_s);
    return false;
  }
  updates->updateDynamicBuffer(uniformBuffer.get(), 0, pass.uniformSize,
                               pass.uniforms);

  cb->beginPass(renderTarget.get(), kOffscreenClearColor,
                {kClearDepth, kClearStencil}, updates);
  cb->setGraphicsPipeline(pipeline);
  cb->setViewport(QRhiViewport(0.0f, 0.0f, static_cast<float>(size.width()),
                               static_cast<float>(size.height())));
  cb->setShaderResources(bindings.get());
  drawQuad(cb);
  cb->endPass();
  return true;
}

FrameTexture RenderDevice::recordBoxPass(QRhiCommandBuffer *cb,
                                         QRhiTexture *source, QSize sourceSize,
                                         QSize size,
                                         QRhiTexture::Flags extraFlags) {
  ReduceUniforms uniforms;
  const QMatrix4x4 mvp = offscreenProjection(size);
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
      cb, OffscreenPass{PassKind::BoxReduce, source, nullptr, nullptr, size,
                        &uniforms, sizeof(uniforms), extraFlags});
}

// The upload is recorded on cb before the pass that reads the table.
FrameTexture RenderDevice::uploadWeightTable(QRhiCommandBuffer *cb, QSize size,
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
