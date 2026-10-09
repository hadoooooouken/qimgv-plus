#include "imagelayer.h"

#include <QImage>
#include <algorithm>
#include <cmath>
#include <cstring>

#include "gui/quick/render/resamplegrid.h"
#include "gui/quick/render/rhipassuniforms.h"
#include "gui/quick/render/textureuploadformat.h"

namespace {
using namespace Qt::StringLiterals;

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

// Weight table of one resampling pass (resample.frag): column d describes
// destination index d = output firstOutput + d, with its first source texel
// (relative to the source texture's texel 0, source index sourceBase) in row
// 0 and its normalized weights in rows 1..taps, zero-padded.
constexpr int kWeightTableHeaderRows = 1;

// HDR primaries -> linear sRGB, HdrToneMapper's matrices (row-major).
constexpr float kBt2020ToSrgb[3][3] = {
    {1.6604910f, -0.5876411f, -0.0728499f},
    {-0.1245505f, 1.1328999f, -0.0083494f},
    {-0.0181508f, -0.1005789f, 1.1187297f}};
constexpr float kP3ToSrgb[3][3] = {{1.2249402f, -0.2249402f, 0.0000000f},
                                   {-0.0420569f, 1.0420569f, 0.0000000f},
                                   {-0.0196376f, -0.0786361f, 1.0982737f}};
constexpr float kIdentity3[3][3] = {
    {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}};

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

void setGray(float (&target)[4], float value) {
  target[0] = value;
  target[1] = value;
  target[2] = value;
  target[3] = 1.0f;
}

void setMatrixRow(float (&target)[4], const ColorMatrix3 &matrix, int row) {
  target[0] = static_cast<float>(matrix[row * 3]);
  target[1] = static_cast<float>(matrix[row * 3 + 1]);
  target[2] = static_cast<float>(matrix[row * 3 + 2]);
  target[3] = 0.0f;
}

void setCurve(float (&first)[4], float (&second)[4],
              const TransferCurve &curve) {
  first[0] = curve.a;
  first[1] = curve.b;
  first[2] = curve.c;
  first[3] = curve.d;
  second[0] = curve.e;
  second[1] = curve.f;
  second[2] = curve.g;
  second[3] = 0.0f;
}

// Uniforms of the conversion pass shared by every tile (size and projection
// are per tile); lutSize is the side of the bound lookup table.
ConvertUniforms conversionUniforms(const SourceConversion &conversion,
                                   int lutSize) {
  ConvertUniforms uniforms;
  if (conversion.hdr) {
    uniforms.sourceMode = conversion.toneMapping.enabled
                              ? ConvertUniforms::kSourceToneMapped
                              : ConvertUniforms::kSourceEncoded;
    uniforms.hdrTransfer = static_cast<qint32>(conversion.hdrEncoding.transfer);
    uniforms.toneMapOperator =
        static_cast<qint32>(conversion.toneMapping.op);
    const float white = conversion.toneMapping.whiteNits > 0.0f
                            ? conversion.toneMapping.whiteNits
                            : ToneMapping::kDefaultWhiteNits;
    uniforms.whiteScale = 1.0f / white;
  }
  const auto &primaries =
      conversion.hdrEncoding.primaries == HdrPrimaries::Bt2020 ? kBt2020ToSrgb
      : conversion.hdrEncoding.primaries == HdrPrimaries::DisplayP3
          ? kP3ToSrgb
          : kIdentity3;
  setUniformRow(uniforms.primariesRow0, primaries[0]);
  setUniformRow(uniforms.primariesRow1, primaries[1]);
  setUniformRow(uniforms.primariesRow2, primaries[2]);

  const ParametricColorTransform &parametric = conversion.color.parametric;
  setMatrixRow(uniforms.colorRow0, parametric.matrix, 0);
  setMatrixRow(uniforms.colorRow1, parametric.matrix, 1);
  setMatrixRow(uniforms.colorRow2, parametric.matrix, 2);
  setCurve(uniforms.sourceCurve0, uniforms.sourceCurve1,
           parametric.sourceCurve);
  setCurve(uniforms.targetCurve0, uniforms.targetCurve1,
           parametric.targetCurve);
  if (conversion.color.kind == ColorTransformKind::Parametric) {
    uniforms.colorMode = ConvertUniforms::kColorParametric;
  } else if (conversion.color.kind == ColorTransformKind::Lut &&
             conversion.lut && lutSize > 0) {
    uniforms.colorMode = ConvertUniforms::kColorLut;
    // Lattice point i of n lies at the centre of texel i.
    uniforms.lutScale =
        static_cast<float>(lutSize - 1) / static_cast<float>(lutSize);
    uniforms.lutOffset = 0.5f / static_cast<float>(lutSize);
  }
  return uniforms;
}

// Filtering decisions of one layer in one frame, shared by all its tiles.
struct LayerFilter {
  bool downscaling = false;
  bool sharpen = false;
  // Draw the exact-ratio downsample instead of the mip chain.
  bool exactReduce = false;
  // Draw the MKS2021 resampling instead of the mip chain.
  bool resample = false;
};

LayerFilter layerFilter(const RenderFrame &frame, qreal scale,
                        bool reduceAvailable, bool resampleAvailable) {
  LayerFilter filter;
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

// Uniforms shared by every tile of the layer: projection, checkerboard,
// colour matrix and sharpening parameters.
TileUniforms layerUniforms(const RenderFrame &frame,
                           const ImageLayer::DrawParams &params,
                           bool imageHasAlpha, const LayerFilter &filter) {
  TileUniforms uniforms;
  std::memcpy(uniforms.mvp, params.mvp.constData(), sizeof(uniforms.mvp));
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
      std::fmod(params.checkerOrigin.x(), static_cast<qreal>(checkerTile)));
  uniforms.checkerOrigin[1] = static_cast<float>(
      std::fmod(params.checkerOrigin.y(), static_cast<qreal>(checkerTile)));
  setGray(uniforms.checkerLight, kCheckerboardLight);
  setGray(uniforms.checkerDark, kCheckerboardDark);

  const ImageFilter &imageFilter = frame.filter;
  if (imageFilter.colorAdjustments.hasAdjustments()) {
    const ColorMatrix matrix =
        colorAdjustmentMatrix(imageFilter.colorAdjustments);
    setUniformRow(uniforms.colorRow0, matrix.m[0]);
    setUniformRow(uniforms.colorRow1, matrix.m[1]);
    setUniformRow(uniforms.colorRow2, matrix.m[2]);
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
                                            const ImageLayer::DrawParams &params) {
  const QRectF core(tile.core);
  const QRectF deviceRect(params.origin + core.topLeft() * params.scale,
                          core.size() * params.scale);
  // Clip on the CPU in double precision: at high zoom levels the unclipped
  // corners of a large image are too far away for float vertex positions.
  const QRectF visible = deviceRect.intersected(
      QRectF(QPointF(0.0, 0.0), QSizeF(params.targetSize)));
  if (visible.isEmpty())
    return std::nullopt;

  const QRectF texture(tile.texture);
  const QPointF sourceTopLeft =
      core.topLeft() + (visible.topLeft() - deviceRect.topLeft()) /
                           params.scale;
  const QSizeF sourceSize = visible.size() / params.scale;
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
} // namespace

//------------------------------------------------------------------------------
ImageLayer::ImageLayer(RenderDevice &device) : mDevice(device) {}

bool ImageLayer::isEmpty() const { return mTiles.empty(); }

QSize ImageLayer::imageSize() const { return mUploadedSize; }

bool ImageLayer::hasAlpha() const { return mImageHasAlpha; }

std::vector<ImageLayer::GpuTile> &ImageLayer::tiles() { return mTiles; }

void ImageLayer::release() {
  // QRhi defers the release of native resources still used by frames in
  // flight, so the tiles can be dropped immediately.
  mTiles.clear();
  mUploadedGeneration.reset();
  mUploadedTileSizeLimit = 0;
  mUploadedSize = QSize();
  mUploadFormat = QRhiTexture::UnknownFormat;
  mDisplayFormat = QRhiTexture::UnknownFormat;
  mImageHasAlpha = false;
  mUploadedConverted = false;
  mSourcesRetained = false;
  mConvertedWith.reset();
}

//------------------------------------------------------------------------------
bool ImageLayer::wantsConversion(const SourceConversion &conversion) const {
  return mDevice.hasConvertPass() && conversion.isActive();
}

bool ImageLayer::needsUpload(const LayerSource &source,
                             int tileSizeLimit) const {
  if (!mUploadedGeneration || *mUploadedGeneration != source.generation)
    return true;
  if (!source.image)
    return false;
  if (mUploadedTileSizeLimit != tileSizeLimit ||
      mUploadedConverted != wantsConversion(source.conversion))
    return true;
  // An SDR source was released after its conversion; a new conversion needs
  // it again.
  return mUploadedConverted && !mSourcesRetained &&
         mConvertedWith != source.conversion;
}

bool ImageLayer::needsConversion(const SourceConversion &conversion) const {
  return mUploadedConverted && !mTiles.empty() && mTiles.front().source &&
         mConvertedWith != conversion;
}

void ImageLayer::prepare(QRhiCommandBuffer *cb, const LayerSource &source,
                         int tileSizeLimit) {
  QRhi *rhi = mDevice.rhi();
  if (needsUpload(source, tileSizeLimit)) {
    QRhiResourceUpdateBatch *updates = rhi->nextResourceUpdateBatch();
    if (!updates) {
      mDevice.reportError(u"No QRhi resource update batch is available"_s);
      return;
    }
    if (!upload(updates, source, tileSizeLimit)) {
      // The batch may reference textures of the failed upload.
      updates->release();
      return;
    }
    cb->resourceUpdate(updates);
    if (!mTiles.empty() && !mUploadedConverted) {
      // Box-reduce mips read the uploaded level 0, submitted above.
      QRhiResourceUpdateBatch *mips = rhi->nextResourceUpdateBatch();
      if (!mips) {
        mDevice.reportError(u"No QRhi resource update batch is available"_s);
        return;
      }
      for (GpuTile &tile : mTiles)
        mDevice.generateMips(cb, tile.texture.get(), mips);
      cb->resourceUpdate(mips);
    }
  }
  if (needsConversion(source.conversion))
    convertTiles(cb, source.conversion);
}

QRhiTexture *ImageLayer::createTexture(QRhiTexture::Format format, QSize size,
                                       QRhiTexture::Flags flags) {
  QRhiTexture *texture = mDevice.rhi()->newTexture(format, size, 1, flags);
  if (!texture->create()) {
    mDevice.reportError(u"Cannot create a %1 x %2 image texture"_s.arg(
        size.width()).arg(size.height()));
    delete texture;
    return nullptr;
  }
  mDevice.countImageTextureCreated();
  return texture;
}

bool ImageLayer::createTile(GpuTile &tile, QRhiTexture::Format format,
                            QRhiTexture::Flags extraFlags) {
  QRhi *rhi = mDevice.rhi();
  tile.texture.reset(createTexture(format, tile.region.texture.size(),
                                   mDevice.imageTextureFlags() | extraFlags));
  if (!tile.texture)
    return false;
  tile.uniforms.reset(rhi->newBuffer(
      QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(TileUniforms)));
  if (!tile.uniforms->create()) {
    mDevice.reportError(u"Cannot create an image tile uniform buffer"_s);
    return false;
  }
  for (std::size_t index = 0; index < kSamplingCount; ++index) {
    auto &bindings = tile.bindings[index];
    bindings.reset(rhi->newShaderResourceBindings());
    bindings->setBindings(
        {QRhiShaderResourceBinding::uniformBuffer(
             kUniformBinding,
             QRhiShaderResourceBinding::VertexStage |
                 QRhiShaderResourceBinding::FragmentStage,
             tile.uniforms.get()),
         QRhiShaderResourceBinding::sampledTexture(
             kTextureBinding, QRhiShaderResourceBinding::FragmentStage,
             tile.texture.get(),
             mDevice.sampler(
                 static_cast<RenderEnums::TextureSampling>(index)))});
    if (!bindings->create()) {
      mDevice.reportError(u"Cannot create image tile shader bindings"_s);
      return false;
    }
  }
  return true;
}

bool ImageLayer::upload(QRhiResourceUpdateBatch *updates,
                        const LayerSource &source, int tileSizeLimit) {
  QRhi *rhi = mDevice.rhi();
  mUploadedGeneration = source.generation;
  if (!source.image) {
    release();
    mUploadedGeneration = source.generation;
    return true;
  }

  const QImage &image = *source.image;
  const QList<ImageTile> layout = TileGrid::layout(image.size(), tileSizeLimit);
  if (layout.isEmpty()) {
    release();
    mUploadedGeneration = source.generation;
    mDevice.reportError(
        u"Cannot split a %1 x %2 image into textures of at most %3 "
        u"pixels"_s.arg(image.width())
            .arg(image.height())
            .arg(tileSizeLimit));
    return false;
  }

  const bool bgra8 = rhi->isTextureFormatSupported(QRhiTexture::BGRA8);
  const QRhiTexture::Flags imageFlags = mDevice.imageTextureFlags();
  // The mip chain is generated by the GPU, or rendered by the box-reduce
  // pipeline (generatesMipsByBoxReduce()), so the format must support that.
  const TextureFormatSupport support{
      bgra8, rhi->isTextureFormatSupported(QRhiTexture::RGBA16F, imageFlags) &&
                 (!mDevice.generatesMipsByBoxReduce() ||
                  rhi->isTextureFormatSupported(QRhiTexture::RGBA16F,
                                                QRhiTexture::RenderTarget))};
  TextureUploadFormat format =
      chooseTextureUploadFormat(image.format(), support);
  // Displayed texture of the conversion pass: rendered to, then mipmapped.
  QRhiTexture::Format displayFormat = format.textureFormat;
  const bool convert = wantsConversion(source.conversion);
  if (source.conversion.isActive() && !convert) {
    mDevice.reportError(u"Colour conversion is not available; the image is "
                        u"shown without tone mapping or colour management"_s);
  }
  if (convert) {
    // The source is only read with texelFetch().
    const std::optional<TextureUploadFormat> sourceFormat =
        chooseConversionSourceFormat(
            image.format(), source.conversion.hdr,
            TextureFormatSupport{
                bgra8, rhi->isTextureFormatSupported(QRhiTexture::RGBA16F),
                rhi->isTextureFormatSupported(QRhiTexture::RGBA32F)});
    if (!sourceFormat) {
      release();
      mUploadedGeneration = source.generation;
      mDevice.reportError(u"The GPU has no RGBA16F textures; HDR images "
                          u"cannot be shown"_s);
      return false;
    }
    format = *sourceFormat;
    displayFormat = convertedTextureFormat(format);
    if (displayFormat == QRhiTexture::RGBA16F &&
        !rhi->isTextureFormatSupported(
            QRhiTexture::RGBA16F, imageFlags | QRhiTexture::RenderTarget))
      displayFormat = QRhiTexture::RGBA8;
  }

  // The next frame of an animation: same tiles and formats, so the texels go
  // into the existing textures. Sources released after the conversion of a
  // previous image (not an animation frame) are created again.
  const bool reuse =
      !mTiles.empty() && mUploadedSize == image.size() &&
      mUploadedTileSizeLimit == tileSizeLimit &&
      mUploadedConverted == convert && mUploadFormat == format.textureFormat &&
      mDisplayFormat == displayFormat;
  if (reuse) {
    invalidateFilteredTiles();
    mConvertedWith.reset();
    for (GpuTile &tile : mTiles) {
      if (!convert || tile.source)
        continue;
      tile.source.reset(createTexture(format.textureFormat,
                                      tile.region.texture.size(), {}));
      if (!tile.source) {
        release();
        mUploadedGeneration = source.generation;
        return false;
      }
    }
  } else {
    release();
    mUploadedGeneration = source.generation;
    std::vector<GpuTile> tiles(layout.size());
    for (qsizetype index = 0; index < layout.size(); ++index) {
      GpuTile &tile = tiles[index];
      tile.region = layout[index];
      if (!createTile(tile, displayFormat,
                      convert ? QRhiTexture::RenderTarget
                              : QRhiTexture::Flags()))
        return false;
      if (convert) {
        tile.source.reset(createTexture(format.textureFormat,
                                        tile.region.texture.size(), {}));
        if (!tile.source)
          return false;
      }
    }
    mTiles = std::move(tiles);
  }

  // Formats that match a texture format are uploaded straight from the
  // shared image; everything else is converted one tile at a time, so that a
  // huge image is never converted as a whole.
  const bool direct = format.imageFormat == image.format();
  for (GpuTile &tile : mTiles) {
    QRhiTextureSubresourceUploadDescription texels;
    if (direct) {
      texels.setImage(image);
      texels.setSourceTopLeft(tile.region.texture.topLeft());
      texels.setSourceSize(tile.region.texture.size());
    } else {
      texels.setImage(
          image.copy(tile.region.texture).convertToFormat(format.imageFormat));
    }
    if (texels.image().isNull()) {
      mDevice.reportError(u"Cannot convert a %1 x %2 image tile for upload"_s.arg(
          tile.region.texture.width()).arg(tile.region.texture.height()));
      release();
      mUploadedGeneration = source.generation;
      return false;
    }
    // Converted tiles get their displayed texture and its mip chain from
    // convertTiles(); the others get their mips from prepare().
    updates->uploadTexture(convert ? tile.source.get() : tile.texture.get(),
                           QRhiTextureUploadEntry(0, 0, texels));
  }
  mUploadedTileSizeLimit = tileSizeLimit;
  mUploadedSize = image.size();
  mUploadFormat = format.textureFormat;
  mDisplayFormat = displayFormat;
  mImageHasAlpha = image.hasAlphaChannel();
  mUploadedConverted = convert;
  mSourcesRetained =
      convert && (source.conversion.hdr || source.animationFrame);
  mDevice.countImageUpload();
  mDevice.clearErrorRepeatGuard();
  return true;
}

//------------------------------------------------------------------------------
// Renders every tile's source through the conversion pass into level 0 of
// its displayed texture, then builds the mip chain from the result. Cached
// exact downsamples and resamplings were built from the previous conversion.
void ImageLayer::convertTiles(QRhiCommandBuffer *cb,
                              const SourceConversion &conversion) {
  mConvertedWith = conversion;
  invalidateFilteredTiles();
  int lutSize = 0;
  QRhiTexture *lut = mDevice.colorLut(cb, conversion, &lutSize);
  // Without its table (reported) the conversion still decodes and tone maps;
  // it only skips the colour transform.
  if (!lut) {
    lut = mDevice.placeholderLut();
    lutSize = 0;
  }
  QRhiResourceUpdateBatch *mips = mDevice.rhi()->nextResourceUpdateBatch();
  if (!mips) {
    mDevice.reportError(u"No QRhi resource update batch is available"_s);
    return;
  }
  ConvertUniforms uniforms = conversionUniforms(conversion, lutSize);
  for (GpuTile &tile : mTiles) {
    const QSize size = tile.texture->pixelSize();
    const QMatrix4x4 mvp = mDevice.offscreenProjection(size);
    std::memcpy(uniforms.mvp, mvp.constData(), sizeof(uniforms.mvp));
    uniforms.dstSize[0] = static_cast<float>(size.width());
    uniforms.dstSize[1] = static_cast<float>(size.height());
    const RenderDevice::OffscreenPass pass{
        RenderDevice::PassKind::Convert, tile.source.get(), nullptr, lut, size,
        &uniforms, sizeof(uniforms), {}};
    if (!mDevice.recordPassInto(cb, pass, tile.texture.get()))
      continue;
    mDevice.generateMips(cb, tile.texture.get(), mips);
  }
  cb->resourceUpdate(mips);
  if (mSourcesRetained)
    return;
  // Read by the passes just recorded: released once the frame is done.
  for (GpuTile &tile : mTiles)
    FrameTexture released(tile.source.release());
}

void ImageLayer::invalidateFilteredTiles() {
  for (GpuTile &tile : mTiles) {
    tile.reducedScale.reset();
    tile.reduced.reset();
    tile.reducedBindings.reset();
    tile.resampledScale.reset();
    tile.resampledOutputs = QRect();
    tile.resampled.reset();
    tile.resampledBindings.reset();
  }
}

//------------------------------------------------------------------------------
void ImageLayer::prepareDraws(QRhiCommandBuffer *cb, const RenderFrame &frame,
                              const DrawParams &params,
                              QRhiResourceUpdateBatch *updates,
                              std::vector<QRhiShaderResourceBindings *> &draws) {
  if (mTiles.empty() || params.scale <= 0.0)
    return;
  const LayerFilter filter =
      layerFilter(frame, params.scale, mDevice.hasReducePass(),
                  mDevice.hasResamplePass());
  const QSize outputSize = ResampleGrid::outputSize(mUploadedSize, params.scale);
  const QPoint outputOrigin = params.origin.toPoint();

  struct VisibleTile {
    GpuTile *tile;
    TileRegion region;
    // Visible outputs of the tile in the resampling grid; empty unless the
    // layer is resampled.
    QRect outputs;
  };
  std::vector<VisibleTile> visible;
  for (GpuTile &tile : mTiles) {
    const auto region = visibleTileRegion(tile.region, params);
    if (!region)
      continue;
    QRect outputs;
    if (filter.resample) {
      outputs = ResampleGrid::visibleTileOutputs(
          tile.region, mUploadedSize, outputSize, outputOrigin,
          params.targetSize);
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
      if (tile.resampledScale != params.scale ||
          tile.resampledOutputs != entry.outputs)
        buildResampledTile(cb, tile, params.scale, entry.outputs);
    } else if (filter.exactReduce && tile.reducedScale != params.scale) {
      buildReducedTile(cb, tile, params.scale);
    }
  }

  enum class DrawSource { MipChain, Reduced, Resampled };
  TileUniforms uniforms = layerUniforms(frame, params, mImageHasAlpha, filter);
  const std::size_t sampling = samplingIndex(frame.settings.sampling);
  for (const VisibleTile &entry : visible) {
    const GpuTile &tile = *entry.tile;
    DrawSource source = DrawSource::MipChain;
    if (filter.resample && tile.resampled &&
        tile.resampledScale == params.scale &&
        tile.resampledOutputs == entry.outputs)
      source = DrawSource::Resampled;
    else if (filter.exactReduce && tile.reduced &&
             tile.reducedScale == params.scale)
      source = DrawSource::Reduced;

    QSizeF stepTexels;
    QRhiShaderResourceBindings *bindings = tile.bindings[sampling].get();
    if (source == DrawSource::Resampled) {
      // The resampled outputs are whole device pixels: drawn 1:1.
      setUniformRect(uniforms.targetRect,
                     QRectF(entry.outputs.translated(outputOrigin)));
      setUniformRect(uniforms.texRect, QRectF(0.0, 0.0, 1.0, 1.0));
      stepTexels = QSizeF(tile.resampled->pixelSize());
      bindings = tile.resampledBindings.get();
    } else {
      setUniformRect(uniforms.targetRect, entry.region.target);
      setUniformRect(uniforms.texRect, entry.region.texture);
      // One device pixel in texture coordinates: the reduced texture is
      // already at device resolution; the mip chain is at source
      // resolution.
      stepTexels = source == DrawSource::Reduced
                       ? QSizeF(tile.reduced->pixelSize())
                       : QSizeF(tile.region.texture.size()) * params.scale;
      if (source == DrawSource::Reduced)
        bindings = tile.reducedBindings.get();
    }
    uniforms.texelStep[0] = static_cast<float>(1.0 / stepTexels.width());
    uniforms.texelStep[1] = static_cast<float>(1.0 / stepTexels.height());
    uniforms.downscaleTaps =
        filter.downscaling && source == DrawSource::MipChain
            ? TileUniforms::kEnabled
            : TileUniforms::kDisabled;
    updates->updateDynamicBuffer(tile.uniforms.get(), 0, sizeof(TileUniforms),
                                 &uniforms);
    draws.push_back(bindings);
  }
}

QRhiShaderResourceBindings *
ImageLayer::wrappingBindings(GpuTile &tile,
                             RenderEnums::TextureSampling sampling) {
  auto &bindings = tile.wrappingBindings[samplingIndex(sampling)];
  if (bindings)
    return bindings.get();
  auto created = std::unique_ptr<QRhiShaderResourceBindings>(
      mDevice.rhi()->newShaderResourceBindings());
  created->setBindings(
      {QRhiShaderResourceBinding::uniformBuffer(
           kUniformBinding,
           QRhiShaderResourceBinding::VertexStage |
               QRhiShaderResourceBinding::FragmentStage,
           tile.uniforms.get()),
       QRhiShaderResourceBinding::sampledTexture(
           kTextureBinding, QRhiShaderResourceBinding::FragmentStage,
           tile.texture.get(), mDevice.wrappingSampler(sampling))});
  if (!created->create()) {
    mDevice.reportError(u"Cannot create panorama shader bindings"_s);
    return nullptr;
  }
  bindings = std::move(created);
  return bindings.get();
}

//------------------------------------------------------------------------------
// Builds tile.reduced as a chain of box-reduce passes: each pass reduces an
// axis that has not reached its target by at most half, so the last pass per
// axis resamples the remaining fractional ratio.
void ImageLayer::buildReducedTile(QRhiCommandBuffer *cb, GpuTile &tile,
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
    FrameTexture reduced = mDevice.recordBoxPass(cb, source, current, next);
    if (!reduced)
      return;
    // The previous step is released once the frame no longer uses it.
    result = std::move(reduced);
    current = next;
  }

  auto bindings = std::unique_ptr<QRhiShaderResourceBindings>(
      mDevice.rhi()->newShaderResourceBindings());
  bindings->setBindings(
      {QRhiShaderResourceBinding::uniformBuffer(
           kUniformBinding,
           QRhiShaderResourceBinding::VertexStage |
               QRhiShaderResourceBinding::FragmentStage,
           tile.uniforms.get()),
       QRhiShaderResourceBinding::sampledTexture(
           kTextureBinding, QRhiShaderResourceBinding::FragmentStage,
           result.get(),
           mDevice.sampler(RenderEnums::TextureSampling::Bilinear))});
  if (!bindings->create()) {
    mDevice.reportError(u"Cannot create downsample shader bindings"_s);
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
void ImageLayer::buildResampledTile(QRhiCommandBuffer *cb, GpuTile &tile,
                                    qreal scale, const QRect &outputs) {
  tile.resampledScale = scale;
  tile.resampledOutputs = outputs;
  tile.resampled.reset();
  tile.resampledBindings.reset();

  const QSize outputSize = ResampleGrid::outputSize(mUploadedSize, scale);
  const Mks2021Axis xAxis{mUploadedSize.width(), outputSize.width()};
  const Mks2021Axis yAxis{mUploadedSize.height(), outputSize.height()};
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
  const int textureSizeMax =
      mDevice.rhi()->resourceLimit(QRhi::TextureSizeMax);
  if (columns.size.height() > textureSizeMax ||
      rowWeights.size.height() > textureSizeMax)
    return;
  const FrameTexture columnTable =
      mDevice.uploadWeightTable(cb, columns.size, columns.values);
  const FrameTexture rowTable =
      mDevice.uploadWeightTable(cb, rowWeights.size, rowWeights.values);
  if (!columnTable || !rowTable)
    return;

  ResampleUniforms horizontal;
  const QMatrix4x4 horizontalMvp = mDevice.offscreenProjection(horizontalSize);
  std::memcpy(horizontal.mvp, horizontalMvp.constData(),
              sizeof(horizontal.mvp));
  horizontal.dstSize[0] = static_cast<float>(horizontalSize.width());
  horizontal.dstSize[1] = static_cast<float>(horizontalSize.height());
  horizontal.axis = ResampleUniforms::kAxisHorizontal;
  horizontal.clampMin = 0;
  horizontal.clampMax = texture.width() - 1;
  horizontal.crossOffset = firstRow - texture.top();
  horizontal.tapCount = columns.taps;
  FrameTexture rows = mDevice.recordOffscreenPass(
      cb, RenderDevice::OffscreenPass{
              RenderDevice::PassKind::Resample, tile.texture.get(),
              columnTable.get(), nullptr, horizontalSize, &horizontal,
              sizeof(horizontal)});
  if (!rows)
    return;

  ResampleUniforms vertical;
  const QMatrix4x4 verticalMvp = mDevice.offscreenProjection(outputs.size());
  std::memcpy(vertical.mvp, verticalMvp.constData(), sizeof(vertical.mvp));
  vertical.dstSize[0] = static_cast<float>(outputs.width());
  vertical.dstSize[1] = static_cast<float>(outputs.height());
  vertical.axis = ResampleUniforms::kAxisVertical;
  vertical.clampMin = 0;
  vertical.clampMax = lastRow - firstRow;
  vertical.crossOffset = 0;
  vertical.finalPass = ResampleUniforms::kFinalPass;
  vertical.tapCount = rowWeights.taps;
  FrameTexture result = mDevice.recordOffscreenPass(
      cb, RenderDevice::OffscreenPass{
              RenderDevice::PassKind::Resample, rows.get(), rowTable.get(),
              nullptr, outputs.size(), &vertical, sizeof(vertical)});
  if (!result)
    return;

  auto bindings = std::unique_ptr<QRhiShaderResourceBindings>(
      mDevice.rhi()->newShaderResourceBindings());
  bindings->setBindings(
      {QRhiShaderResourceBinding::uniformBuffer(
           kUniformBinding,
           QRhiShaderResourceBinding::VertexStage |
               QRhiShaderResourceBinding::FragmentStage,
           tile.uniforms.get()),
       QRhiShaderResourceBinding::sampledTexture(
           kTextureBinding, QRhiShaderResourceBinding::FragmentStage,
           result.get(),
           mDevice.sampler(RenderEnums::TextureSampling::Nearest))});
  if (!bindings->create()) {
    mDevice.reportError(u"Cannot create resampling shader bindings"_s);
    return;
  }
  tile.resampled.reset(result.release());
  tile.resampledBindings = std::move(bindings);
}
