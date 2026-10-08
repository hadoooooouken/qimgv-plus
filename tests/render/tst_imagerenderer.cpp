#include <QCoreApplication>
#include <QQuickWindow>
#include <QRandomGenerator>
#include <QSGRendererInterface>
#include <QSignalSpy>
#include <QTest>
#include <memory>

#include "gui/quick/render/imagerenderitem.h"
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

// Largest image of the plan's acceptance criteria, shown at 1 / 32 so that
// it is drawn from mip level 5 (within TileGrid::kExactMipLevels).
constexpr QSize kHugeImageSize(32000, 8000);
constexpr int kHugeReduction = 32;
// Smooth pattern with features smaller than one mip block.
constexpr int kHugePatternPeriodX = 97;
constexpr int kHugePatternPeriodY = 89;
constexpr int kHugePatternStepX = 41;
constexpr int kHugePatternStepY = 67;
constexpr int kHugePatternDiagonal = 13;
constexpr int kByteMask = 0xFF;

constexpr QSize kTiledImageSize(400, 300);
// Small enough to split kTiledImageSize into many tiles.
constexpr int kSmallTileLimit = TileGrid::kMinimumTiledTextureSize;

constexpr int kCheckerTilePx = 16;
constexpr int kCheckerCellPx = kCheckerTilePx / 2;
constexpr int kCheckerLightLevel = 0x99;
constexpr int kCheckerDarkLevel = 0x66;
constexpr int kOpaqueLevel = 255;
constexpr int kChannels = 4;

enum class ImageKind { Opaque, Alpha, Deep };

QImage makeTestImage(ImageKind kind, QSize size) {
  const QImage::Format format = kind == ImageKind::Opaque ? QImage::Format_RGB32
                                : kind == ImageKind::Alpha ? QImage::Format_ARGB32
                                                           : QImage::Format_RGBA64;
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

// One offscreen scene with an ImageRenderItem filling it. The item is
// declared after the scene so that it is destroyed first.
struct Scene {
  OffscreenQuick quick;
  std::unique_ptr<ImageRenderItem> item;

  [[nodiscard]] bool create(QSize frameSize) {
    if (!quick.create(frameSize))
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

Q_DECLARE_METATYPE(ImageKind)
Q_DECLARE_METATYPE(ReferenceScale)

class ImageRendererTests : public QObject {
  Q_OBJECT

private slots:
  void initTestCase() {
    // The offscreen QPA platform makes Qt Quick fall back to the software
    // adaptation; QQuickRhiItem needs the default (QRhi) one.
    QQuickWindow::setSceneGraphBackend(u"rhi"_s);
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Direct3D11);
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
    QString where;
    const int difference = maxLevelDifference(frame, expected, &where);
    QVERIFY2(difference <= tolerance,
             qPrintable(u"max difference %1 at %2"_s.arg(difference).arg(where)));
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
    QVERIFY2(scene.quick.rhi()->resourceLimit(QRhi::TextureSizeMax) <
                 kHugeImageSize.width(),
             "the GPU texture limit does not require tiling");
    QSignalSpy errors(scene.item.get(), &ImageRenderItem::renderError);
    const QImage image = makeHugeImage();
    QVERIFY(!image.isNull());
    scene.item->setImage(shared(image));
    scene.item->setRenderSettings(
        settingsWith(RenderEnums::TextureSampling::Trilinear));
    scene.item->setPlacement(
        ImagePlacement{QPointF(0.0, 0.0), 1.0 / kHugeReduction});
    RENDER(scene, frame);

    const FloatImage expected =
        expectedFrame(boxReduceGrayscale8(image, kHugeReduction),
                      ReferenceScene{frameSize, QPoint(0, 0), {}, {},
                                     kBackground});
    QString where;
    const int difference = maxLevelDifference(frame, expected, &where);
    QVERIFY2(difference <= kDeepMipTolerance,
             qPrintable(u"max difference %1 at %2"_s.arg(difference).arg(where)));
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
    QVERIFY2(second.create(kFrameSize), qPrintable(second.error()));

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
};

QTEST_MAIN(ImageRendererTests)

#include "tst_imagerenderer.moc"
