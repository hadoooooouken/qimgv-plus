#include <QCoreApplication>
#include <QElapsedTimer>
#include <QQuickWindow>
#include <QRandomGenerator>
#include <QSGRendererInterface>
#include <QSignalSpy>
#include <QTest>
#include <algorithm>
#include <memory>

#include "gui/quick/render/imagerenderitem.h"
#include "gui/quick/render/resamplegrid.h"
#include "gui/quick/render/textureuploadformat.h"
#include "gui/quick/render/tilegrid.h"
#include "offscreenquick.h"
#include "referenceimages.h"

namespace {
using namespace Qt::StringLiterals;

constexpr QSize kTestImageSize(64, 48);
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
Q_DECLARE_METATYPE(ReferenceScale)
Q_DECLARE_METATYPE(ImageFilter)

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
    const TextureFormatSupport all{true, true};
    const TextureFormatSupport none{false, false};
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
};

QTEST_MAIN(ImageRendererTests)

#include "tst_imagerenderer.moc"
