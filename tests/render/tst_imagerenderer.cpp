#include <QColorSpace>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QElapsedTimer>
#include <QFloat16>
#include <QQuickWindow>
#include <QRandomGenerator>
#include <QSGRendererInterface>
#include <QSignalSpy>
#include <QTest>
#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>

#include "gui/quick/render/imagerenderitem.h"
#include "gui/quick/render/resamplegrid.h"
#include "gui/quick/render/textureuploadformat.h"
#include "gui/quick/render/thumbnailitem.h"
#include "gui/quick/render/tilegrid.h"
#include "gui/quick/ui/imageviewportcontroller.h"
#include "offscreenquick.h"
#include "referenceimages.h"
#include "utils/hdrtonemapper.h"

namespace {
using namespace Qt::StringLiterals;

constexpr QSize kTestImageSize(64, 48);
// Zoom step of the viewport settle test.
constexpr double kViewportZoomStep = 0.25;
// The viewport read back goes through the scene graph's layer rendering of
// the item's texture; 8-bit rounding only.
constexpr int kGrabTolerance = 1;
constexpr int kMargin = 8;
constexpr int kLargestMagnification = 3;
constexpr QSize kFrameSize = kTestImageSize * kLargestMagnification +
                             QSize(2 * kMargin, 2 * kMargin);
constexpr QPoint kImageOrigin(kMargin, kMargin);
constexpr quint32 kImageSeed = 0x51A7E;
const QColor kBackground(32, 64, 96);
constexpr QSize kProbeSize(1, 1);

// 8-bit levels the GPU may differ from the CPU reference by.
// 1:1: premultiplication and blend rounding.
constexpr int kExactTolerance = 1;
// Filtered: texel quantization of mip levels, subtexel weight precision.
constexpr int kFilteredTolerance = 3;
// Deep mip chains: rounding accumulates once per generated level.
constexpr int kDeepMipTolerance = 4;
// Tiled vs untiled rendering of the same image. Tile-local texture
// coordinates round differently from whole-image ones; on the random-noise
// test image that moves trilinear results at fractional scales by up to two
// levels anywhere in the image. A seam without the overlap texels differs by
// tens of levels on the same image.
constexpr int kTileSeamTolerance = 2;
// Colour adjustments: un-premultiplying 8-bit texels and the matrix gain
// amplify the texel quantization.
constexpr int kColorTolerance = 3;
// Sharpening amplifies the filtering error of its taps (CAS by up to the
// inverse of its window weight, about 2x on the noise image).
constexpr int kSharpenedTolerance = 6;
// Exact downsample: one 8-bit quantization per pass of the chain.
constexpr int kReducedTolerance = 2;

// Largest image of the plan's acceptance criteria, shown at 1 / 32 so that
// it is drawn from mip level 5 (within TileGrid::kExactMipLevels).
constexpr QSize kHugeImageSize(32000, 8000);
constexpr int kHugeReduction = 32;
// QRhi::TextureSizeMax of Direct3D; GPUs with a larger limit (Vulkan often
// reports 32768) are restricted to it so that the huge image is still tiled.
constexpr int kHugeTileLimit = 16384;
// Smooth pattern with features smaller than one mip block.
constexpr int kHugePatternPeriodX = 97;
constexpr int kHugePatternPeriodY = 89;
constexpr int kHugePatternStepX = 41;
constexpr int kHugePatternStepY = 67;
constexpr int kHugePatternDiagonal = 13;
constexpr int kByteMask = 0xFF;
// Divisible by 2^TileGrid::kExactMipLevels; fits into one texture.
constexpr QSize kMipTestImageSize(2048, 512);

constexpr QSize kTiledImageSize(400, 300);
// Small enough to split kTiledImageSize into many tiles.
constexpr int kSmallTileLimit = TileGrid::kMinimumTiledTextureSize;

constexpr int kCheckerTilePx = 16;
constexpr int kCheckerCellPx = kCheckerTilePx / 2;
constexpr int kCheckerLightLevel = 0x99;
constexpr int kCheckerDarkLevel = 0x66;
constexpr int kOpaqueLevel = 255;
constexpr int kChannels = 4;

// Colour adjustments touching every stage of colorAdjustmentMatrix().
constexpr ColorAdjustments kTestAdjustments{
    0.5f,   // exposure
    1.2f,   // contrast
    0.05f,  // brightness
    0.1f,   // temperature
    -0.05f, // tint
    1.3f,   // saturation
    30.0f,  // hue
};
constexpr float kStrongCas = 1.0f;
constexpr float kNoCasContrast = 0.0f;
constexpr float kMildCas = 0.6f;
constexpr float kHighCasContrast = 0.8f;

// Scales whose on-screen size of kTestImageSize is whole pixels, so that the
// exact downsample is drawn texel-exact: 3/8 (two passes per axis) and 5/8
// (one fractional pass).
constexpr qreal kTwoPassReduction = 0.375;
constexpr qreal kOnePassReduction = 0.625;
// Scales that keep every tile of kTiledImageSize at a whole reduced size.
constexpr qreal kTiledFractionalReduction = 0.75;
constexpr qreal kTiledQuarterReduction = 0.25;
// Downscale without exact downsample (not settled), between mip levels.
constexpr qreal kUnsettledReduction = 0.37;
// Pan offset while settled; the cached downsample must follow it.
constexpr QPoint kPanOffset(5, 3);
// Flat colour for the downscale sharpening identity test.
const QColor kFlatColor(77, 128, 179);
// A sharpened noise image differs visibly from the unsharpened one.
constexpr int kVisibleSharpening = 8;

// MKS2021 resampling: float weights on the GPU, and one 8-bit quantization of
// the intermediate image whose rounding may flip, amplified by the kernel's
// negative lobes in the second pass.
constexpr int kMks2021Tolerance = 2;
// Upscales (whole and fractional output sizes) and downscales of
// kTestImageSize that fit into kFrameSize.
constexpr qreal kMksWholeUpscale = 3.0;
constexpr qreal kMksFractionalUpscale = 1.7;
constexpr qreal kMksHalf = 0.5;
constexpr qreal kMksFractionalReduction = 0.37;
constexpr qreal kMksStrongReduction = 0.2;
// Part of a strongly magnified image: only a window of the output is
// computed and drawn.
constexpr QSize kMksPartialImageSize(200, 150);
constexpr qreal kMksPartialScale = 4.0;
constexpr QSize kMksPartialFrameSize(160, 120);
constexpr QPoint kMksPartialOrigin(-300, -250);
// Tiled vs untiled MKS2021 at scales whose kernel stays inside the tile
// overlap.
constexpr qreal kMksTiledReduction = 0.5;
constexpr qreal kMksTiledUpscale = 1.6;

// Settle latency of the MKS2021 resampling, measured on a camera-sized image
// shown to fit a large window and magnified 3x.
constexpr QSize kLatencyImageSize(6000, 4000);
constexpr QSize kLatencyFrameSize(1920, 1280);
constexpr qreal kLatencyFitScale = 0.32;
constexpr qreal kLatencyZoomScale = 3.0;
// The fastest of several runs: other GPU work on the machine only adds time.
constexpr int kLatencyRuns = 9;
// Each run changes the scale slightly so that the resampling is rebuilt.
constexpr qreal kLatencyScaleStep = 0.001;
constexpr quint32 kRgb32OpaqueBits = 0xFF000000u;

// HDR tone mapping against HdrToneMapper (the unchanged CPU code, linked into
// this test). The GPU decodes 16-bit integer codes from half floats, evaluates
// the transfer, tanh and sRGB curves analytically where the CPU uses tables,
// and keeps the result in a half-float texture; the CPU rounds to 8 bits.
constexpr int kToneMapTolerance = 2;
// Colour management against QImage::convertedToColorSpace() (what
// ColorManager does): Qt's transfer tables against analytic curves.
constexpr int kColorManagementTolerance = 2;
// Targets with a pure gamma curve (Adobe RGB): Qt's 8-bit conversion tables
// flatten the steep start of the inverse curve, so the CPU shows the darkest
// shades up to 4 levels too dark where the GPU's analytic curve is exact
// (e.g. a small green contribution to Adobe RGB blue becomes 0 instead of
// 4).
constexpr int kPureGammaShadowTolerance = 4;
// Through a 33^3 lookup table: trilinear interpolation between the lattice
// points; largest for the darkest shades of a curve that is infinitely steep
// at black (the test's 2.0 gamma table curve).
constexpr int kColorLutTolerance = 4;
// HDR tone mapping followed by colour management: the CPU rounds to 8 bits
// in between.
constexpr int kHdrColorManagementTolerance = 3;
// A converted HDR image, then filtered: the CPU reference is the 8-bit tone
// mapped image, the GPU filters the half-float conversion.
constexpr int kConvertedFilterTolerance = 3;
// Peak of the random linear-light test images, in units of the 80 nit scRGB
// white: 2000 nits.
constexpr float kLinearPeak = 25.0f;
// Slightly negative samples exercise the gamut compression.
constexpr float kLinearFloor = -0.05f;
constexpr float kBrightWhiteNits = 400.0f;
constexpr int kLutWaitTimeoutMs = 10000;
constexpr int kTransferTableSize = 1024;
constexpr double kTableGamma = 2.0;
constexpr double kChannelMax16 = 65535.0;

// Panorama: a smooth image that is periodic horizontally (no edge at the
// back seam), magnified on screen so that trilinear sampling stays on level
// 0. The GPU's bilinear weights are quantized to a few subtexel bits.
constexpr QSize kPanoramaImageSize(512, 256);
constexpr QSize kPanoramaFrameSize(96, 64);
constexpr int kPanoramaTolerance = 2;
// Adjusted colours amplify the filtering error by the matrix gain.
constexpr int kPanoramaColorTolerance = 4;
constexpr double kPanoramaAmplitude = 0.4;
constexpr double kPanoramaMid = 0.5;
constexpr double kPanoramaGreenBase = 0.2;
constexpr double kPanoramaGreenRange = 0.6;
constexpr int kPanoramaRedPeriods = 2;
// Facing the image centre (u = 0.5), away from the back seam.
constexpr ReferencePanorama kPanoramaFront{0.0, 0.0, 90.0};

// Upscaled crop: a part of kTestImageSize upscaled kCropUpscale times.
constexpr QRect kCropSourceRect(16, 12, 24, 16);
constexpr int kCropUpscale = 2;

QRhi::Implementation rhiBackend = QRhi::D3D11;

QSGRendererInterface::GraphicsApi graphicsApiFor(QRhi::Implementation backend) {
  switch (backend) {
  case QRhi::D3D12:
    return QSGRendererInterface::Direct3D12;
  case QRhi::Vulkan:
    return QSGRendererInterface::Vulkan;
  default:
    return QSGRendererInterface::Direct3D11;
  }
}

enum class ImageKind { Opaque, Alpha, Deep, Grey };

QImage makeTestImage(ImageKind kind, QSize size) {
  const QImage::Format format = kind == ImageKind::Alpha ? QImage::Format_ARGB32
                                : kind == ImageKind::Deep ? QImage::Format_RGBA64
                                                          : QImage::Format_RGB32;
  QImage image(size, format);
  QRandomGenerator generator(kImageSeed);
  constexpr quint32 k8BitRange = 256;
  constexpr quint32 k16BitRange = 65536;
  for (int y = 0; y < size.height(); ++y) {
    for (int x = 0; x < size.width(); ++x) {
      if (kind == ImageKind::Deep) {
        image.setPixelColor(
            x, y,
            QColor::fromRgba64(quint16(generator.bounded(k16BitRange)),
                               quint16(generator.bounded(k16BitRange)),
                               quint16(generator.bounded(k16BitRange)),
                               quint16(generator.bounded(k16BitRange))));
      } else if (kind == ImageKind::Grey) {
        const int level = int(generator.bounded(k8BitRange));
        image.setPixelColor(x, y, QColor(level, level, level));
      } else {
        image.setPixelColor(x, y,
                            QColor(int(generator.bounded(k8BitRange)),
                                   int(generator.bounded(k8BitRange)),
                                   int(generator.bounded(k8BitRange)),
                                   kind == ImageKind::Alpha
                                       ? int(generator.bounded(k8BitRange))
                                       : kOpaqueLevel));
      }
    }
  }
  return image;
}

QImage makeHugeImage() {
  QImage image(kHugeImageSize, QImage::Format_Grayscale8);
  if (image.isNull())
    return image;
  for (int y = 0; y < image.height(); ++y) {
    uchar *line = image.scanLine(y);
    const int rowPart = y / kHugePatternPeriodY * kHugePatternStepY;
    for (int x = 0; x < image.width(); ++x) {
      line[x] = uchar((x / kHugePatternPeriodX * kHugePatternStepX + rowPart +
                       (x + y) / kHugePatternDiagonal) &
                      kByteMask);
    }
  }
  return image;
}

enum class HdrKind { Pq, PqAlpha, Hlg, LinearHalf, LinearFloat };

// Random HDR samples in the formats the image plugins decode into: 16-bit
// PQ / HLG codes tagged with a BT.2100 colour space, untagged half-float
// linear light (BT.2020 primaries, the tone mapper's default) and
// float linear light tagged linear sRGB.
QImage makeHdrImage(HdrKind kind, QSize size) {
  QRandomGenerator generator(kImageSeed);
  constexpr quint32 k16BitRange = 65536;
  if (kind == HdrKind::LinearHalf || kind == HdrKind::LinearFloat) {
    const bool half = kind == HdrKind::LinearHalf;
    QImage image(size, half ? QImage::Format_RGBA16FPx4
                            : QImage::Format_RGBA32FPx4);
    if (!half)
      image.setColorSpace(QColorSpace(QColorSpace::SRgbLinear));
    for (int y = 0; y < size.height(); ++y) {
      for (int x = 0; x < size.width(); ++x) {
        float values[kChannels];
        for (int c = 0; c < kChannels - 1; ++c) {
          values[c] = kLinearFloor + float(generator.generateDouble()) *
                                         (kLinearPeak - kLinearFloor);
        }
        values[kChannels - 1] = 1.0f;
        for (int c = 0; c < kChannels; ++c) {
          if (half) {
            reinterpret_cast<qfloat16 *>(image.scanLine(y))[x * kChannels + c] =
                qfloat16(values[c]);
          } else {
            reinterpret_cast<float *>(image.scanLine(y))[x * kChannels + c] =
                values[c];
          }
        }
      }
    }
    return image;
  }
  QImage image(size, QImage::Format_RGBA64);
  image.setColorSpace(QColorSpace(kind == HdrKind::Hlg
                                      ? QColorSpace::Bt2100Hlg
                                      : QColorSpace::Bt2100Pq));
  for (int y = 0; y < size.height(); ++y) {
    for (int x = 0; x < size.width(); ++x) {
      const quint16 alpha = kind == HdrKind::PqAlpha
                                ? quint16(generator.bounded(k16BitRange))
                                : quint16(kChannelMax16);
      image.setPixelColor(
          x, y,
          QColor::fromRgba64(quint16(generator.bounded(k16BitRange)),
                             quint16(generator.bounded(k16BitRange)),
                             quint16(generator.bounded(k16BitRange)), alpha));
    }
  }
  return image;
}

// The CPU display path of an HDR image (ImageStatic::loadGeneric()).
QImage cpuToneMapped(const QImage &image, const ToneMapping &toneMapping) {
  if (!toneMapping.enabled) {
    return image.convertToFormat(image.hasAlphaChannel()
                                     ? QImage::Format_ARGB32
                                     : QImage::Format_RGB32);
  }
  return HdrToneMapper::applyToneMapping(
      image, HdrToneMapParams{true,
                              static_cast<ToneMapOperator>(toneMapping.op),
                              toneMapping.whiteNits});
}

// sRGB primaries with a table transfer function (a pure 2.0 gamma): only
// reachable through a colour lookup table.
QColorSpace tableCurveSpace() {
  QList<uint16_t> table;
  for (int i = 0; i < kTransferTableSize; ++i) {
    const double x = double(i) / (kTransferTableSize - 1);
    table.append(uint16_t(std::lround(std::pow(x, kTableGamma) * kChannelMax16)));
  }
  return QColorSpace(QColorSpace::Primaries::SRgb, table);
}

// ColorManager's Rec2020 preset.
QColorSpace rec2020Gamma22() {
  return QColorSpace(QPointF(0.3127, 0.3290), QPointF(0.708, 0.292),
                     QPointF(0.170, 0.797), QPointF(0.131, 0.046),
                     QColorSpace::TransferFunction::Gamma, 2.2f);
}

// What ColorManager::applyColorManagement() shows for image.
QImage cpuColorManaged(QImage image, const QColorSpace &target) {
  if (!image.colorSpace().isValid())
    image.setColorSpace(QColorSpace(QColorSpace::SRgb));
  return image.convertedToColorSpace(target);
}

ColorManagement managedFor(const QColorSpace &target) {
  return ColorManagement{true, target};
}

// The image drawn 1:1 with nearest sampling: exactly the converted texels.
FloatImage oneToOneFrame(const QImage &reference) {
  return expectedFrame(premultipliedSource(reference),
                       ReferenceScene{kFrameSize, kImageOrigin, {1, 1},
                                      ReferenceFilter::Nearest, kBackground});
}

std::shared_ptr<const QImage> shared(QImage image) {
  return std::make_shared<const QImage>(std::move(image));
}

// Premultiplied texels as the renderer uploads them (8-bit images are
// quantized after premultiplication), which the filters read.
FloatImage uploadedSource(const QImage &image) {
  const TextureUploadFormat format =
      chooseTextureUploadFormat(image.format(), TextureFormatSupport{true, true});
  return premultipliedSource(image.convertToFormat(format.imageFormat));
}

RenderSettings settingsWith(RenderEnums::TextureSampling sampling,
                            bool transparencyGrid = false,
                            int maxTileSize = RenderSettings::kNoTileSizeLimit) {
  RenderSettings settings;
  settings.sampling = sampling;
  settings.backgroundColor = kBackground;
  settings.transparencyGrid = transparencyGrid;
  settings.maxTileSize = maxTileSize;
  return settings;
}

ImageFilter filterWith(RenderEnums::Sharpening sharpening,
                       float casSharpening = kStrongCas,
                       float casContrast = kNoCasContrast,
                       const ColorAdjustments &adjustments = {}) {
  ImageFilter filter;
  filter.sharpening = sharpening;
  filter.casSharpening = casSharpening;
  filter.casContrast = casContrast;
  filter.colorAdjustments = adjustments;
  return filter;
}

ReferenceFilterParams referenceParams(const ImageFilter &filter) {
  ReferenceFilterParams params;
  switch (filter.sharpening) {
  case RenderEnums::Sharpening::None:
    params.sharpening = ReferenceSharpening::None;
    break;
  case RenderEnums::Sharpening::Cas:
    params.sharpening = ReferenceSharpening::Cas;
    break;
  case RenderEnums::Sharpening::Smart:
    params.sharpening = ReferenceSharpening::Smart;
    break;
  }
  params.casSharpening = filter.casSharpening;
  params.casContrast = filter.casContrast;
  if (filter.colorAdjustments.hasAdjustments())
    params.color = colorAdjustmentMatrix(filter.colorAdjustments);
  return params;
}

int maxDifference(const QImage &a, const QImage &b) {
  int worst = 0;
  for (int y = 0; y < a.height(); ++y) {
    const uchar *lineA = a.constScanLine(y);
    const uchar *lineB = b.constScanLine(y);
    for (int i = 0; i < a.width() * kChannels; ++i)
      worst = qMax(worst, std::abs(int(lineA[i]) - int(lineB[i])));
  }
  return worst;
}

QString describeDifference(int difference, const QString &where) {
  return u"max difference %1 at %2"_s.arg(difference).arg(where);
}

// Smooth RGB test image of the panorama tests: red and blue repeat
// horizontally, green is a vertical ramp. Red and blue peak at the back seam
// (u = 0), far from the image mean that a too coarse mip level shows there.
QImage makePanoramaImage() {
  constexpr double kTau = 6.283185307179586;
  QImage image(kPanoramaImageSize, QImage::Format_RGB32);
  const double width = kPanoramaImageSize.width();
  const double height = kPanoramaImageSize.height();
  for (int y = 0; y < image.height(); ++y) {
    for (int x = 0; x < image.width(); ++x) {
      const double u = (x + 0.5) / width;
      const double v = (y + 0.5) / height;
      const double red =
          kPanoramaMid + kPanoramaAmplitude * std::cos(kTau * kPanoramaRedPeriods * u);
      const double green = kPanoramaGreenBase + kPanoramaGreenRange * v;
      const double blue =
          kPanoramaMid + kPanoramaAmplitude * std::cos(kTau * (u + v));
      image.setPixelColor(x, y, QColor::fromRgbF(float(red), float(green),
                                                 float(blue)));
    }
  }
  return image;
}

// The test image drawn in frame at scale magnification / reduction with the
// crop drawn over kCropSourceRect, both with bilinear magnification and box
// average reductions (the trilinear GPU result at these scales).
FloatImage expectedWithCrop(const QImage &image, const QImage &crop,
                            QSize frameSize, QPoint origin,
                            ReferenceScale scale) {
  FloatImage frame = expectedFrame(
      premultipliedSource(image),
      ReferenceScene{frameSize, origin, scale, ReferenceFilter::Bilinear,
                     kBackground});
  // Crop pixels per image pixel on screen: scale / kCropUpscale.
  ReferenceScale cropScale{scale.magnification, scale.reduction * kCropUpscale};
  while (cropScale.magnification > 1 && cropScale.reduction > 1 &&
         cropScale.magnification % 2 == 0 && cropScale.reduction % 2 == 0) {
    cropScale.magnification /= 2;
    cropScale.reduction /= 2;
  }
  const QPoint cropOrigin =
      origin + kCropSourceRect.topLeft() * scale.magnification / scale.reduction;
  const FloatImage cropFrame = expectedFrame(
      premultipliedSource(crop),
      ReferenceScene{frameSize, cropOrigin, cropScale,
                     ReferenceFilter::Bilinear, kBackground});
  const QRect cropRect(cropOrigin, kCropSourceRect.size() * scale.magnification /
                                       scale.reduction);
  const QRect visible = cropRect.intersected(QRect(QPoint(0, 0), frameSize));
  for (int y = visible.top(); y <= visible.bottom(); ++y) {
    for (int x = visible.left(); x <= visible.right(); ++x)
      frame.at(x, y) = cropFrame.at(x, y);
  }
  return frame;
}

QImage makeCropImage() {
  // Different content from the image part it covers.
  return makeTestImage(ImageKind::Opaque, kCropSourceRect.size() * kCropUpscale)
      .mirrored(true, true);
}

// ThumbnailItem tests: a solid thumbnail filling the scene, with corners of
// kThumbnailRadius; kThumbnailArcProbe lies on the arc of the top left
// corner (distance 0 from the outline at its centre to within a pixel).
constexpr QSize kThumbnailFrameSize(40, 30);
constexpr QRgb kThumbnailColor = 0xff6496c8;
constexpr qreal kThumbnailRadius = 8.0;
constexpr QPoint kThumbnailArcProbe(2, 2);
constexpr double kThumbnailHighlight = 0.5;

int maxChannelDifference(QRgb a, QRgb b) {
  return std::max({std::abs(qRed(a) - qRed(b)), std::abs(qGreen(a) - qGreen(b)),
                   std::abs(qBlue(a) - qBlue(b))});
}

std::unique_ptr<ThumbnailItem> makeThumbnailItem(OffscreenQuick &quick, double highlight) {
  QImage image(kThumbnailFrameSize, QImage::Format_ARGB32_Premultiplied);
  image.fill(QColor::fromRgba(kThumbnailColor));
  auto item = std::make_unique<ThumbnailItem>();
  item->setParentItem(quick.contentItem());
  item->setSize(QSizeF(kThumbnailFrameSize));
  item->setMaximumSize(kThumbnailFrameSize);
  item->setCornerRadius(kThumbnailRadius);
  item->setHighlight(highlight);
  item->setThumbnail({.image = image, .sourceSize = image.size()});
  return item;
}

// One offscreen scene with an ImageRenderItem filling it. The item is
// declared after the scene so that it is destroyed first.
struct Scene {
  OffscreenQuick quick;
  std::unique_ptr<ImageRenderItem> item;

  [[nodiscard]] bool create(QSize frameSize) {
    if (!quick.create(frameSize, rhiBackend))
      return false;
    item = std::make_unique<ImageRenderItem>();
    item->setParentItem(quick.contentItem());
    item->setSize(QSizeF(frameSize));
    return true;
  }

  [[nodiscard]] QImage render() {
    // Delivers queued work of the previous frame (renderer update requests,
    // posted render errors) before the next one, as the event loop would.
    QCoreApplication::processEvents();
    return quick.render();
  }
};
} // namespace

#define CREATE_SCENE(scene, size)                                              \
  QVERIFY2((scene).create(size), qPrintable((scene).quick.error()))

#define RENDER(scene, frame)                                                   \
  const QImage frame = (scene).render();                                       \
  QVERIFY2(!frame.isNull(), qPrintable((scene).quick.error()))

#define COMPARE_TO_REFERENCE(frame, expected, tolerance)                       \
  do {                                                                         \
    QString where;                                                             \
    const int difference = maxLevelDifference((frame), (expected), &where);    \
    QVERIFY2(difference <= (tolerance),                                        \
             qPrintable(describeDifference(difference, where)));               \
  } while (false)

Q_DECLARE_METATYPE(ImageKind)
Q_DECLARE_METATYPE(HdrKind)
Q_DECLARE_METATYPE(ToneMapping)
Q_DECLARE_METATYPE(QColorSpace)
Q_DECLARE_METATYPE(ReferenceScale)
Q_DECLARE_METATYPE(ImageFilter)
Q_DECLARE_METATYPE(ReferencePanorama)

class ImageRendererTests : public QObject {
  Q_OBJECT

private slots:
  void initTestCase() {
    const std::optional<QRhi::Implementation> backend = testRhiBackend();
    QVERIFY2(backend, "QSG_RHI_BACKEND names a backend the tests do not support");
    rhiBackend = *backend;
    // The offscreen QPA platform makes Qt Quick fall back to the software
    // adaptation; QQuickRhiItem needs the default (QRhi) one.
    QQuickWindow::setSceneGraphBackend(u"rhi"_s);
    QQuickWindow::setGraphicsApi(graphicsApiFor(rhiBackend));
    OffscreenQuick probe;
    if (!probe.create(kProbeSize, rhiBackend)) {
      QSKIP(qPrintable(u"QRhi backend %1 is not available: %2"_s.arg(
          rhiBackendName(rhiBackend), probe.error())));
    }
    qInfo().noquote() << "QRhi backend:" << rhiBackendName(rhiBackend)
                      << probe.rhi()->driverInfo().deviceName;
  }

  void uploadFormatSelection() {
    const TextureFormatSupport all{true, true, true};
    const TextureFormatSupport none{false, false, false};
    QCOMPARE(chooseTextureUploadFormat(QImage::Format_RGB32, all),
             (TextureUploadFormat{QRhiTexture::BGRA8, QImage::Format_RGB32}));
    QCOMPARE(chooseTextureUploadFormat(QImage::Format_ARGB32_Premultiplied, all),
             (TextureUploadFormat{QRhiTexture::BGRA8,
                                  QImage::Format_ARGB32_Premultiplied}));
    QCOMPARE(chooseTextureUploadFormat(QImage::Format_RGB32, none),
             (TextureUploadFormat{QRhiTexture::RGBA8,
                                  QImage::Format_RGBA8888_Premultiplied}));
    // Straight alpha is always premultiplied before upload.
    QCOMPARE(chooseTextureUploadFormat(QImage::Format_ARGB32, all),
             (TextureUploadFormat{QRhiTexture::RGBA8,
                                  QImage::Format_RGBA8888_Premultiplied}));
    QCOMPARE(chooseTextureUploadFormat(QImage::Format_RGBA8888, all),
             (TextureUploadFormat{QRhiTexture::RGBA8,
                                  QImage::Format_RGBA8888_Premultiplied}));
    QCOMPARE(chooseTextureUploadFormat(QImage::Format_RGBX8888, none),
             (TextureUploadFormat{QRhiTexture::RGBA8, QImage::Format_RGBX8888}));
    QCOMPARE(chooseTextureUploadFormat(QImage::Format_Grayscale8, all),
             (TextureUploadFormat{QRhiTexture::RGBA8,
                                  QImage::Format_RGBA8888_Premultiplied}));
    QCOMPARE(chooseTextureUploadFormat(QImage::Format_RGBA64, all),
             (TextureUploadFormat{QRhiTexture::RGBA16F,
                                  QImage::Format_RGBA16FPx4_Premultiplied}));
    QCOMPARE(chooseTextureUploadFormat(QImage::Format_Grayscale16, all),
             (TextureUploadFormat{QRhiTexture::RGBA16F,
                                  QImage::Format_RGBA16FPx4_Premultiplied}));
    QCOMPARE(chooseTextureUploadFormat(QImage::Format_RGBA64, none),
             (TextureUploadFormat{QRhiTexture::RGBA8,
                                  QImage::Format_RGBA8888_Premultiplied}));
    QCOMPARE(chooseTextureUploadFormat(QImage::Format_RGBA16FPx4_Premultiplied,
                                       all),
             (TextureUploadFormat{QRhiTexture::RGBA16F,
                                  QImage::Format_RGBA16FPx4_Premultiplied}));

    // Conversion sources keep straight alpha.
    QCOMPARE(chooseConversionSourceFormat(QImage::Format_ARGB32, false, all),
             (TextureUploadFormat{QRhiTexture::BGRA8, QImage::Format_ARGB32}));
    QCOMPARE(chooseConversionSourceFormat(QImage::Format_ARGB32_Premultiplied,
                                          false, all),
             (TextureUploadFormat{QRhiTexture::RGBA8, QImage::Format_RGBA8888}));
    // 16-bit HDR codes are kept exact.
    QCOMPARE(chooseConversionSourceFormat(QImage::Format_RGBA64, true, all),
             (TextureUploadFormat{QRhiTexture::RGBA32F,
                                  QImage::Format_RGBA32FPx4}));
    QCOMPARE(chooseConversionSourceFormat(QImage::Format_RGBA64, true,
                                          TextureFormatSupport{true, true,
                                                               false}),
             (TextureUploadFormat{QRhiTexture::RGBA16F,
                                  QImage::Format_RGBA16FPx4}));
    QCOMPARE(chooseConversionSourceFormat(QImage::Format_RGBA64, false, all),
             (TextureUploadFormat{QRhiTexture::RGBA16F,
                                  QImage::Format_RGBA16FPx4}));
    QCOMPARE(chooseConversionSourceFormat(QImage::Format_RGBA16FPx4, true, all),
             (TextureUploadFormat{QRhiTexture::RGBA16F,
                                  QImage::Format_RGBA16FPx4}));
    QCOMPARE(chooseConversionSourceFormat(QImage::Format_RGBA32FPx4, true, all),
             (TextureUploadFormat{QRhiTexture::RGBA16F,
                                  QImage::Format_RGBA16FPx4}));
    // An HDR image (even an 8-bit one) needs float textures.
    QCOMPARE(chooseConversionSourceFormat(QImage::Format_ARGB32, true, all),
             (TextureUploadFormat{QRhiTexture::RGBA32F,
                                  QImage::Format_RGBA32FPx4}));
    QVERIFY(!chooseConversionSourceFormat(QImage::Format_RGBA64, true, none));
    QCOMPARE(chooseConversionSourceFormat(QImage::Format_RGBA64, false, none),
             (TextureUploadFormat{QRhiTexture::RGBA8, QImage::Format_RGBA8888}));
    QCOMPARE(convertedTextureFormat(TextureUploadFormat{
                 QRhiTexture::RGBA16F, QImage::Format_RGBA16FPx4}),
             QRhiTexture::RGBA16F);
    QCOMPARE(convertedTextureFormat(TextureUploadFormat{
                 QRhiTexture::RGBA32F, QImage::Format_RGBA32FPx4}),
             QRhiTexture::RGBA16F);
    QCOMPARE(convertedTextureFormat(
                 TextureUploadFormat{QRhiTexture::BGRA8, QImage::Format_RGB32}),
             QRhiTexture::RGBA8);
  }

  void withoutImageClearsToBackground() {
    Scene scene;
    CREATE_SCENE(scene, kFrameSize);
    QSignalSpy errors(scene.item.get(), &ImageRenderItem::renderError);
    scene.item->setRenderSettings(
        settingsWith(RenderEnums::TextureSampling::Trilinear));
    RENDER(scene, frame);
    const FloatImage expected = expectedFrame(
        FloatImage(QSize(1, 1)),
        ReferenceScene{kFrameSize, QPoint(kFrameSize.width(), 0), {}, {},
                       kBackground});
    QString where;
    QCOMPARE(maxLevelDifference(frame, expected, &where), 0);
    QCOMPARE(errors.count(), 0);
  }

  void matchesReference_data() {
    QTest::addColumn<ImageKind>("kind");
    QTest::addColumn<ReferenceScale>("scale");
    QTest::addColumn<RenderEnums::TextureSampling>("sampling");
    QTest::addColumn<int>("tolerance");

    const struct {
      const char *name;
      ImageKind kind;
    } kinds[] = {{"opaque", ImageKind::Opaque},
                 {"alpha", ImageKind::Alpha},
                 {"16-bit", ImageKind::Deep}};
    const struct {
      const char *name;
      ReferenceScale scale;
      int tolerance;
    } scales[] = {{"100%", {1, 1}, kExactTolerance},
                  {"50%", {1, 2}, kFilteredTolerance},
                  {"25%", {1, 4}, kFilteredTolerance},
                  {"300%", {3, 1}, kFilteredTolerance}};
    for (const auto &kind : kinds) {
      for (const auto &scale : scales) {
        QTest::addRow("%s %s trilinear", kind.name, scale.name)
            << kind.kind << scale.scale
            << RenderEnums::TextureSampling::Trilinear << scale.tolerance;
      }
      QTest::addRow("%s 100%% nearest", kind.name)
          << kind.kind << ReferenceScale{1, 1}
          << RenderEnums::TextureSampling::Nearest << kExactTolerance;
      QTest::addRow("%s 300%% nearest", kind.name)
          << kind.kind << ReferenceScale{3, 1}
          << RenderEnums::TextureSampling::Nearest << kExactTolerance;
      QTest::addRow("%s 300%% bilinear", kind.name)
          << kind.kind << ReferenceScale{3, 1}
          << RenderEnums::TextureSampling::Bilinear << kFilteredTolerance;
    }
  }

  void matchesReference() {
    QFETCH(ImageKind, kind);
    QFETCH(ReferenceScale, scale);
    QFETCH(RenderEnums::TextureSampling, sampling);
    QFETCH(int, tolerance);

    Scene scene;
    CREATE_SCENE(scene, kFrameSize);
    QSignalSpy errors(scene.item.get(), &ImageRenderItem::renderError);
    const QImage image = makeTestImage(kind, kTestImageSize);
    scene.item->setImage(shared(image));
    scene.item->setRenderSettings(settingsWith(sampling));
    scene.item->setPlacement(ImagePlacement{
        QPointF(kImageOrigin),
        qreal(scale.magnification) / qreal(scale.reduction)});
    RENDER(scene, frame);

    const ReferenceFilter filter = sampling == RenderEnums::TextureSampling::Nearest
                                       ? ReferenceFilter::Nearest
                                       : ReferenceFilter::Bilinear;
    const FloatImage expected = expectedFrame(
        premultipliedSource(image),
        ReferenceScene{kFrameSize, kImageOrigin, scale, filter, kBackground});
    COMPARE_TO_REFERENCE(frame, expected, tolerance);
    QCOMPARE(errors.count(), 0);
  }

  void checkerboardUnderTransparentImage() {
    Scene scene;
    CREATE_SCENE(scene, kFrameSize);
    QImage transparent(kTestImageSize, QImage::Format_ARGB32);
    transparent.fill(Qt::transparent);
    scene.item->setImage(shared(transparent));
    scene.item->setRenderSettings(
        settingsWith(RenderEnums::TextureSampling::Trilinear, true));
    scene.item->setPlacement(ImagePlacement{QPointF(kImageOrigin), 1.0});
    RENDER(scene, frame);

    const QRect imageRect(kImageOrigin, kTestImageSize);
    for (int y = 0; y < kFrameSize.height(); ++y) {
      for (int x = 0; x < kFrameSize.width(); ++x) {
        QColor expected = kBackground;
        if (imageRect.contains(x, y)) {
          const int cx = (x - kImageOrigin.x()) % kCheckerTilePx;
          const int cy = (y - kImageOrigin.y()) % kCheckerTilePx;
          const int level = (cx < kCheckerCellPx) == (cy < kCheckerCellPx)
                                ? kCheckerLightLevel
                                : kCheckerDarkLevel;
          expected = QColor(level, level, level);
        }
        const QColor actual = frame.pixelColor(x, y);
        if (actual.rgba() != expected.rgba()) {
          QFAIL(qPrintable(u"pixel (%1, %2) is %3, expected %4"_s.arg(x).arg(y).arg(
              actual.name(QColor::HexArgb), expected.name(QColor::HexArgb))));
        }
      }
    }
  }

  void noCheckerboardUnderOpaqueImage() {
    Scene scene;
    CREATE_SCENE(scene, kFrameSize);
    const QImage image = makeTestImage(ImageKind::Opaque, kTestImageSize);
    scene.item->setImage(shared(image));
    scene.item->setRenderSettings(
        settingsWith(RenderEnums::TextureSampling::Nearest, true));
    scene.item->setPlacement(ImagePlacement{QPointF(kImageOrigin), 1.0});
    RENDER(scene, frame);
    const FloatImage expected =
        expectedFrame(premultipliedSource(image),
                      ReferenceScene{kFrameSize, kImageOrigin, {}, {},
                                     kBackground});
    QString where;
    QCOMPARE(maxLevelDifference(frame, expected, &where), 0);
  }

  void tiledMatchesUntiled_data() {
    QTest::addColumn<qreal>("scale");
    QTest::newRow("100%") << 1.0;
    QTest::newRow("200%") << 2.0;
    QTest::newRow("50%") << 0.5;
    QTest::newRow("37%") << 0.37;
    QTest::newRow("25%") << 0.25;
  }

  void tiledMatchesUntiled() {
    QFETCH(qreal, scale);
    const QSize frameSize =
        (QSizeF(kTiledImageSize) * scale).toSize() +
        QSize(2 * kMargin, 2 * kMargin);
    Scene scene;
    CREATE_SCENE(scene, frameSize);
    QSignalSpy errors(scene.item.get(), &ImageRenderItem::renderError);
    scene.item->setImage(shared(makeTestImage(ImageKind::Alpha, kTiledImageSize)));
    scene.item->setPlacement(ImagePlacement{QPointF(kImageOrigin), scale});
    scene.item->setRenderSettings(
        settingsWith(RenderEnums::TextureSampling::Trilinear));
    RENDER(scene, untiled);
    scene.item->setRenderSettings(settingsWith(
        RenderEnums::TextureSampling::Trilinear, false, kSmallTileLimit));
    RENDER(scene, tiled);
    QVERIFY(TileGrid::layout(kTiledImageSize, kSmallTileLimit).size() > 1);
    QVERIFY2(maxDifference(tiled, untiled) <= kTileSeamTolerance,
             qPrintable(u"max difference %1"_s.arg(maxDifference(tiled, untiled))));
    QCOMPARE(errors.count(), 0);
  }

  void hugeImageRendersTiled() {
    const QSize frameSize = kHugeImageSize / kHugeReduction;
    Scene scene;
    CREATE_SCENE(scene, frameSize);
    const int tileLimit = qMin(
        scene.quick.rhi()->resourceLimit(QRhi::TextureSizeMax), kHugeTileLimit);
    QVERIFY(TileGrid::layout(kHugeImageSize, tileLimit).size() > 1);
    QSignalSpy errors(scene.item.get(), &ImageRenderItem::renderError);
    const QImage image = makeHugeImage();
    QVERIFY(!image.isNull());
    scene.item->setImage(shared(image));
    scene.item->setRenderSettings(settingsWith(
        RenderEnums::TextureSampling::Trilinear, false, tileLimit));
    scene.item->setPlacement(
        ImagePlacement{QPointF(0.0, 0.0), 1.0 / kHugeReduction});
    RENDER(scene, frame);

    const FloatImage expected =
        expectedFrame(boxReduceGrayscale8(image, kHugeReduction),
                      ReferenceScene{frameSize, QPoint(0, 0), {}, {},
                                     kBackground});
    COMPARE_TO_REFERENCE(frame, expected, kDeepMipTolerance);
    QCOMPARE(errors.count(), 0);
  }

  // Every mip level the renderer samples is the box average of the image
  // (Direct3D 12 builds the chain with the box-reduce pipeline).
  void mipChainMatchesBoxAverages_data() {
    QTest::addColumn<int>("level");
    for (int level = 1; level <= TileGrid::kExactMipLevels; ++level)
      QTest::addRow("level %d", level) << level;
  }

  void mipChainMatchesBoxAverages() {
    QFETCH(int, level);
    const int reduction = 1 << level;
    QImage image(kMipTestImageSize, QImage::Format_Grayscale8);
    QVERIFY(!image.isNull());
    for (int y = 0; y < image.height(); ++y) {
      uchar *line = image.scanLine(y);
      const int rowPart = y / kHugePatternPeriodY * kHugePatternStepY;
      for (int x = 0; x < image.width(); ++x) {
        line[x] = uchar((x / kHugePatternPeriodX * kHugePatternStepX + rowPart +
                         (x + y) / kHugePatternDiagonal) &
                        kByteMask);
      }
    }
    const QSize frameSize = kMipTestImageSize / reduction;
    Scene scene;
    CREATE_SCENE(scene, frameSize);
    QSignalSpy errors(scene.item.get(), &ImageRenderItem::renderError);
    scene.item->setImage(shared(image));
    scene.item->setRenderSettings(
        settingsWith(RenderEnums::TextureSampling::Trilinear));
    scene.item->setPlacement(
        ImagePlacement{QPointF(0.0, 0.0), 1.0 / reduction});
    RENDER(scene, frame);
    const FloatImage expected =
        expectedFrame(boxReduceGrayscale8(image, reduction),
                      ReferenceScene{frameSize, QPoint(0, 0), {}, {},
                                     kBackground});
    COMPARE_TO_REFERENCE(frame, expected, kDeepMipTolerance);
    QCOMPARE(errors.count(), 0);
  }

  void untileableImageReportsError() {
    Scene scene;
    CREATE_SCENE(scene, kFrameSize);
    QSignalSpy errors(scene.item.get(), &ImageRenderItem::renderError);
    scene.item->setImage(shared(makeTestImage(ImageKind::Opaque, kTiledImageSize)));
    scene.item->setRenderSettings(settingsWith(
        RenderEnums::TextureSampling::Trilinear, false,
        TileGrid::kMinimumTiledTextureSize - 1));
    RENDER(scene, reported);
    // Delivered through a queued call on the GUI thread.
    QTRY_COMPARE(errors.count(), 1);
    QVERIFY(!errors.first().first().toString().isEmpty());

    // The failure clears to the background and is reported only once.
    const FloatImage background = expectedFrame(
        FloatImage(QSize(1, 1)),
        ReferenceScene{kFrameSize, QPoint(kFrameSize.width(), 0), {}, {},
                       kBackground});
    QString where;
    QCOMPARE(maxLevelDifference(reported, background, &where), 0);
    RENDER(scene, again);
    QCoreApplication::processEvents();
    QCOMPARE(errors.count(), 1);
  }

  void movingToAnotherRhiRebuildsResources() {
    Scene first;
    CREATE_SCENE(first, kFrameSize);
    OffscreenQuick second;
    QVERIFY2(second.create(kFrameSize, rhiBackend), qPrintable(second.error()));

    const QImage image = makeTestImage(ImageKind::Alpha, kTestImageSize);
    first.item->setImage(shared(image));
    first.item->setRenderSettings(
        settingsWith(RenderEnums::TextureSampling::Nearest));
    first.item->setPlacement(ImagePlacement{QPointF(kImageOrigin), 1.0});
    QSignalSpy errors(first.item.get(), &ImageRenderItem::renderError);
    RENDER(first, before);

    first.item->setParentItem(second.contentItem());
    QCoreApplication::processEvents();
    const QImage after = second.render();
    QVERIFY2(!after.isNull(), qPrintable(second.error()));
    QCOMPARE(maxDifference(before, after), 0);
    QCOMPARE(errors.count(), 0);
    // Detach before `second` is destroyed; the item itself goes with `first`.
    first.item->setParentItem(first.quick.contentItem());
  }

  //----------------------------------------------------------------------------
  // S1.2: colour adjustments, sharpening, exact downsample.

  void colorAdjustmentsMatchReference_data() {
    QTest::addColumn<ImageKind>("kind");
    QTest::addColumn<int>("magnification");
    QTest::addColumn<RenderEnums::TextureSampling>("sampling");
    const struct {
      const char *name;
      ImageKind kind;
    } kinds[] = {{"opaque", ImageKind::Opaque},
                 {"alpha", ImageKind::Alpha},
                 {"16-bit", ImageKind::Deep}};
    for (const auto &kind : kinds) {
      QTest::addRow("%s 100%% nearest", kind.name)
          << kind.kind << 1 << RenderEnums::TextureSampling::Nearest;
      QTest::addRow("%s 300%% bilinear", kind.name)
          << kind.kind << kLargestMagnification
          << RenderEnums::TextureSampling::Bilinear;
    }
  }

  void colorAdjustmentsMatchReference() {
    QFETCH(ImageKind, kind);
    QFETCH(int, magnification);
    QFETCH(RenderEnums::TextureSampling, sampling);

    Scene scene;
    CREATE_SCENE(scene, kFrameSize);
    QSignalSpy errors(scene.item.get(), &ImageRenderItem::renderError);
    const QImage image = makeTestImage(kind, kTestImageSize);
    const ImageFilter filter = filterWith(RenderEnums::Sharpening::None,
                                          kStrongCas, kNoCasContrast,
                                          kTestAdjustments);
    scene.item->setImage(shared(image));
    scene.item->setRenderSettings(settingsWith(sampling));
    scene.item->setImageFilter(filter);
    scene.item->setPlacement(
        ImagePlacement{QPointF(kImageOrigin), qreal(magnification)});
    RENDER(scene, frame);

    const FloatImage expected = composeOver(
        filteredMagnified(uploadedSource(image), magnification,
                          referenceParams(filter)),
        kFrameSize, kImageOrigin, kBackground);
    COMPARE_TO_REFERENCE(frame, expected, kColorTolerance);
    QCOMPARE(errors.count(), 0);
  }

  void sharpeningMatchesReference_data() {
    QTest::addColumn<ImageKind>("kind");
    QTest::addColumn<int>("magnification");
    QTest::addColumn<ImageFilter>("filter");
    const struct {
      const char *name;
      ImageKind kind;
    } kinds[] = {{"opaque", ImageKind::Opaque}, {"alpha", ImageKind::Alpha}};
    const struct {
      const char *name;
      ImageFilter filter;
    } filters[] = {
        {"cas", filterWith(RenderEnums::Sharpening::Cas)},
        {"cas mild", filterWith(RenderEnums::Sharpening::Cas, kMildCas,
                                kHighCasContrast)},
        {"smart", filterWith(RenderEnums::Sharpening::Smart)},
        {"cas + colour", filterWith(RenderEnums::Sharpening::Cas, kStrongCas,
                                    kNoCasContrast, kTestAdjustments)},
    };
    for (const auto &kind : kinds) {
      for (const auto &filter : filters) {
        for (const int magnification : {2, kLargestMagnification}) {
          QTest::addRow("%s %s %d00%%", kind.name, filter.name, magnification)
              << kind.kind << magnification << filter.filter;
        }
      }
    }
  }

  // Plain taps (at and above 1:1). Translucent pixels of the alpha image are
  // not sharpened; the reference applies the same opacity test.
  void sharpeningMatchesReference() {
    QFETCH(ImageKind, kind);
    QFETCH(int, magnification);
    QFETCH(ImageFilter, filter);

    Scene scene;
    CREATE_SCENE(scene, kFrameSize);
    QSignalSpy errors(scene.item.get(), &ImageRenderItem::renderError);
    const QImage image = makeTestImage(kind, kTestImageSize);
    scene.item->setImage(shared(image));
    scene.item->setRenderSettings(
        settingsWith(RenderEnums::TextureSampling::Trilinear));
    scene.item->setImageFilter(filter);
    scene.item->setPlacement(
        ImagePlacement{QPointF(kImageOrigin), qreal(magnification)});
    RENDER(scene, frame);

    const FloatImage expected = composeOver(
        filteredMagnified(uploadedSource(image), magnification,
                          referenceParams(filter)),
        kFrameSize, kImageOrigin, kBackground);
    COMPARE_TO_REFERENCE(frame, expected, kSharpenedTolerance);
    QCOMPARE(errors.count(), 0);
  }

  void noSharpeningAtOneToOne_data() {
    QTest::addColumn<RenderEnums::Sharpening>("sharpening");
    QTest::newRow("cas") << RenderEnums::Sharpening::Cas;
    QTest::newRow("smart") << RenderEnums::Sharpening::Smart;
  }

  void noSharpeningAtOneToOne() {
    QFETCH(RenderEnums::Sharpening, sharpening);
    Scene scene;
    CREATE_SCENE(scene, kFrameSize);
    const QImage image = makeTestImage(ImageKind::Opaque, kTestImageSize);
    scene.item->setImage(shared(image));
    scene.item->setRenderSettings(
        settingsWith(RenderEnums::TextureSampling::Trilinear));
    scene.item->setImageFilter(filterWith(sharpening));
    scene.item->setPlacement(ImagePlacement{QPointF(kImageOrigin), 1.0});
    RENDER(scene, frame);
    const FloatImage expected =
        expectedFrame(premultipliedSource(image),
                      ReferenceScene{kFrameSize, kImageOrigin, {}, {},
                                     kBackground});
    COMPARE_TO_REFERENCE(frame, expected, kExactTolerance);
  }

  void exactDownsampleMatchesReference_data() {
    QTest::addColumn<ImageKind>("kind");
    QTest::addColumn<qreal>("scale");
    QTest::addColumn<ImageFilter>("filter");
    QTest::addColumn<int>("tolerance");
    const struct {
      const char *name;
      ImageKind kind;
    } kinds[] = {{"opaque", ImageKind::Opaque},
                 {"alpha", ImageKind::Alpha},
                 {"16-bit", ImageKind::Deep}};
    const struct {
      const char *name;
      qreal scale;
    } scales[] = {{"37.5%", kTwoPassReduction}, {"62.5%", kOnePassReduction}};
    const struct {
      const char *name;
      ImageFilter filter;
      int tolerance;
    } filters[] = {
        {"plain", filterWith(RenderEnums::Sharpening::None), kReducedTolerance},
        {"cas", filterWith(RenderEnums::Sharpening::Cas), kSharpenedTolerance},
        {"smart", filterWith(RenderEnums::Sharpening::Smart),
         kSharpenedTolerance},
        {"colour", filterWith(RenderEnums::Sharpening::None, kStrongCas,
                              kNoCasContrast, kTestAdjustments),
         kColorTolerance},
    };
    for (const auto &kind : kinds) {
      for (const auto &scale : scales) {
        for (const auto &filter : filters) {
          QTest::addRow("%s %s %s", kind.name, scale.name, filter.name)
              << kind.kind << scale.scale << filter.filter << filter.tolerance;
        }
      }
    }
  }

  // Settled below 1:1: the image is reduced to its on-screen size by the
  // exact-area box chain, then filtered with the plain taps.
  void exactDownsampleMatchesReference() {
    QFETCH(ImageKind, kind);
    QFETCH(qreal, scale);
    QFETCH(ImageFilter, filter);
    QFETCH(int, tolerance);

    Scene scene;
    CREATE_SCENE(scene, kFrameSize);
    QSignalSpy errors(scene.item.get(), &ImageRenderItem::renderError);
    const QImage image = makeTestImage(kind, kTestImageSize);
    scene.item->setImage(shared(image));
    scene.item->setRenderSettings(
        settingsWith(RenderEnums::TextureSampling::Trilinear));
    scene.item->setImageFilter(filter);
    scene.item->setSettled(true);
    scene.item->setPlacement(ImagePlacement{QPointF(kImageOrigin), scale});
    RENDER(scene, frame);

    const QSize onScreen = (QSizeF(kTestImageSize) * scale).toSize();
    const FloatImage expected = composeOver(
        filteredMagnified(exactReduce(uploadedSource(image), onScreen), 1,
                          referenceParams(filter)),
        kFrameSize, kImageOrigin, kBackground);
    COMPARE_TO_REFERENCE(frame, expected, tolerance);
    QCOMPARE(errors.count(), 0);
  }

  // The downsample is only drawn while settled, follows panning from its
  // cache and is dropped in favour of the mip chain while interacting.
  void exactDownsampleFollowsSettledState() {
    Scene scene;
    CREATE_SCENE(scene, kFrameSize);
    QSignalSpy errors(scene.item.get(), &ImageRenderItem::renderError);
    const QImage image = makeTestImage(ImageKind::Opaque, kTestImageSize);
    scene.item->setImage(shared(image));
    scene.item->setRenderSettings(
        settingsWith(RenderEnums::TextureSampling::Trilinear));
    scene.item->setPlacement(
        ImagePlacement{QPointF(kImageOrigin), kTwoPassReduction});
    RENDER(scene, interacting);

    scene.item->setSettled(true);
    RENDER(scene, settled);
    QVERIFY(maxDifference(interacting, settled) > 0);

    const QSize onScreen =
        (QSizeF(kTestImageSize) * kTwoPassReduction).toSize();
    const FloatImage reduced = exactReduce(uploadedSource(image), onScreen);
    const QPoint panned = kImageOrigin + kPanOffset;
    scene.item->setPlacement(
        ImagePlacement{QPointF(panned), kTwoPassReduction});
    RENDER(scene, pannedFrame);
    COMPARE_TO_REFERENCE(
        pannedFrame,
        composeOver(reduced, kFrameSize, panned, kBackground),
        kReducedTolerance);

    scene.item->setSettled(false);
    scene.item->setPlacement(
        ImagePlacement{QPointF(kImageOrigin), kTwoPassReduction});
    RENDER(scene, interactingAgain);
    QCOMPARE(maxDifference(interacting, interactingAgain), 0);
    QCOMPARE(errors.count(), 0);
  }

  void tiledExactDownsampleMatchesUntiled_data() {
    QTest::addColumn<qreal>("scale");
    QTest::newRow("75%") << kTiledFractionalReduction;
    QTest::newRow("25%") << kTiledQuarterReduction;
  }

  void tiledExactDownsampleMatchesUntiled() {
    QFETCH(qreal, scale);
    const QSize frameSize = (QSizeF(kTiledImageSize) * scale).toSize() +
                            QSize(2 * kMargin, 2 * kMargin);
    Scene scene;
    CREATE_SCENE(scene, frameSize);
    QSignalSpy errors(scene.item.get(), &ImageRenderItem::renderError);
    scene.item->setImage(shared(makeTestImage(ImageKind::Alpha, kTiledImageSize)));
    scene.item->setPlacement(ImagePlacement{QPointF(kImageOrigin), scale});
    scene.item->setImageFilter(filterWith(RenderEnums::Sharpening::Cas));
    scene.item->setSettled(true);
    scene.item->setRenderSettings(
        settingsWith(RenderEnums::TextureSampling::Trilinear));
    RENDER(scene, untiled);
    scene.item->setRenderSettings(settingsWith(
        RenderEnums::TextureSampling::Trilinear, false, kSmallTileLimit));
    RENDER(scene, tiled);
    QVERIFY(TileGrid::layout(kTiledImageSize, kSmallTileLimit).size() > 1);
    QVERIFY2(maxDifference(tiled, untiled) <= kTileSeamTolerance,
             qPrintable(u"max difference %1"_s.arg(maxDifference(tiled, untiled))));
    QCOMPARE(errors.count(), 0);
  }

  void downscaleSharpeningKeepsFlatImage_data() {
    QTest::addColumn<RenderEnums::Sharpening>("sharpening");
    QTest::newRow("cas") << RenderEnums::Sharpening::Cas;
    QTest::newRow("smart") << RenderEnums::Sharpening::Smart;
  }

  // Not settled below 1:1: the widened, mip-biased taps of a flat image all
  // read the same colour, so sharpening must leave it unchanged.
  void downscaleSharpeningKeepsFlatImage() {
    QFETCH(RenderEnums::Sharpening, sharpening);
    Scene scene;
    CREATE_SCENE(scene, kFrameSize);
    QImage flat(kTestImageSize, QImage::Format_RGB32);
    flat.fill(kFlatColor);
    scene.item->setImage(shared(flat));
    scene.item->setRenderSettings(
        settingsWith(RenderEnums::TextureSampling::Trilinear));
    scene.item->setImageFilter(filterWith(sharpening));
    scene.item->setPlacement(
        ImagePlacement{QPointF(kImageOrigin), kUnsettledReduction});
    RENDER(scene, frame);

    // Whole device pixels inside the image.
    const QSize inside =
        (QSizeF(kTestImageSize) * kUnsettledReduction).toSize() - QSize(1, 1);
    FloatImage drawn(inside);
    const Rgba flatColor{kFlatColor.redF(), kFlatColor.greenF(),
                         kFlatColor.blueF(), 1.0};
    for (int y = 0; y < inside.height(); ++y) {
      for (int x = 0; x < inside.width(); ++x)
        drawn.at(x, y) = flatColor;
    }
    const QImage insideFrame = frame.copy(QRect(kImageOrigin, inside));
    COMPARE_TO_REFERENCE(insideFrame, drawn, kExactTolerance);
  }

  // Not settled below 1:1: sharpening changes a noisy grey image visibly but
  // keeps it grey (one weight for all channels).
  void downscaleSharpeningKeepsGrey_data() {
    downscaleSharpeningKeepsFlatImage_data();
  }

  void downscaleSharpeningKeepsGrey() {
    QFETCH(RenderEnums::Sharpening, sharpening);
    Scene scene;
    CREATE_SCENE(scene, kFrameSize);
    QSignalSpy errors(scene.item.get(), &ImageRenderItem::renderError);
    scene.item->setImage(shared(makeTestImage(ImageKind::Grey, kTestImageSize)));
    scene.item->setRenderSettings(
        settingsWith(RenderEnums::TextureSampling::Trilinear));
    scene.item->setPlacement(
        ImagePlacement{QPointF(kImageOrigin), kUnsettledReduction});
    RENDER(scene, unsharpened);
    scene.item->setImageFilter(filterWith(sharpening));
    RENDER(scene, sharpened);

    QVERIFY(maxDifference(unsharpened, sharpened) >= kVisibleSharpening);
    const QSize inside =
        (QSizeF(kTestImageSize) * kUnsettledReduction).toSize() - QSize(1, 1);
    for (int y = 0; y < inside.height(); ++y) {
      for (int x = 0; x < inside.width(); ++x) {
        const QColor color =
            sharpened.pixelColor(kImageOrigin + QPoint(x, y));
        if (std::abs(color.red() - color.green()) > kExactTolerance ||
            std::abs(color.green() - color.blue()) > kExactTolerance) {
          QFAIL(qPrintable(u"pixel (%1, %2) is not grey: %3"_s.arg(x).arg(y).arg(
              color.name())));
        }
      }
    }
    QCOMPARE(errors.count(), 0);
  }

  void mks2021MatchesReference_data() {
    QTest::addColumn<ImageKind>("kind");
    QTest::addColumn<qreal>("scale");
    const struct {
      const char *name;
      ImageKind kind;
    } kinds[] = {{"opaque", ImageKind::Opaque}, {"alpha", ImageKind::Alpha}};
    const struct {
      const char *name;
      qreal scale;
    } scales[] = {{"300%", kMksWholeUpscale},
                  {"170%", kMksFractionalUpscale},
                  {"50%", kMksHalf},
                  {"37%", kMksFractionalReduction},
                  {"20%", kMksStrongReduction}};
    for (const auto &kind : kinds) {
      for (const auto &scale : scales)
        QTest::addRow("%s %s", kind.name, scale.name) << kind.kind << scale.scale;
    }
  }

  // Settled with MKS2021 selected: the image is resampled to its on-screen
  // size with the CPU's kernel and taps and drawn 1:1.
  void mks2021MatchesReference() {
    QFETCH(ImageKind, kind);
    QFETCH(qreal, scale);

    Scene scene;
    CREATE_SCENE(scene, kFrameSize);
    QSignalSpy errors(scene.item.get(), &ImageRenderItem::renderError);
    const QImage image = makeTestImage(kind, kTestImageSize);
    scene.item->setImage(shared(image));
    scene.item->setRenderSettings(
        settingsWith(RenderEnums::TextureSampling::Trilinear));
    scene.item->setResampling(RenderEnums::Resampling::Mks2021);
    scene.item->setSettled(true);
    scene.item->setPlacement(ImagePlacement{QPointF(kImageOrigin), scale});
    RENDER(scene, frame);

    const FloatImage expected = composeOver(
        mks2021Resample(uploadedSource(image),
                        ResampleGrid::outputSize(kTestImageSize, scale)),
        kFrameSize, kImageOrigin, kBackground);
    COMPARE_TO_REFERENCE(frame, expected, kMks2021Tolerance);
    QCOMPARE(errors.count(), 0);
  }

  // The resampling is only drawn while settled, is rebuilt for a panned view
  // and is replaced by the mip chain while interacting.
  void mks2021FollowsSettledState() {
    Scene scene;
    CREATE_SCENE(scene, kFrameSize);
    QSignalSpy errors(scene.item.get(), &ImageRenderItem::renderError);
    const QImage image = makeTestImage(ImageKind::Opaque, kTestImageSize);
    scene.item->setImage(shared(image));
    scene.item->setRenderSettings(
        settingsWith(RenderEnums::TextureSampling::Trilinear));
    scene.item->setPlacement(
        ImagePlacement{QPointF(kImageOrigin), kMksFractionalUpscale});
    RENDER(scene, plain);
    scene.item->setResampling(RenderEnums::Resampling::Mks2021);
    RENDER(scene, interacting);
    QCOMPARE(maxDifference(plain, interacting), 0);

    scene.item->setSettled(true);
    RENDER(scene, settled);
    QVERIFY(maxDifference(interacting, settled) > 0);

    const FloatImage resampled = mks2021Resample(
        uploadedSource(image),
        ResampleGrid::outputSize(kTestImageSize, kMksFractionalUpscale));
    const QPoint panned = kImageOrigin + kPanOffset;
    scene.item->setPlacement(
        ImagePlacement{QPointF(panned), kMksFractionalUpscale});
    RENDER(scene, pannedFrame);
    COMPARE_TO_REFERENCE(pannedFrame,
                         composeOver(resampled, kFrameSize, panned, kBackground),
                         kMks2021Tolerance);

    scene.item->setSettled(false);
    scene.item->setPlacement(
        ImagePlacement{QPointF(kImageOrigin), kMksFractionalUpscale});
    RENDER(scene, interactingAgain);
    QCOMPARE(maxDifference(interacting, interactingAgain), 0);
    QCOMPARE(errors.count(), 0);
  }

  // At 1:1 nothing is resampled.
  void mks2021AtOneToOneIsUnfiltered() {
    Scene scene;
    CREATE_SCENE(scene, kFrameSize);
    scene.item->setImage(shared(makeTestImage(ImageKind::Alpha, kTestImageSize)));
    scene.item->setRenderSettings(
        settingsWith(RenderEnums::TextureSampling::Trilinear));
    scene.item->setSettled(true);
    scene.item->setPlacement(ImagePlacement{QPointF(kImageOrigin), 1.0});
    RENDER(scene, plain);
    scene.item->setResampling(RenderEnums::Resampling::Mks2021);
    RENDER(scene, resampled);
    QCOMPARE(maxDifference(plain, resampled), 0);
  }

  // A strongly magnified image whose output is far larger than the frame:
  // only the visible window is computed, at the right position.
  void mks2021ResamplesVisiblePart() {
    Scene scene;
    CREATE_SCENE(scene, kMksPartialFrameSize);
    QSignalSpy errors(scene.item.get(), &ImageRenderItem::renderError);
    const QImage image = makeTestImage(ImageKind::Opaque, kMksPartialImageSize);
    scene.item->setImage(shared(image));
    scene.item->setRenderSettings(
        settingsWith(RenderEnums::TextureSampling::Trilinear));
    scene.item->setResampling(RenderEnums::Resampling::Mks2021);
    scene.item->setSettled(true);
    scene.item->setPlacement(
        ImagePlacement{QPointF(kMksPartialOrigin), kMksPartialScale});
    RENDER(scene, frame);

    const FloatImage expected = composeOver(
        mks2021Resample(uploadedSource(image),
                        ResampleGrid::outputSize(kMksPartialImageSize,
                                                 kMksPartialScale)),
        kMksPartialFrameSize, kMksPartialOrigin, kBackground);
    COMPARE_TO_REFERENCE(frame, expected, kMks2021Tolerance);
    QCOMPARE(errors.count(), 0);
  }

  void tiledMks2021MatchesUntiled_data() {
    QTest::addColumn<qreal>("scale");
    QTest::newRow("50%") << kMksTiledReduction;
    QTest::newRow("160%") << kMksTiledUpscale;
  }

  void tiledMks2021MatchesUntiled() {
    QFETCH(qreal, scale);
    const QSize frameSize = (QSizeF(kTiledImageSize) * scale).toSize() +
                            QSize(2 * kMargin, 2 * kMargin);
    Scene scene;
    CREATE_SCENE(scene, frameSize);
    QSignalSpy errors(scene.item.get(), &ImageRenderItem::renderError);
    scene.item->setImage(shared(makeTestImage(ImageKind::Alpha, kTiledImageSize)));
    scene.item->setPlacement(ImagePlacement{QPointF(kImageOrigin), scale});
    scene.item->setResampling(RenderEnums::Resampling::Mks2021);
    scene.item->setSettled(true);
    scene.item->setRenderSettings(
        settingsWith(RenderEnums::TextureSampling::Trilinear));
    RENDER(scene, untiled);
    scene.item->setRenderSettings(settingsWith(
        RenderEnums::TextureSampling::Trilinear, false, kSmallTileLimit));
    RENDER(scene, tiled);
    QVERIFY(TileGrid::layout(kTiledImageSize, kSmallTileLimit).size() > 1);
    QVERIFY2(maxDifference(tiled, untiled) <= kExactTolerance,
             qPrintable(u"max difference %1"_s.arg(maxDifference(tiled, untiled))));
    QCOMPARE(errors.count(), 0);
  }

  void mks2021SettleLatency_data() {
    QTest::addColumn<qreal>("scale");
    QTest::newRow("fit") << kLatencyFitScale;
    QTest::newRow("300%") << kLatencyZoomScale;
  }

  // Logs the cost of the settled MKS2021 frame over an unsettled one (render
  // and read back), fastest of several rebuilds.
  void mks2021SettleLatency() {
    QFETCH(qreal, scale);
    Scene scene;
    CREATE_SCENE(scene, kLatencyFrameSize);
    QSignalSpy errors(scene.item.get(), &ImageRenderItem::renderError);
    QImage image(kLatencyImageSize, QImage::Format_RGB32);
    QRandomGenerator generator(kImageSeed);
    for (int y = 0; y < image.height(); ++y) {
      auto *line = reinterpret_cast<quint32 *>(image.scanLine(y));
      generator.fillRange(line, image.width());
      // RGB32 requires 0xFF in the unused byte; it is uploaded as is.
      for (int x = 0; x < image.width(); ++x)
        line[x] |= kRgb32OpaqueBits;
    }
    scene.item->setImage(shared(std::move(image)));
    scene.item->setRenderSettings(
        settingsWith(RenderEnums::TextureSampling::Trilinear));
    scene.item->setResampling(RenderEnums::Resampling::Mks2021);
    scene.item->setPlacement(ImagePlacement{QPointF(), scale});
    RENDER(scene, warmup);

    QList<qint64> unsettled;
    QList<qint64> settled;
    QElapsedTimer timer;
    for (int run = 0; run < kLatencyRuns; ++run) {
      scene.item->setSettled(false);
      scene.item->setPlacement(
          ImagePlacement{QPointF(), scale + run * kLatencyScaleStep});
      timer.start();
      RENDER(scene, moving);
      unsettled.append(timer.nsecsElapsed());
      scene.item->setSettled(true);
      timer.start();
      RENDER(scene, resampled);
      settled.append(timer.nsecsElapsed());
    }
    constexpr double kNsPerMs = 1e6;
    const double unsettledMs =
        *std::min_element(unsettled.begin(), unsettled.end()) / kNsPerMs;
    const double settledMs =
        *std::min_element(settled.begin(), settled.end()) / kNsPerMs;
    qInfo().noquote() << u"MKS2021 %1 x %2 at %3: unsettled frame %4 ms, "
                         u"settled (resampled) frame %5 ms"_s
                             .arg(kLatencyImageSize.width())
                             .arg(kLatencyImageSize.height())
                             .arg(scale)
                             .arg(unsettledMs, 0, 'f', 2)
                             .arg(settledMs, 0, 'f', 2);
    QCOMPARE(errors.count(), 0);
  }

  void hdrToneMappingMatchesCpu_data() {
    QTest::addColumn<HdrKind>("kind");
    QTest::addColumn<ToneMapping>("toneMapping");
    const struct {
      const char *name;
      HdrKind kind;
    } kinds[] = {{"PQ", HdrKind::Pq},
                 {"PQ alpha", HdrKind::PqAlpha},
                 {"HLG", HdrKind::Hlg},
                 {"linear half", HdrKind::LinearHalf},
                 {"linear float sRGB", HdrKind::LinearFloat}};
    const struct {
      const char *name;
      RenderEnums::ToneMapOperator op;
    } operators[] = {{"BT.2408", RenderEnums::ToneMapOperator::Bt2408},
                     {"Reinhard-Jodie",
                      RenderEnums::ToneMapOperator::ReinhardJodie},
                     {"ACES", RenderEnums::ToneMapOperator::AcesFilmic},
                     {"Hable", RenderEnums::ToneMapOperator::Hable}};
    for (const auto &kind : kinds) {
      for (const auto &op : operators) {
        QTest::addRow("%s %s", kind.name, op.name)
            << kind.kind
            << ToneMapping{true, op.op, ToneMapping::kDefaultWhiteNits};
      }
      QTest::addRow("%s BT.2408 400 nits", kind.name)
          << kind.kind
          << ToneMapping{true, RenderEnums::ToneMapOperator::Bt2408,
                         kBrightWhiteNits};
      QTest::addRow("%s off", kind.name)
          << kind.kind
          << ToneMapping{false, RenderEnums::ToneMapOperator::Bt2408,
                         ToneMapping::kDefaultWhiteNits};
    }
  }

  // The conversion pass matches HdrToneMapper (and, with tone mapping off,
  // the CPU's clamping fallback) texel for texel.
  void hdrToneMappingMatchesCpu() {
    QFETCH(HdrKind, kind);
    QFETCH(ToneMapping, toneMapping);
    Scene scene;
    CREATE_SCENE(scene, kFrameSize);
    QSignalSpy errors(scene.item.get(), &ImageRenderItem::renderError);
    const QImage image = makeHdrImage(kind, kTestImageSize);
    scene.item->setToneMapping(toneMapping);
    scene.item->setImage(shared(image));
    scene.item->setRenderSettings(
        settingsWith(RenderEnums::TextureSampling::Nearest));
    scene.item->setPlacement(ImagePlacement{QPointF(kImageOrigin), 1.0});
    QVERIFY(scene.item->sourceConversion().hdr);
    RENDER(scene, frame);
    COMPARE_TO_REFERENCE(frame, oneToOneFrame(cpuToneMapped(image, toneMapping)),
                         kToneMapTolerance);
    QCOMPARE(errors.count(), 0);
  }

  // A new operator or white level only re-runs the conversion of the
  // uploaded HDR source, with the same result as a fresh image.
  void toneMappingChangeReconverts() {
    Scene scene;
    CREATE_SCENE(scene, kFrameSize);
    QSignalSpy errors(scene.item.get(), &ImageRenderItem::renderError);
    const QImage image = makeHdrImage(HdrKind::Pq, kTestImageSize);
    scene.item->setImage(shared(image));
    scene.item->setRenderSettings(
        settingsWith(RenderEnums::TextureSampling::Nearest));
    scene.item->setPlacement(ImagePlacement{QPointF(kImageOrigin), 1.0});
    RENDER(scene, first);

    QSignalSpy imageChanges(scene.item.get(), &ImageRenderItem::imageChanged);
    scene.item->setToneMapOperator(RenderEnums::ToneMapOperator::Hable);
    scene.item->setHdrWhiteLevel(kBrightWhiteNits);
    RENDER(scene, hable);
    COMPARE_TO_REFERENCE(hable,
                         oneToOneFrame(cpuToneMapped(
                             image, scene.item->toneMapping())),
                         kToneMapTolerance);
    QVERIFY(maxDifference(first, hable) > 0);

    scene.item->setToneMappingEnabled(false);
    RENDER(scene, clamped);
    COMPARE_TO_REFERENCE(clamped,
                         oneToOneFrame(cpuToneMapped(
                             image, scene.item->toneMapping())),
                         kToneMapTolerance);

    scene.item->setToneMapping(ToneMapping{});
    RENDER(scene, again);
    QCOMPARE(maxDifference(first, again), 0);
    QCOMPARE(imageChanges.count(), 0);
    QCOMPARE(errors.count(), 0);
  }

  void colorManagementMatchesQt_data() {
    QTest::addColumn<ImageKind>("kind");
    QTest::addColumn<QColorSpace>("source");
    QTest::addColumn<QColorSpace>("target");
    QTest::addColumn<int>("tolerance");
    const QColorSpace srgb(QColorSpace::SRgb);
    const struct {
      const char *name;
      QColorSpace space;
      int tolerance;
    } targets[] = {{"Display P3", QColorSpace(QColorSpace::DisplayP3),
                    kColorManagementTolerance},
                   {"Adobe RGB", QColorSpace(QColorSpace::AdobeRgb),
                    kPureGammaShadowTolerance},
                   {"ProPhoto", QColorSpace(QColorSpace::ProPhotoRgb),
                    kColorManagementTolerance},
                   {"Rec2020", rec2020Gamma22(), kPureGammaShadowTolerance},
                   {"linear sRGB", QColorSpace(QColorSpace::SRgbLinear),
                    kColorManagementTolerance}};
    const struct {
      const char *name;
      ImageKind kind;
    } kinds[] = {{"opaque", ImageKind::Opaque},
                 {"alpha", ImageKind::Alpha},
                 {"16-bit", ImageKind::Deep}};
    for (const auto &kind : kinds) {
      for (const auto &target : targets) {
        QTest::addRow("%s sRGB -> %s", kind.name, target.name)
            << kind.kind << srgb << target.space << target.tolerance;
      }
    }
    QTest::newRow("opaque Adobe RGB -> sRGB")
        << ImageKind::Opaque << QColorSpace(QColorSpace::AdobeRgb) << srgb
        << kColorManagementTolerance;
    QTest::newRow("opaque untagged -> Display P3")
        << ImageKind::Opaque << QColorSpace()
        << QColorSpace(QColorSpace::DisplayP3) << kColorManagementTolerance;
  }

  // Parametric colour management matches what ColorManager does on the CPU.
  void colorManagementMatchesQt() {
    QFETCH(ImageKind, kind);
    QFETCH(QColorSpace, source);
    QFETCH(QColorSpace, target);
    QFETCH(int, tolerance);
    Scene scene;
    CREATE_SCENE(scene, kFrameSize);
    QSignalSpy errors(scene.item.get(), &ImageRenderItem::renderError);
    QImage image = makeTestImage(kind, kTestImageSize);
    image.setColorSpace(source);
    scene.item->setColorManagement(managedFor(target));
    scene.item->setImage(shared(image));
    scene.item->setRenderSettings(
        settingsWith(RenderEnums::TextureSampling::Nearest));
    scene.item->setPlacement(ImagePlacement{QPointF(kImageOrigin), 1.0});
    QCOMPARE(scene.item->sourceConversion().color.kind,
             ColorTransformKind::Parametric);
    RENDER(scene, frame);
    COMPARE_TO_REFERENCE(frame, oneToOneFrame(cpuColorManaged(image, target)),
                         tolerance);
    QCOMPARE(errors.count(), 0);
  }

  // A display profile without a parametric form goes through a lookup table
  // built on a worker thread; until it is ready the image is unconverted.
  void colorLutMatchesQt() {
    Scene scene;
    CREATE_SCENE(scene, kFrameSize);
    QSignalSpy errors(scene.item.get(), &ImageRenderItem::renderError);
    const QImage image = makeTestImage(ImageKind::Alpha, kTestImageSize);
    const QColorSpace target = tableCurveSpace();
    scene.item->setColorManagement(managedFor(target));
    scene.item->setImage(shared(image));
    scene.item->setRenderSettings(
        settingsWith(RenderEnums::TextureSampling::Nearest));
    scene.item->setPlacement(ImagePlacement{QPointF(kImageOrigin), 1.0});
    QCOMPARE(scene.item->sourceConversion().color.kind,
             ColorTransformKind::Lut);
    QTRY_VERIFY_WITH_TIMEOUT(scene.item->sourceConversion().lut != nullptr,
                             kLutWaitTimeoutMs);
    RENDER(scene, frame);
    COMPARE_TO_REFERENCE(frame, oneToOneFrame(cpuColorManaged(image, target)),
                         kColorLutTolerance);
    QCOMPARE(errors.count(), 0);
  }

  void hdrWithColorManagementMatchesCpu_data() {
    QTest::addColumn<HdrKind>("kind");
    QTest::addColumn<QColorSpace>("target");
    QTest::newRow("PQ -> Display P3")
        << HdrKind::Pq << QColorSpace(QColorSpace::DisplayP3);
    QTest::newRow("HLG -> Adobe RGB")
        << HdrKind::Hlg << QColorSpace(QColorSpace::AdobeRgb);
    QTest::newRow("linear -> table curve")
        << HdrKind::LinearHalf << tableCurveSpace();
  }

  // HDR images are tone mapped to sRGB first, then colour managed, like
  // HdrToneMapper followed by ColorManager.
  void hdrWithColorManagementMatchesCpu() {
    QFETCH(HdrKind, kind);
    QFETCH(QColorSpace, target);
    Scene scene;
    CREATE_SCENE(scene, kFrameSize);
    QSignalSpy errors(scene.item.get(), &ImageRenderItem::renderError);
    const QImage image = makeHdrImage(kind, kTestImageSize);
    scene.item->setColorManagement(managedFor(target));
    scene.item->setImage(shared(image));
    scene.item->setRenderSettings(
        settingsWith(RenderEnums::TextureSampling::Nearest));
    scene.item->setPlacement(ImagePlacement{QPointF(kImageOrigin), 1.0});
    QTRY_VERIFY_WITH_TIMEOUT(
        scene.item->sourceConversion().appliesColorTransform(),
        kLutWaitTimeoutMs);
    RENDER(scene, frame);
    const int tolerance =
        scene.item->sourceConversion().color.kind == ColorTransformKind::Lut
            ? kColorLutTolerance + kToneMapTolerance
            : kHdrColorManagementTolerance;
    COMPARE_TO_REFERENCE(
        frame,
        oneToOneFrame(cpuColorManaged(cpuToneMapped(image, ToneMapping{}),
                                      target)),
        tolerance);
    QCOMPARE(errors.count(), 0);
  }

  // An sRGB image on an sRGB display needs no conversion: the plain upload
  // path is used, with an identical result.
  void colorManagementToSameSpaceIsUnconverted() {
    Scene scene;
    CREATE_SCENE(scene, kFrameSize);
    const QImage image = makeTestImage(ImageKind::Alpha, kTestImageSize);
    scene.item->setImage(shared(image));
    scene.item->setRenderSettings(
        settingsWith(RenderEnums::TextureSampling::Trilinear));
    scene.item->setPlacement(ImagePlacement{QPointF(kImageOrigin), 0.5});
    RENDER(scene, plain);
    scene.item->setColorManagement(
        managedFor(QColorSpace(QColorSpace::SRgb)));
    QVERIFY(!scene.item->sourceConversion().isActive());
    RENDER(scene, managed);
    QCOMPARE(maxDifference(plain, managed), 0);
  }

  // SDR sources are released after their conversion: a new display colour
  // space re-uploads them, and turning colour management off returns to the
  // plain path.
  void colorManagementChangeReuploads() {
    Scene scene;
    CREATE_SCENE(scene, kFrameSize);
    QSignalSpy errors(scene.item.get(), &ImageRenderItem::renderError);
    const QImage image = makeTestImage(ImageKind::Opaque, kTestImageSize);
    scene.item->setImage(shared(image));
    scene.item->setRenderSettings(
        settingsWith(RenderEnums::TextureSampling::Nearest));
    scene.item->setPlacement(ImagePlacement{QPointF(kImageOrigin), 1.0});
    RENDER(scene, plain);
    const QColorSpace p3(QColorSpace::DisplayP3);
    const QColorSpace adobe(QColorSpace::AdobeRgb);
    scene.item->setColorManagement(managedFor(p3));
    RENDER(scene, inP3);
    COMPARE_TO_REFERENCE(inP3, oneToOneFrame(cpuColorManaged(image, p3)),
                         kColorManagementTolerance);
    scene.item->setColorManagement(managedFor(adobe));
    RENDER(scene, inAdobe);
    COMPARE_TO_REFERENCE(inAdobe, oneToOneFrame(cpuColorManaged(image, adobe)),
                         kPureGammaShadowTolerance);
    scene.item->setColorManagement(ColorManagement{false, adobe});
    RENDER(scene, unmanaged);
    QCOMPARE(maxDifference(plain, unmanaged), 0);
    QCOMPARE(errors.count(), 0);
  }

  // A display colour space that cannot be a target is reported once; the
  // image is shown unconverted.
  void invalidTargetIsReported() {
    Scene scene;
    CREATE_SCENE(scene, kFrameSize);
    QSignalSpy errors(scene.item.get(), &ImageRenderItem::renderError);
    const QImage image = makeTestImage(ImageKind::Opaque, kTestImageSize);
    scene.item->setImage(shared(image));
    scene.item->setRenderSettings(
        settingsWith(RenderEnums::TextureSampling::Nearest));
    scene.item->setPlacement(ImagePlacement{QPointF(kImageOrigin), 1.0});
    RENDER(scene, plain);
    scene.item->setColorManagement(managedFor(QColorSpace()));
    RENDER(scene, managed);
    QCOMPARE(maxDifference(plain, managed), 0);
    scene.item->setImage(shared(makeTestImage(ImageKind::Alpha, kTestImageSize)));
    RENDER(scene, next);
    QCOMPARE(errors.count(), 1);
  }

  // Each tile converts its own texels; the tiles meet without seams.
  void tiledConversionMatchesUntiled() {
    const QSize frameSize = kTiledImageSize + QSize(2 * kMargin, 2 * kMargin);
    Scene scene;
    CREATE_SCENE(scene, frameSize);
    QSignalSpy errors(scene.item.get(), &ImageRenderItem::renderError);
    scene.item->setImage(shared(makeHdrImage(HdrKind::PqAlpha, kTiledImageSize)));
    scene.item->setColorManagement(
        managedFor(QColorSpace(QColorSpace::DisplayP3)));
    scene.item->setPlacement(ImagePlacement{QPointF(kImageOrigin), 1.0});
    scene.item->setRenderSettings(
        settingsWith(RenderEnums::TextureSampling::Nearest));
    RENDER(scene, untiled);
    scene.item->setRenderSettings(settingsWith(
        RenderEnums::TextureSampling::Nearest, false, kSmallTileLimit));
    RENDER(scene, tiled);
    QVERIFY(TileGrid::layout(kTiledImageSize, kSmallTileLimit).size() > 1);
    QCOMPARE(maxDifference(tiled, untiled), 0);
    QCOMPARE(errors.count(), 0);
  }

  void convertedImageIsFiltered_data() {
    QTest::addColumn<qreal>("scale");
    QTest::addColumn<RenderEnums::Resampling>("resampling");
    QTest::newRow("exact downsample 62.5%")
        << kOnePassReduction << RenderEnums::Resampling::None;
    QTest::newRow("MKS2021 50%") << kMksHalf << RenderEnums::Resampling::Mks2021;
    QTest::newRow("MKS2021 170%")
        << kMksFractionalUpscale << RenderEnums::Resampling::Mks2021;
  }

  // The mip chain, the exact downsample and the resampling read the
  // converted texels, like the CPU filters read the tone-mapped image.
  void convertedImageIsFiltered() {
    QFETCH(qreal, scale);
    QFETCH(RenderEnums::Resampling, resampling);
    Scene scene;
    CREATE_SCENE(scene, kFrameSize);
    QSignalSpy errors(scene.item.get(), &ImageRenderItem::renderError);
    const QImage image = makeHdrImage(HdrKind::Pq, kTestImageSize);
    scene.item->setImage(shared(image));
    scene.item->setRenderSettings(
        settingsWith(RenderEnums::TextureSampling::Trilinear));
    scene.item->setResampling(resampling);
    scene.item->setSettled(true);
    scene.item->setPlacement(ImagePlacement{QPointF(kImageOrigin), scale});
    RENDER(scene, frame);

    const FloatImage toneMapped =
        premultipliedSource(cpuToneMapped(image, ToneMapping{}));
    const FloatImage filtered =
        resampling == RenderEnums::Resampling::Mks2021
            ? mks2021Resample(toneMapped,
                              ResampleGrid::outputSize(kTestImageSize, scale))
            : exactReduce(toneMapped,
                          (QSizeF(kTestImageSize) * scale).toSize());
    COMPARE_TO_REFERENCE(frame,
                         composeOver(filtered, kFrameSize, kImageOrigin,
                                     kBackground),
                         kConvertedFilterTolerance);
    QCOMPARE(errors.count(), 0);
  }

  // Animation frames of the same size and format go into the textures of
  // the previous frame; converted sources are kept for the next frame.
  void animationFramesReuseTextures_data() {
    QTest::addColumn<bool>("managed");
    QTest::newRow("plain") << false;
    QTest::newRow("colour managed") << true;
  }

  void animationFramesReuseTextures() {
    QFETCH(bool, managed);
    Scene scene;
    CREATE_SCENE(scene, kFrameSize);
    QSignalSpy errors(scene.item.get(), &ImageRenderItem::renderError);
    const QColorSpace p3(QColorSpace::DisplayP3);
    if (managed)
      scene.item->setColorManagement(managedFor(p3));
    scene.item->setRenderSettings(
        settingsWith(RenderEnums::TextureSampling::Nearest));
    scene.item->setPlacement(ImagePlacement{QPointF(kImageOrigin), 1.0});
    const QImage first = makeTestImage(ImageKind::Opaque, kTestImageSize);
    const QImage second = first.mirrored(true, false);
    const QImage third = first.mirrored(false, true);
    scene.item->setImage(shared(first));
    RENDER(scene, firstFrame);
    scene.item->setImage(shared(second),
                         ImageRenderItem::ImageUpdate::AnimationFrame);
    RENDER(scene, secondFrame);
    const RenderStatisticsSnapshot afterSecond = scene.item->statistics();
    scene.item->setImage(shared(third),
                         ImageRenderItem::ImageUpdate::AnimationFrame);
    RENDER(scene, thirdFrame);
    const RenderStatisticsSnapshot afterThird = scene.item->statistics();
    QCOMPARE(afterThird.imageTexturesCreated, afterSecond.imageTexturesCreated);
    QCOMPARE(afterThird.imageUploads, afterSecond.imageUploads + 1);
    if (managed) {
      COMPARE_TO_REFERENCE(thirdFrame,
                           oneToOneFrame(cpuColorManaged(third, p3)),
                           kColorManagementTolerance);
    } else {
      COMPARE_TO_REFERENCE(thirdFrame, oneToOneFrame(third), kExactTolerance);
      COMPARE_TO_REFERENCE(secondFrame, oneToOneFrame(second),
                           kExactTolerance);
    }

    // A frame of another size needs new textures.
    scene.item->setImage(
        shared(makeTestImage(ImageKind::Opaque, kTestImageSize / 2)),
        ImageRenderItem::ImageUpdate::AnimationFrame);
    RENDER(scene, smaller);
    QVERIFY(scene.item->statistics().imageTexturesCreated >
            afterThird.imageTexturesCreated);
    QCOMPARE(errors.count(), 0);
  }

  void panoramaMatchesReference_data() {
    QTest::addColumn<ReferencePanorama>("camera");
    QTest::addColumn<bool>("adjusted");
    QTest::newRow("front") << kPanoramaFront << false;
    QTest::newRow("turned up") << ReferencePanorama{135.0, 30.0, 70.0} << false;
    QTest::newRow("back seam, down")
        << ReferencePanorama{180.0, -30.0, 100.0} << false;
    QTest::newRow("adjusted") << ReferencePanorama{45.0, 10.0, 90.0} << true;
  }

  void panoramaMatchesReference() {
    QFETCH(ReferencePanorama, camera);
    QFETCH(bool, adjusted);
    Scene scene;
    CREATE_SCENE(scene, kPanoramaFrameSize);
    QSignalSpy errors(scene.item.get(), &ImageRenderItem::renderError);
    const QImage image = makePanoramaImage();
    scene.item->setImage(shared(image));
    scene.item->setRenderSettings(
        settingsWith(RenderEnums::TextureSampling::Bilinear));
    scene.item->setProjection(RenderEnums::Projection::Equirectangular);
    scene.item->setPanoramaCamera(PanoramaCamera{
        float(camera.yaw), float(camera.pitch), float(camera.fov)});
    std::optional<ColorMatrix> color;
    if (adjusted) {
      scene.item->setColorAdjustments(kTestAdjustments);
      color = colorAdjustmentMatrix(kTestAdjustments);
    }
    RENDER(scene, frame);
    COMPARE_TO_REFERENCE(
        frame,
        panoramaFrame(premultipliedSource(image), kPanoramaFrameSize, camera,
                      color),
        adjusted ? kPanoramaColorTolerance : kPanoramaTolerance);
    QCOMPARE(errors.count(), 0);
  }

  // The longitude footprint is corrected at the back seam: trilinear
  // sampling stays on level 0 there instead of the smallest mip level.
  void panoramaSeamKeepsDetail() {
    Scene scene;
    CREATE_SCENE(scene, kPanoramaFrameSize);
    const QImage image = makePanoramaImage();
    // Off 180 degrees: the seam must fall inside a 2 x 2 pixel quad, where
    // the derivatives straddle it, not on a quad boundary.
    const ReferencePanorama back{178.5, 0.0, 90.0};
    scene.item->setImage(shared(image));
    scene.item->setRenderSettings(
        settingsWith(RenderEnums::TextureSampling::Trilinear));
    scene.item->setProjection(RenderEnums::Projection::Equirectangular);
    scene.item->setPanoramaCamera(
        PanoramaCamera{float(back.yaw), float(back.pitch), float(back.fov)});
    RENDER(scene, frame);
    COMPARE_TO_REFERENCE(frame,
                         panoramaFrame(premultipliedSource(image),
                                       kPanoramaFrameSize, back, std::nullopt),
                         kPanoramaTolerance);
  }

  // Each tile draws the rays that hit its core.
  void tiledPanoramaMatchesUntiled() {
    Scene scene;
    CREATE_SCENE(scene, kPanoramaFrameSize);
    QSignalSpy errors(scene.item.get(), &ImageRenderItem::renderError);
    scene.item->setImage(shared(makePanoramaImage()));
    scene.item->setProjection(RenderEnums::Projection::Equirectangular);
    scene.item->setPanoramaCamera(PanoramaCamera{
        float(kPanoramaFront.yaw), float(kPanoramaFront.pitch),
        float(kPanoramaFront.fov)});
    scene.item->setRenderSettings(
        settingsWith(RenderEnums::TextureSampling::Bilinear));
    RENDER(scene, untiled);
    scene.item->setRenderSettings(settingsWith(
        RenderEnums::TextureSampling::Bilinear, false, kSmallTileLimit));
    RENDER(scene, tiled);
    QVERIFY(TileGrid::layout(kPanoramaImageSize, kSmallTileLimit).size() > 1);
    QVERIFY2(maxDifference(tiled, untiled) <= kTileSeamTolerance,
             qPrintable(u"max difference %1"_s.arg(maxDifference(tiled, untiled))));
    QCOMPARE(errors.count(), 0);
  }

  void upscaledCropMatchesReference_data() {
    QTest::addColumn<ReferenceScale>("scale");
    QTest::addColumn<QPoint>("origin");
    // Crop scale = image scale / kCropUpscale.
    QTest::newRow("100% (crop 50%)") << ReferenceScale{1, 1} << kImageOrigin;
    QTest::newRow("200% (crop 1:1)") << ReferenceScale{2, 1} << kImageOrigin;
    QTest::newRow("400% panned (crop 200%)")
        << ReferenceScale{4, 1} << QPoint(-40, -30);
    QTest::newRow("50% (crop 25%)") << ReferenceScale{1, 2} << kImageOrigin;
  }

  void upscaledCropMatchesReference() {
    QFETCH(ReferenceScale, scale);
    QFETCH(QPoint, origin);
    Scene scene;
    CREATE_SCENE(scene, kFrameSize);
    QSignalSpy errors(scene.item.get(), &ImageRenderItem::renderError);
    const QImage image = makeTestImage(ImageKind::Opaque, kTestImageSize);
    const QImage crop = makeCropImage();
    scene.item->setImage(shared(image));
    scene.item->setUpscaledCrop(shared(crop), kCropSourceRect);
    scene.item->setRenderSettings(
        settingsWith(RenderEnums::TextureSampling::Trilinear));
    scene.item->setPlacement(ImagePlacement{
        QPointF(origin), qreal(scale.magnification) / qreal(scale.reduction)});
    RENDER(scene, frame);
    COMPARE_TO_REFERENCE(
        frame, expectedWithCrop(image, crop, kFrameSize, origin, scale),
        kFilteredTolerance);
    QCOMPARE(errors.count(), 0);
  }

  // The crop goes through the same colour management as the image.
  void upscaledCropIsColorManaged() {
    Scene scene;
    CREATE_SCENE(scene, kFrameSize);
    QSignalSpy errors(scene.item.get(), &ImageRenderItem::renderError);
    const QColorSpace p3(QColorSpace::DisplayP3);
    const QImage image = makeTestImage(ImageKind::Opaque, kTestImageSize);
    const QImage crop = makeCropImage();
    scene.item->setColorManagement(managedFor(p3));
    scene.item->setImage(shared(image));
    scene.item->setUpscaledCrop(shared(crop), kCropSourceRect);
    scene.item->setRenderSettings(
        settingsWith(RenderEnums::TextureSampling::Nearest));
    // Crop shown 1:1.
    const ReferenceScale scale{kCropUpscale, 1};
    scene.item->setPlacement(
        ImagePlacement{QPointF(kImageOrigin), qreal(kCropUpscale)});
    QVERIFY(scene.item->cropConversion().isActive());
    RENDER(scene, frame);
    const QImage managedImage = cpuColorManaged(image, p3);
    const QImage managedCrop = cpuColorManaged(crop, p3);
    FloatImage expected = expectedFrame(
        premultipliedSource(managedImage),
        ReferenceScene{kFrameSize, kImageOrigin, scale, ReferenceFilter::Nearest,
                       kBackground});
    const QPoint cropOrigin = kImageOrigin + kCropSourceRect.topLeft() * kCropUpscale;
    const FloatImage cropSource = premultipliedSource(managedCrop);
    for (int y = 0; y < managedCrop.height(); ++y) {
      for (int x = 0; x < managedCrop.width(); ++x)
        expected.at(cropOrigin.x() + x, cropOrigin.y() + y) = cropSource.at(x, y);
    }
    COMPARE_TO_REFERENCE(frame, expected, kColorManagementTolerance);
    QCOMPARE(errors.count(), 0);
  }

  // A new image drops the crop, animation frames keep it, and panorama mode
  // does not draw it.
  void upscaledCropLifetime() {
    Scene scene;
    CREATE_SCENE(scene, kFrameSize);
    QSignalSpy changes(scene.item.get(), &ImageRenderItem::upscaledCropChanged);
    const QImage image = makeTestImage(ImageKind::Opaque, kTestImageSize);
    scene.item->setImage(shared(image));
    scene.item->setRenderSettings(
        settingsWith(RenderEnums::TextureSampling::Trilinear));
    scene.item->setPlacement(ImagePlacement{QPointF(kImageOrigin), 1.0});
    scene.item->setProjection(RenderEnums::Projection::Equirectangular);
    RENDER(scene, panoramaPlain);
    scene.item->setUpscaledCrop(shared(makeCropImage()), kCropSourceRect);
    QVERIFY(scene.item->hasUpscaledCrop());
    RENDER(scene, panoramaWithCrop);
    QCOMPARE(maxDifference(panoramaPlain, panoramaWithCrop), 0);

    scene.item->setImage(shared(image.mirrored(true, false)),
                         ImageRenderItem::ImageUpdate::AnimationFrame);
    QVERIFY(scene.item->hasUpscaledCrop());
    scene.item->setImage(shared(image));
    QVERIFY(!scene.item->hasUpscaledCrop());
    QCOMPARE(changes.count(), 2);
    // An empty source area clears the crop.
    scene.item->setUpscaledCrop(shared(makeCropImage()), QRect());
    QVERIFY(!scene.item->hasUpscaledCrop());
  }

  // ImageViewportController reports renderingSettled() only after a frame
  // that carries its settle pass has ended (QQuickWindow::afterFrameEnd),
  // the replacement of the widget viewer's frameSwapped wait.
  void viewportSettlesAfterThePresentedFrame() {
    Scene scene;
    CREATE_SCENE(scene, kFrameSize);
    UiSettingsSnapshot settings;
    settings.viewer.fitMode = SettingsEnums::FitMode::Window;
    settings.viewer.zoomStep = kViewportZoomStep;
    ImageViewportController controller(settings);
    controller.setView(scene.item.get());
    QSignalSpy settled(&controller, &ImageViewportController::renderingSettled);
    RENDER(scene, empty);

    // A frame rendered before the image was shown does not count.
    controller.showImage(shared(makeTestImage(ImageKind::Opaque, kTestImageSize)),
                         u"viewport.png"_s);
    QVERIFY(scene.item->isSettled());
    QCoreApplication::processEvents();
    QCOMPARE(settled.count(), 0);
    QVERIFY(!controller.isRenderingSettled());

    RENDER(scene, first);
    QCoreApplication::processEvents();
    QCOMPARE(settled.count(), 1);
    QVERIFY(controller.isRenderingSettled());
    QVERIFY(scene.item->isSettled());

    // Every further frame of the same state stays quiet.
    RENDER(scene, second);
    QCoreApplication::processEvents();
    QCOMPARE(settled.count(), 1);

    // A view change unsettles until the next presented frame.
    controller.zoomIn();
    QVERIFY(!controller.isRenderingSettled());
    RENDER(scene, zoomed);
    QCoreApplication::processEvents();
    QCOMPARE(settled.count(), 2);
    QVERIFY(controller.isRenderingSettled());
  }

  // "Copy viewport to clipboard": the read back visible image is the image
  // area of the presented frame.
  void viewportGrabMatchesThePresentedImage() {
    // QQuickItem::grabToImage() needs a visible window. Showing the render
    // control's window creates no on-screen window only on the offscreen
    // platform (the Vulkan variant runs on the windows platform).
    if (QGuiApplication::platformName() != u"offscreen"_s)
      QSKIP("needs the offscreen platform to show the scene's window");
    Scene scene;
    CREATE_SCENE(scene, kFrameSize);
    scene.quick.window()->setVisible(true);
    UiSettingsSnapshot settings;
    settings.viewer.fitMode = SettingsEnums::FitMode::Window;
    settings.viewer.zoomStep = kViewportZoomStep;
    ImageViewportController controller(settings);
    controller.setView(scene.item.get());
    controller.showImage(shared(makeTestImage(ImageKind::Opaque, kTestImageSize)),
                         u"grab.png"_s);
    controller.zoomIn();
    RENDER(scene, shown);

    QSignalSpy grabbed(&controller, &ImageViewportController::visibleImageGrabbed);
    QSignalSpy failed(&controller, &ImageViewportController::visibleImageGrabFailed);
    controller.grabVisibleImage();
    RENDER(scene, withGrab);
    QCoreApplication::processEvents();
    QCOMPARE(failed.count(), 0);
    QCOMPARE(grabbed.count(), 1);

    const QRect imageArea =
        QRectF(scene.item->imagePosition(),
               QSizeF(kTestImageSize) * scene.item->imageScale())
            .toAlignedRect()
            .intersected(QRect(QPoint(0, 0), kFrameSize));
    const QImage copied =
        grabbed.first().first().value<QImage>().convertToFormat(withGrab.format());
    QCOMPARE(copied.size(), imageArea.size());
    const QImage expected = withGrab.copy(imageArea);
    QVERIFY2(maxDifference(copied, expected) <= kGrabTolerance,
             qPrintable(u"max difference %1"_s.arg(maxDifference(copied, expected))));
  }

  //--- ThumbnailItem (thumbnail strip) ---------------------------------------
  // The material cuts the corners with an antialiased edge and keeps the
  // edges between them whole.
  void thumbnailItemRoundsItsCorners() {
    OffscreenQuick quick;
    QVERIFY2(quick.create(kThumbnailFrameSize, rhiBackend), qPrintable(quick.error()));
    quick.window()->setColor(Qt::black);
    const auto item = makeThumbnailItem(quick, 0.0);
    QCoreApplication::processEvents();
    const QImage frame = quick.render();
    QVERIFY2(!frame.isNull(), qPrintable(quick.error()));
    QCOMPARE(item->paintedRect(), QRectF(QPointF(0, 0), QSizeF(kThumbnailFrameSize)));

    const QPoint center(kThumbnailFrameSize.width() / 2, kThumbnailFrameSize.height() / 2);
    QVERIFY(maxChannelDifference(frame.pixel(center), kThumbnailColor) <= kExactTolerance);
    // Middle of an edge: inside the straight part of the outline.
    QVERIFY(maxChannelDifference(frame.pixel(center.x(), 0), kThumbnailColor) <= kExactTolerance);
    QVERIFY(maxChannelDifference(frame.pixel(0, center.y()), kThumbnailColor) <= kExactTolerance);
    // Corner pixels are outside the rounded outline.
    for (const QPoint corner : {QPoint(0, 0), QPoint(kThumbnailFrameSize.width() - 1, 0),
                                QPoint(0, kThumbnailFrameSize.height() - 1),
                                QPoint(kThumbnailFrameSize.width() - 1,
                                       kThumbnailFrameSize.height() - 1)}) {
      QVERIFY2(maxChannelDifference(frame.pixel(corner), qRgb(0, 0, 0)) <= kExactTolerance,
               qPrintable(u"corner %1,%2"_s.arg(corner.x()).arg(corner.y())));
    }
    // The outline is antialiased: a pixel on the arc is partly covered.
    const QRgb onArc = frame.pixel(kThumbnailArcProbe);
    QVERIFY(qGreen(onArc) > 0 && qGreen(onArc) < qGreen(kThumbnailColor));
  }

  // The hover highlight adds that fraction of the thumbnail to itself and
  // saturates.
  void thumbnailItemHighlightBrightens() {
    OffscreenQuick quick;
    QVERIFY2(quick.create(kThumbnailFrameSize, rhiBackend), qPrintable(quick.error()));
    quick.window()->setColor(Qt::black);
    const auto item = makeThumbnailItem(quick, kThumbnailHighlight);
    QCoreApplication::processEvents();
    const QImage frame = quick.render();
    QVERIFY2(!frame.isNull(), qPrintable(quick.error()));
    const auto lit = [](int channel) {
      return qMin(kOpaqueLevel, qRound(channel * (1.0 + kThumbnailHighlight)));
    };
    const QRgb expected = qRgb(lit(qRed(kThumbnailColor)), lit(qGreen(kThumbnailColor)),
                               lit(qBlue(kThumbnailColor)));
    const QPoint center(kThumbnailFrameSize.width() / 2, kThumbnailFrameSize.height() / 2);
    QVERIFY2(maxChannelDifference(frame.pixel(center), expected) <= kExactTolerance,
             qPrintable(u"%1 instead of %2"_s.arg(frame.pixel(center), 0, 16).arg(expected, 0, 16)));
  }
};

QTEST_MAIN(ImageRendererTests)

#include "tst_imagerenderer.moc"
