#pragma once

#include <QMatrix4x4>
#include <QSize>
#include <QString>
#include <array>
#include <cstddef>
#include <memory>
#include <rhi/qrhi.h>
#include <rhi/qshader.h>
#include <vector>

#include "gui/quick/render/renderframe.h"

class RenderErrorReporter;
struct RenderStatistics;

// Releases a QRhi resource once the frame being recorded no longer uses it
// (QRhiResource::deleteLater()).
struct DeferredRhiRelease {
  void operator()(QRhiResource *resource) const { resource->deleteLater(); }
};
template <typename T>
using FrameResource = std::unique_ptr<T, DeferredRhiRelease>;
using FrameTexture = FrameResource<QRhiTexture>;

// Index of a RenderEnums::TextureSampling in per-sampling arrays.
inline constexpr std::size_t kSamplingCount = 3;
[[nodiscard]] std::size_t samplingIndex(RenderEnums::TextureSampling sampling);

// Resources of ImageRenderer that live as long as its QRhi: the shaders, the
// unit quad, the samplers, the binding layouts of the pipelines, the
// colour lookup table and the pipelines of the offscreen passes, and the
// recording of those passes (exact-area box reduce, MKS2021 resampling,
// source conversion, mip chains rendered by box reduce).
//
// The graphics pipelines of the item's own render pass (image tiles,
// panorama) depend on its render target and belong to ImageRenderer; they are
// built from shaders() and layoutBindings().
//
// Render thread only.
class RenderDevice {
public:
  // Offscreen passes.
  enum class PassKind { BoxReduce, Resample, Convert };

  // Shaders of the item's render pass.
  struct MainShaders {
    QShader imageVertex;
    QShader imageFragment;
    QShader panoramaVertex;
    QShader panoramaFragment;
  };

  // One offscreen pass: reads source (and, for resampling passes, the
  // weight table weights; for conversion passes, the colour lookup table
  // colorLut) and renders into a texture of the source's format and of size
  // `size`; uniforms (uniformSize bytes) fill the pass's uniform buffer.
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

  explicit RenderDevice(RenderErrorReporter &errors);
  ~RenderDevice();
  RenderDevice(const RenderDevice &) = delete;
  RenderDevice &operator=(const RenderDevice &) = delete;

  // Releases everything and creates the resources on rhi; uploads recorded
  // on cb. Returns false when the base resources (main shaders, quad,
  // samplers) fail; the optional passes report their own failure and are
  // then unavailable.
  bool create(QRhi *rhi, QRhiCommandBuffer *cb);
  void release();

  [[nodiscard]] QRhi *rhi() const;
  [[nodiscard]] bool isReady() const;
  [[nodiscard]] bool hasReducePass() const;
  [[nodiscard]] bool hasResamplePass() const;
  [[nodiscard]] bool hasConvertPass() const;

  [[nodiscard]] const MainShaders &shaders() const;
  // Binding layout of the image and panorama pipelines: a uniform buffer and
  // one sampled texture.
  [[nodiscard]] QRhiShaderResourceBindings *layoutBindings() const;
  // Clamping sampler of sampling.
  [[nodiscard]] QRhiSampler *sampler(RenderEnums::TextureSampling sampling) const;
  // Same filtering, repeating horizontally (panorama longitude).
  [[nodiscard]] QRhiSampler *
  wrappingSampler(RenderEnums::TextureSampling sampling) const;
  // Exact texels of level 0 (texelFetch and box passes).
  [[nodiscard]] QRhiSampler *exactSampler() const;

  // Binds the unit quad and draws it; inside a pass with a pipeline set.
  void drawQuad(QRhiCommandBuffer *cb) const;

  // Qt 6.12's Direct3D 12 generateMips() is wrong from mip level 5 on; the
  // box-reduce pipeline builds the mip chain there instead.
  [[nodiscard]] bool generatesMipsByBoxReduce() const;
  // Flags of a displayed image texture (mip-mapped).
  [[nodiscard]] QRhiTexture::Flags imageTextureFlags() const;
  // Records the generation of mip levels 1..n of texture: on updates
  // (generateMips()), or as box-reduce passes on cb whose copies into the
  // texture go through updates (generatesMipsByBoxReduce()). updates must be
  // submitted before the texture is sampled.
  void generateMips(QRhiCommandBuffer *cb, QRhiTexture *texture,
                    QRhiResourceUpdateBatch *updates);

  // The 3D lookup table of conversion, uploaded on first use and kept while
  // conversions use the same table; the layout placeholder when the
  // conversion uses none; null on failure (reported). lutSize receives the
  // side of the returned table (0 for the placeholder).
  [[nodiscard]] QRhiTexture *colorLut(QRhiCommandBuffer *cb,
                                      const SourceConversion &conversion,
                                      int *lutSize);
  [[nodiscard]] QRhiTexture *placeholderLut() const;

  // Records pass on cb into a new texture; null on failure (reported).
  [[nodiscard]] FrameTexture recordOffscreenPass(QRhiCommandBuffer *cb,
                                                 const OffscreenPass &pass);
  // Records pass on cb into level 0 of target (pass.size and
  // pass.extraFlags are not used); false on failure (reported).
  [[nodiscard]] bool recordPassInto(QRhiCommandBuffer *cb,
                                    const OffscreenPass &pass,
                                    QRhiTexture *target);
  // Records one exact-area pass from level 0 of source (sourceSize) into a
  // new texture of size `size`; null on failure (reported).
  [[nodiscard]] FrameTexture recordBoxPass(QRhiCommandBuffer *cb,
                                           QRhiTexture *source,
                                           QSize sourceSize, QSize size,
                                           QRhiTexture::Flags extraFlags = {});
  // Uploads a resampling weight table (size.width() floats per row) into a
  // new R32F texture, recorded on cb; null on failure (reported).
  [[nodiscard]] FrameTexture uploadWeightTable(QRhiCommandBuffer *cb,
                                               QSize size,
                                               const std::vector<float> &values);

  // Projection of an offscreen pass into a texture of size `size`:
  // destination texel row 0 lands in the texture row that is sampled at
  // v = 0 on every backend.
  [[nodiscard]] QMatrix4x4 offscreenProjection(QSize size) const;

  void reportError(const QString &message);
  // A step that failed before succeeded: its error is reported again if it
  // fails next time.
  void clearErrorRepeatGuard();
  // Counters of the item (synchronize()); may be null.
  void setStatistics(std::shared_ptr<RenderStatistics> statistics);
  void countImageTextureCreated();
  void countImageUpload();

private:
  // Pipeline of one kind of offscreen pass for one render target format.
  struct PassPipeline {
    PassKind kind = PassKind::BoxReduce;
    QRhiTexture::Format format = QRhiTexture::RGBA8;
    std::unique_ptr<QRhiRenderPassDescriptor> renderPass;
    // Null when its creation failed; not retried for this QRhi.
    std::unique_ptr<QRhiGraphicsPipeline> pipeline;
  };

  [[nodiscard]] bool loadShader(const QString &path, QShader &shader);
  [[nodiscard]] bool createBaseResources(QRhiCommandBuffer *cb);
  [[nodiscard]] bool createReduceResources();
  [[nodiscard]] bool createResampleResources();
  [[nodiscard]] bool createConvertResources();
  [[nodiscard]] QRhiGraphicsPipeline *
  passPipeline(PassKind kind, QRhiTexture::Format format,
               QRhiRenderPassDescriptor *compatiblePass);

  RenderErrorReporter &mErrors;
  std::shared_ptr<RenderStatistics> mStatistics;
  QRhi *mRhi = nullptr;
  bool mReady = false;
  bool mReduceReady = false;
  bool mResampleReady = false;
  bool mConvertReady = false;

  MainShaders mShaders;
  std::unique_ptr<QRhiBuffer> mVertexBuffer;
  std::array<std::unique_ptr<QRhiSampler>, kSamplingCount> mSamplers;
  std::array<std::unique_ptr<QRhiSampler>, kSamplingCount> mWrappingSamplers;
  // Placeholder resources that describe the binding layout of the pipelines.
  std::unique_ptr<QRhiTexture> mLayoutTexture;
  std::unique_ptr<QRhiBuffer> mLayoutUniforms;
  std::unique_ptr<QRhiShaderResourceBindings> mLayoutBindings;

  QShader mReduceVertexShader;
  QShader mReduceFragmentShader;
  std::unique_ptr<QRhiSampler> mExactSampler;
  std::unique_ptr<QRhiBuffer> mReduceLayoutUniforms;
  std::unique_ptr<QRhiShaderResourceBindings> mReduceLayoutBindings;

  QShader mResampleVertexShader;
  QShader mResampleFragmentShader;
  std::unique_ptr<QRhiBuffer> mResampleLayoutUniforms;
  std::unique_ptr<QRhiShaderResourceBindings> mResampleLayoutBindings;

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
};
