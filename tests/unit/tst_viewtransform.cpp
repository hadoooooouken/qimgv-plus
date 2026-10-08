#include <QSignalSpy>
#include <QStringList>
#include <QTest>

#include <cmath>

#include "components/viewtransform/viewtransform.h"
#include "components/viewtransform/viewtransformcontroller.h"
#include "testsuites.h"

namespace {
constexpr QSize kViewport(800, 600);
// Tolerance for float scale comparisons.
constexpr qreal kScaleTolerance = 1e-4;
// Positions are whole logical pixels; anchored points may round by one.
constexpr qreal kPixelTolerance = 1.0;
// Relative-position round trips may be off by this many logical pixels.
constexpr qreal kRelativePixelTolerance = 2.0;

// A large image used for zoom and scroll tests: 4000x3000 source pixels.
constexpr QSize kLargeImage(4000, 3000);
// An image that fits into kViewport at 1:1 for every tested DPR.
constexpr QSize kSmallImage(300, 200);

ViewTransform makeTransform(qreal dpr, const ViewTransformConfig &config = {},
                            QSize viewport = kViewport) {
  ViewTransform transform;
  transform.setViewportSize(viewport);
  transform.setDevicePixelRatio(dpr);
  transform.applyConfig(config, QPointF());
  return transform;
}

ViewTransform showing(QSize imageSize, qreal dpr,
                      const ViewTransformConfig &config = {}) {
  ViewTransform transform = makeTransform(dpr, config);
  transform.clear();
  transform.showImage(imageSize, QPointF());
  return transform;
}

// Source pixel shown at `viewportPos`.
QPointF imagePointAt(const ViewTransform &t, QPointF viewportPos) {
  return (viewportPos - t.imagePosition()) * t.devicePixelRatio() / t.scale();
}

// Viewport position of source pixel `imagePoint`.
QPointF viewportPointOf(const ViewTransform &t, QPointF imagePoint) {
  return t.imagePosition() + imagePoint * t.scale() / t.devicePixelRatio();
}

bool isNear(qreal actual, qreal expected, qreal tolerance) {
  return std::abs(actual - expected) <= tolerance;
}

bool isNear(QPointF actual, QPointF expected, qreal tolerance) {
  return isNear(actual.x(), expected.x(), tolerance) &&
         isNear(actual.y(), expected.y(), tolerance);
}

QByteArray describe(qreal actual, qreal expected) {
  return QStringLiteral("actual %1, expected %2").arg(actual).arg(expected).toUtf8();
}

QByteArray describe(QPointF actual, QPointF expected) {
  return QStringLiteral("actual (%1, %2), expected (%3, %4)")
      .arg(actual.x()).arg(actual.y()).arg(expected.x()).arg(expected.y())
      .toUtf8();
}

#define VERIFY_NEAR(actual, expected, tolerance)                                 \
  QVERIFY2(isNear((actual), (expected), (tolerance)),                            \
           describe((actual), (expected)).constData())

void addDprColumn() {
  QTest::addColumn<qreal>("dpr");
  QTest::newRow("dpr 1.0") << 1.0;
  QTest::newRow("dpr 1.5") << 1.5;
  QTest::newRow("dpr 2.0") << 2.0;
}

class FakeViewSurface final : public IViewSurface {
public:
  QSize size = kViewport;
  QPointF pointer;

  QSize viewportSize() const override { return size; }
  QPointF pointerPosition() const override { return pointer; }
};
} // namespace

// Covers the UI-independent view transform model (fit modes, zoom, anchored
// zoom, scrolling, locks, DPR handling, panorama camera) and its controller.
class ViewTransformTests : public QObject {
  Q_OBJECT

private slots:
  void fitWindow_downscalesLargeImage_data() { addDprColumn(); }
  void fitWindow_downscalesLargeImage();
  void fitWindow_keepsSmallImageAt1to1_data() { addDprColumn(); }
  void fitWindow_keepsSmallImageAt1to1();
  void fitWindow_expandsSmallImage_data() { addDprColumn(); }
  void fitWindow_expandsSmallImage();
  void fitWidth_focusPoint_data();
  void fitWidth_focusPoint();
  void fitHeight_focusPoint_data();
  void fitHeight_focusPoint();
  void fitWidth_focusCursorCentresPointerImagePoint_data() { addDprColumn(); }
  void fitWidth_focusCursorCentresPointerImagePoint();
  void fitOriginal_keepsViewportCentre_data() { addDprColumn(); }
  void fitOriginal_keepsViewportCentre();
  void forceFit_ignoresExpandLimit_data() { addDprColumn(); }
  void forceFit_ignoresExpandLimit();

  void minScale_limits_data() { addDprColumn(); }
  void minScale_limits();
  void zoomSteps_freeAndClamped();
  void zoomSteps_fixedLevels_data();
  void zoomSteps_fixedLevels();
  void parseZoomLevels_sortsValues();

  void anchoredZoom_keepsImagePointUnderAnchor_data();
  void anchoredZoom_keepsImagePointUnderAnchor();
  void zoomTo_detectsFitModeWithinEpsilon_data() { addDprColumn(); }
  void zoomTo_detectsFitModeWithinEpsilon();
  void gestureZoom_scalesWithDpr_data() { addDprColumn(); }
  void gestureZoom_scalesWithDpr();

  void scroll_snapsLargeImageToEdges_data() { addDprColumn(); }
  void scroll_snapsLargeImageToEdges();
  void scroll_keepsSmallImageCentred_data() { addDprColumn(); }
  void scroll_keepsSmallImageCentred();
  void scrollTo_setsScrollPosition_data() { addDprColumn(); }
  void scrollTo_setsScrollPosition();

  void lockZoom_keepsScaleAcrossImages_data() { addDprColumn(); }
  void lockZoom_keepsScaleAcrossImages();
  void lockZoom_extendsMinimumScale_data() { addDprColumn(); }
  void lockZoom_extendsMinimumScale();
  void lockView_restoresRelativePosition_data() { addDprColumn(); }
  void lockView_restoresRelativePosition();
  void transformedImage_keepsScaleAndPosition_data() { addDprColumn(); }
  void transformedImage_keepsScaleAndPosition();
  void keepFitMode_data();
  void keepFitMode();

  void refitToViewport_fitAndFreeModes_data() { addDprColumn(); }
  void refitToViewport_fitAndFreeModes();
  void applyConfig_refitsOnlyForFitSettings_data() { addDprColumn(); }
  void applyConfig_refitsOnlyForFitSettings();
  void applyConfig_raisesFreeScaleToNewMinimum_data() { addDprColumn(); }
  void applyConfig_raisesFreeScaleToNewMinimum();
  void devicePixelRatioChange_keepsLogicalFit();

  void panorama_clampsAndResets();

  void controller_zoomNotifiesInOrder();
  void controller_scrollNotifiesPositionOnly();
  void controller_fitWindowNotifiesCentred();
  void controller_samplesViewportAndPointer();
  void controller_panoramaNotifies();
};

// --- fit modes ----------------------------------------------------------------

void ViewTransformTests::fitWindow_downscalesLargeImage() {
  QFETCH(qreal, dpr);
  const ViewTransform t = showing(QSize(4000, 2000), dpr);
  QCOMPARE(t.fitMode(), FIT_WINDOW);
  VERIFY_NEAR(t.scale(), 0.2 * dpr, kScaleTolerance);
  QCOMPARE(t.scaledSize(), QSize(800, 400));
  // Float scales may truncate the centring offset by one pixel.
  VERIFY_NEAR(t.imagePosition(), QPointF(0, 100), kPixelTolerance);
  QVERIFY(t.scaledImageFits());
  QVERIFY(!t.imageFits());
}

void ViewTransformTests::fitWindow_keepsSmallImageAt1to1() {
  QFETCH(qreal, dpr);
  const ViewTransform t = showing(kSmallImage, dpr);
  QCOMPARE(t.fitMode(), FIT_WINDOW);
  QCOMPARE(t.scale(), 1.0f);
  QVERIFY(t.imageFits());
  const QPointF centred(static_cast<int>(kViewport.width() - kSmallImage.width() / dpr) / 2,
                        static_cast<int>(kViewport.height() - kSmallImage.height() / dpr) / 2);
  QCOMPARE(t.imagePosition(), centred);
}

void ViewTransformTests::fitWindow_expandsSmallImage() {
  QFETCH(qreal, dpr);
  const qreal unlimitedScale = kViewport.width() * dpr / kSmallImage.width();

  ViewTransformConfig expand;
  expand.expandImage = true;
  const ViewTransform unlimited = showing(kSmallImage, dpr, expand);
  VERIFY_NEAR(unlimited.scale(), unlimitedScale, kScaleTolerance);
  QCOMPARE(unlimited.scaledSize().width(), kViewport.width());

  constexpr float kExpandLimit = 2.0f;
  expand.expandLimit = kExpandLimit;
  const ViewTransform limited = showing(kSmallImage, dpr, expand);
  QCOMPARE(limited.scale(), kExpandLimit);

  // The temporary override expands without the setting's limit.
  ViewTransformConfig noExpand;
  noExpand.expandLimit = kExpandLimit;
  ViewTransform temporary = makeTransform(dpr, noExpand);
  temporary.setExpandSmallImagesInFitMode(true);
  temporary.clear();
  temporary.showImage(kSmallImage, QPointF());
  VERIFY_NEAR(temporary.scale(), unlimitedScale, kScaleTolerance);
}

void ViewTransformTests::fitWidth_focusPoint_data() {
  QTest::addColumn<qreal>("dpr");
  QTest::addColumn<ImageFocusPoint>("focus");
  QTest::addColumn<QPointF>("expectedPos");
  for (qreal dpr : {1.0, 1.5, 2.0}) {
    // 1000x4000 source pixels fill the width: 800x3200 logical.
    QTest::addRow("top, dpr %.1f", dpr) << dpr << FOCUS_TOP << QPointF(0, 0);
    QTest::addRow("center, dpr %.1f", dpr) << dpr << FOCUS_CENTER << QPointF(0, -1300);
  }
}

void ViewTransformTests::fitWidth_focusPoint() {
  QFETCH(qreal, dpr);
  QFETCH(ImageFocusPoint, focus);
  QFETCH(QPointF, expectedPos);
  ViewTransformConfig config;
  config.defaultFitMode = FIT_WIDTH;
  config.focusPoint = focus;
  const ViewTransform t = showing(QSize(1000, 4000), dpr, config);
  QCOMPARE(t.fitMode(), FIT_WIDTH);
  VERIFY_NEAR(t.scale(), 0.8 * dpr, kScaleTolerance);
  QCOMPARE(t.scaledSize(), QSize(800, 3200));
  VERIFY_NEAR(t.imagePosition(), expectedPos, kPixelTolerance);
}

void ViewTransformTests::fitHeight_focusPoint_data() {
  QTest::addColumn<qreal>("dpr");
  QTest::addColumn<ImageFocusPoint>("focus");
  QTest::addColumn<QPointF>("expectedPos");
  for (qreal dpr : {1.0, 1.5, 2.0}) {
    // 4000x1000 source pixels fill the height: 2400x600 logical.
    QTest::addRow("top, dpr %.1f", dpr) << dpr << FOCUS_TOP << QPointF(0, 0);
    QTest::addRow("center, dpr %.1f", dpr) << dpr << FOCUS_CENTER << QPointF(-800, 0);
  }
}

void ViewTransformTests::fitHeight_focusPoint() {
  QFETCH(qreal, dpr);
  QFETCH(ImageFocusPoint, focus);
  QFETCH(QPointF, expectedPos);
  ViewTransformConfig config;
  config.defaultFitMode = FIT_HEIGHT;
  config.focusPoint = focus;
  const ViewTransform t = showing(QSize(4000, 1000), dpr, config);
  QCOMPARE(t.fitMode(), FIT_HEIGHT);
  VERIFY_NEAR(t.scale(), 0.6 * dpr, kScaleTolerance);
  QCOMPARE(t.scaledSize(), QSize(2400, 600));
  VERIFY_NEAR(t.imagePosition(), expectedPos, kPixelTolerance);
}

void ViewTransformTests::fitWidth_focusCursorCentresPointerImagePoint() {
  QFETCH(qreal, dpr);
  ViewTransformConfig config;
  config.focusPoint = FOCUS_CURSOR;
  ViewTransform t = showing(QSize(1000, 4000), dpr, config);
  const QPointF pointer(400, 200);
  const QPointF imagePoint = imagePointAt(t, pointer);

  t.forceFitMode(FIT_WIDTH, pointer);
  QCOMPARE(t.fitMode(), FIT_WIDTH);
  QCOMPARE(t.scaledSize().width(), kViewport.width());
  // The image point that was under the pointer is now at the viewport
  // centre along the overflowing axis.
  VERIFY_NEAR(viewportPointOf(t, imagePoint).y(), kViewport.height() / 2.0, kPixelTolerance);
}

void ViewTransformTests::fitOriginal_keepsViewportCentre() {
  QFETCH(qreal, dpr);
  ViewTransformConfig config;
  config.focusPoint = FOCUS_CENTER;
  ViewTransform t = showing(kLargeImage, dpr, config);
  const QPointF center = t.viewportCenter();
  const QPointF imagePoint = imagePointAt(t, center);

  t.setFitMode(FIT_ORIGINAL, QPointF());
  QCOMPARE(t.scale(), 1.0f);
  VERIFY_NEAR(viewportPointOf(t, imagePoint), center, kPixelTolerance);
}

void ViewTransformTests::forceFit_ignoresExpandLimit() {
  QFETCH(qreal, dpr);
  constexpr float kExpandLimit = 2.0f;
  ViewTransformConfig config;
  config.expandImage = true;
  config.expandLimit = kExpandLimit;
  ViewTransform t = showing(kSmallImage, dpr, config);

  t.setFitMode(FIT_WIDTH, QPointF());
  QCOMPARE(t.scale(), kExpandLimit);
  t.forceFitMode(FIT_WIDTH, QPointF());
  VERIFY_NEAR(t.scale(), kViewport.width() * dpr / kSmallImage.width(), kScaleTolerance);
  QCOMPARE(t.scaledSize().width(), kViewport.width());
}

// --- zoom ---------------------------------------------------------------------------

void ViewTransformTests::minScale_limits() {
  QFETCH(qreal, dpr);
  const ViewTransform large = showing(kLargeImage, dpr);
  QCOMPARE(large.minScale(), large.fitWindowScale());
  QCOMPARE(large.zoomOutScale(large.scale()), large.fitWindowScale());

  const ViewTransform small = showing(kSmallImage, dpr);
  QCOMPARE(small.minScale(), 1.0f);

  ViewTransformConfig unlocked;
  unlocked.unlockMinZoom = true;
  const ViewTransform unlockedView = showing(kLargeImage, dpr, unlocked);
  // The shorter side may shrink to 10 device pixels.
  VERIFY_NEAR(unlockedView.minScale(), 10.0 / kLargeImage.height(), kScaleTolerance);
}

void ViewTransformTests::zoomSteps_freeAndClamped() {
  ViewTransformConfig config;
  config.unlockMinZoom = true;
  const ViewTransform t = showing(kLargeImage, 1.0, config);
  VERIFY_NEAR(t.zoomInScale(1.0f), 1.1, kScaleTolerance);
  VERIFY_NEAR(t.zoomOutScale(1.1f), 1.0, kScaleTolerance);
  QCOMPARE(t.zoomInScale(39.0f), ViewTransform::kMaxScale);
}

void ViewTransformTests::zoomSteps_fixedLevels_data() {
  QTest::addColumn<bool>("zoomIn");
  QTest::addColumn<float>("base");
  QTest::addColumn<float>("expected");
  // Levels: 0.5, 1, 2; step 0.1.
  QTest::newRow("in below first level") << true << 0.3f << 0.33f;
  QTest::newRow("in to next level") << true << 0.7f << 1.0f;
  QTest::newRow("in from a level") << true << 1.0f << 2.0f;
  QTest::newRow("in above last level") << true << 2.0f << 2.2f;
  QTest::newRow("out above last level") << false << 3.0f << 3.0f / 1.1f;
  QTest::newRow("out to previous level") << false << 1.5f << 1.0f;
  QTest::newRow("out from a level") << false << 1.0f << 0.5f;
  QTest::newRow("out from last level") << false << 2.0f << 1.0f;
  QTest::newRow("out below first level") << false << 0.5f << 0.5f / 1.1f;
}

void ViewTransformTests::zoomSteps_fixedLevels() {
  QFETCH(bool, zoomIn);
  QFETCH(float, base);
  QFETCH(float, expected);
  ViewTransformConfig config;
  config.unlockMinZoom = true;
  config.useFixedZoomLevels = true;
  config.zoomLevels = parseZoomLevels(QStringLiteral("0.5,1,2"));
  const ViewTransform t = showing(kLargeImage, 1.0, config);
  const float actual = zoomIn ? t.zoomInScale(base) : t.zoomOutScale(base);
  VERIFY_NEAR(actual, expected, kScaleTolerance);
}

void ViewTransformTests::parseZoomLevels_sortsValues() {
  QCOMPARE(parseZoomLevels(QStringLiteral("2,0.5,1")), QList<float>({0.5f, 1.0f, 2.0f}));
}

void ViewTransformTests::anchoredZoom_keepsImagePointUnderAnchor_data() {
  QTest::addColumn<qreal>("dpr");
  QTest::addColumn<QPointF>("anchor");
  for (qreal dpr : {1.0, 1.5, 2.0}) {
    QTest::addRow("centre, dpr %.1f", dpr) << dpr << QPointF(400, 300);
    QTest::addRow("bottom left, dpr %.1f", dpr) << dpr << QPointF(100, 500);
    QTest::addRow("top right, dpr %.1f", dpr) << dpr << QPointF(700, 50);
  }
}

void ViewTransformTests::anchoredZoom_keepsImagePointUnderAnchor() {
  QFETCH(qreal, dpr);
  QFETCH(QPointF, anchor);
  // 8000x6000 source pixels fit the viewport exactly at fit-window scale.
  ViewTransform t = showing(QSize(8000, 6000), dpr);
  const float fitScale = t.scale();
  t.setZoomAnchor(anchor);
  const QPointF imagePoint = imagePointAt(t, anchor);

  constexpr float kZoomFactors[] = {4.0f, 2.0f, 3.0f};
  for (float factor : kZoomFactors) {
    t.zoomTo(fitScale * factor);
    VERIFY_NEAR(t.scale(), fitScale * factor, kScaleTolerance);
    QCOMPARE(t.fitMode(), FIT_FREE);
    VERIFY_NEAR(viewportPointOf(t, imagePoint), anchor, kPixelTolerance);
  }
}

void ViewTransformTests::zoomTo_detectsFitModeWithinEpsilon() {
  QFETCH(qreal, dpr);
  ViewTransform t = showing(kLargeImage, dpr);
  t.setZoomAnchor(t.viewportCenter());
  t.zoomTo(1.0f);
  QCOMPARE(t.fitMode(), FIT_FREE);
  // Smooth zoom and the zoom gesture rarely land exactly on a fit scale.
  t.zoomTo(t.fitWindowScale() + ViewTransform::kScaleEpsilon / 2);
  QCOMPARE(t.fitMode(), FIT_WINDOW);
}

void ViewTransformTests::gestureZoom_scalesWithDpr() {
  QFETCH(qreal, dpr);
  ViewTransform t = showing(kLargeImage, dpr);
  constexpr int kMoveDistance = 10;
  // 0.3% per device pixel moved.
  VERIFY_NEAR(t.gestureZoomScale(kMoveDistance),
              t.scale() * (1.0 + 0.003 * kMoveDistance * dpr), kScaleTolerance);
}

// --- scrolling ------------------------------------------------------------------------

void ViewTransformTests::scroll_snapsLargeImageToEdges() {
  QFETCH(qreal, dpr);
  constexpr qreal kFarAway = 100000;
  ViewTransform t = showing(kLargeImage, dpr);
  t.setZoomAnchor(t.viewportCenter());
  t.zoomTo(1.0f);

  t.scrollBy(QPointF(-kFarAway, -kFarAway));
  QCOMPARE(t.imagePosition(), QPointF(0, 0));

  t.scrollBy(QPointF(kFarAway, kFarAway));
  QCOMPARE(t.scaledRect().right(), kViewport.width());
  QCOMPARE(t.scaledRect().bottom(), kViewport.height());
}

void ViewTransformTests::scroll_keepsSmallImageCentred() {
  QFETCH(qreal, dpr);
  ViewTransform t = showing(kSmallImage, dpr);
  const QPointF centred = t.imagePosition();
  t.scrollBy(QPointF(50, -50));
  QCOMPARE(t.imagePosition(), centred);
}

void ViewTransformTests::scrollTo_setsScrollPosition() {
  QFETCH(qreal, dpr);
  constexpr int kScrollX = 123;
  ViewTransform t = showing(kLargeImage, dpr);
  t.setZoomAnchor(t.viewportCenter());
  t.zoomTo(1.0f);
  t.scrollTo(Qt::Horizontal, kScrollX);
  QCOMPARE(t.scrollPosition().x(), kScrollX);
  QCOMPARE(t.imagePosition().x(), qreal(-kScrollX));
}

// --- locks ------------------------------------------------------------------------------

void ViewTransformTests::lockZoom_keepsScaleAcrossImages() {
  QFETCH(qreal, dpr);
  ViewTransform t = showing(kLargeImage, dpr);
  t.setZoomAnchor(t.viewportCenter());
  t.zoomTo(1.0f);
  t.toggleLockZoom();
  QCOMPARE(t.lock(), ViewLock::Zoom);
  QCOMPARE(t.fitMode(), FIT_FREE);

  t.clear();
  t.showImage(QSize(6000, 2000), QPointF());
  QCOMPARE(t.scale(), 1.0f);
  QCOMPARE(t.fitMode(), FIT_FREE);

  t.toggleLockZoom();
  QCOMPARE(t.lock(), ViewLock::None);
  t.clear();
  t.showImage(QSize(6000, 2000), QPointF());
  QCOMPARE(t.fitMode(), FIT_WINDOW);
  QCOMPARE(t.scale(), t.fitWindowScale());
}

void ViewTransformTests::lockZoom_extendsMinimumScale() {
  QFETCH(qreal, dpr);
  ViewTransform t = showing(QSize(8000, 6000), dpr);
  const float lockedScale = t.scale();
  t.toggleLockZoom();

  // A small image normally cannot go below 1:1; the lock overrides that.
  t.clear();
  t.showImage(QSize(600, 400), QPointF());
  QCOMPARE(t.minScale(), lockedScale);
  QCOMPARE(t.scale(), lockedScale);
}

void ViewTransformTests::lockView_restoresRelativePosition() {
  QFETCH(qreal, dpr);
  ViewTransform t = showing(kLargeImage, dpr);
  t.setZoomAnchor(t.viewportCenter());
  t.zoomTo(1.0f);
  t.scrollBy(QPointF(300, 200));
  t.toggleLockView();
  QCOMPARE(t.lock(), ViewLock::All);
  const QPointF saved = t.relativeViewportCenter();

  t.clear();
  t.showImage(kLargeImage, QPointF());
  QCOMPARE(t.scale(), 1.0f);
  const qreal tolerance = kRelativePixelTolerance / t.scaledSizeF().width();
  VERIFY_NEAR(t.relativeViewportCenter(), saved, tolerance);
}

void ViewTransformTests::transformedImage_keepsScaleAndPosition() {
  QFETCH(qreal, dpr);
  ViewTransform t = showing(kLargeImage, dpr);
  t.setZoomAnchor(t.viewportCenter());
  t.zoomTo(1.0f);
  t.scrollBy(QPointF(300, 200));
  const PreservedView preserved = t.preservedView();

  t.clear();
  t.showTransformedImage(kLargeImage.transposed(), preserved, QPointF());
  QCOMPARE(t.scale(), 1.0f);
  QCOMPARE(t.fitMode(), FIT_FREE);
  const qreal tolerance = kRelativePixelTolerance / t.scaledSizeF().width();
  VERIFY_NEAR(t.relativeViewportCenter(), preserved.relativeCenter, tolerance);
}

void ViewTransformTests::keepFitMode_data() {
  QTest::addColumn<bool>("keep");
  QTest::addColumn<ImageFitMode>("expected");
  QTest::newRow("kept") << true << FIT_WIDTH;
  QTest::newRow("reset to default") << false << FIT_WINDOW;
}

void ViewTransformTests::keepFitMode() {
  QFETCH(bool, keep);
  QFETCH(ImageFitMode, expected);
  ViewTransformConfig config;
  config.keepFitMode = keep;
  ViewTransform t = showing(kLargeImage, 1.0, config);
  t.setFitMode(FIT_WIDTH, QPointF());
  t.clear();
  t.showImage(kLargeImage, QPointF());
  QCOMPARE(t.fitMode(), expected);
}

// --- environment ------------------------------------------------------------------------

void ViewTransformTests::refitToViewport_fitAndFreeModes() {
  QFETCH(qreal, dpr);
  const QSize halfViewport = kViewport / 2;

  ViewTransform fitted = showing(QSize(4000, 2000), dpr);
  fitted.setViewportSize(halfViewport);
  fitted.refitToViewport(QPointF());
  VERIFY_NEAR(fitted.scale(), 0.1 * dpr, kScaleTolerance);
  QCOMPARE(fitted.scaledSize(), QSize(400, 200));

  ViewTransform freeView = showing(kLargeImage, dpr);
  freeView.setZoomAnchor(freeView.viewportCenter());
  freeView.zoomTo(1.0f);
  freeView.setViewportSize(halfViewport);
  freeView.refitToViewport(QPointF());
  QCOMPARE(freeView.scale(), 1.0f);
  QVERIFY(freeView.scaledRect().left() <= 0);
  QVERIFY(freeView.scaledRect().right() >= halfViewport.width());
}

void ViewTransformTests::applyConfig_refitsOnlyForFitSettings() {
  QFETCH(qreal, dpr);
  ViewTransformConfig config;
  ViewTransform t = showing(kLargeImage, dpr, config);

  config.defaultFitMode = FIT_WIDTH;
  QVERIFY(t.applyConfig(config, QPointF()));
  QCOMPARE(t.fitMode(), FIT_WIDTH);
  QCOMPARE(t.scale(), t.fitWidthScale());

  constexpr float kOtherZoomStep = 0.2f;
  config.zoomStep = kOtherZoomStep;
  QVERIFY(!t.applyConfig(config, QPointF()));
  QCOMPARE(t.fitMode(), FIT_WIDTH);
}

void ViewTransformTests::applyConfig_raisesFreeScaleToNewMinimum() {
  QFETCH(qreal, dpr);
  ViewTransformConfig config;
  config.unlockMinZoom = true;
  ViewTransform t = showing(kLargeImage, dpr, config);
  t.setZoomAnchor(t.viewportCenter());
  t.zoomTo(t.fitWindowScale() / 2);
  QCOMPARE(t.fitMode(), FIT_FREE);

  config.unlockMinZoom = false;
  QVERIFY(!t.applyConfig(config, QPointF()));
  QCOMPARE(t.scale(), t.fitWindowScale());
}

void ViewTransformTests::devicePixelRatioChange_keepsLogicalFit() {
  constexpr qreal kNewDpr = 2.0;
  ViewTransform t = showing(QSize(4000, 2000), 1.0);
  VERIFY_NEAR(t.scale(), 0.2, kScaleTolerance);
  t.changeDevicePixelRatio(kNewDpr, QPointF());
  VERIFY_NEAR(t.scale(), 0.4, kScaleTolerance);
  QCOMPARE(t.scaledSize(), QSize(800, 400));
}

void ViewTransformTests::panorama_clampsAndResets() {
  constexpr int kViewportWidth = 800;
  constexpr int kFarDrag = 100000;
  constexpr int kWheelNotch = 120;
  constexpr int kWheelNotches = 100;
  PanoramaView view;

  // 80 px across an 800 px wide view at 90° FOV turns by 18°.
  view.drag(QPoint(80, 0), kViewportWidth);
  VERIFY_NEAR(view.yaw(), -18.0, kScaleTolerance);
  view.drag(QPoint(0, -kFarDrag), kViewportWidth);
  QCOMPARE(view.pitch(), PanoramaView::kMaxPitch);
  view.drag(QPoint(0, kFarDrag), kViewportWidth);
  QCOMPARE(view.pitch(), -PanoramaView::kMaxPitch);
  view.drag(QPoint(kFarDrag, 0), 0);
  VERIFY_NEAR(view.yaw(), -18.0, kScaleTolerance);

  view.zoomByWheel(kWheelNotch);
  VERIFY_NEAR(view.fov(), 81.0, kScaleTolerance);
  for (int i = 0; i < kWheelNotches; ++i)
    view.zoomByWheel(kWheelNotch);
  QCOMPARE(view.fov(), PanoramaView::kMinFov);
  for (int i = 0; i < kWheelNotches; ++i)
    view.zoomByWheel(-kWheelNotch);
  QCOMPARE(view.fov(), PanoramaView::kMaxFov);

  view.reset();
  QCOMPARE(view.yaw(), 0.0f);
  QCOMPARE(view.pitch(), 0.0f);
  QCOMPARE(view.fov(), PanoramaView::kDefaultFov);
  constexpr int kGestureDistance = 10;
  view.zoomByGesture(kGestureDistance, 1.0);
  VERIFY_NEAR(view.fov(), 87.3, kScaleTolerance);
}

// --- controller ---------------------------------------------------------------------------

void ViewTransformTests::controller_zoomNotifiesInOrder() {
  FakeViewSurface surface;
  ViewTransformController controller(surface);
  controller.showImage(kLargeImage);

  QStringList order;
  connect(&controller, &ViewTransformController::transformChanged, this,
          [&order]() { order << QStringLiteral("transform"); });
  connect(&controller, &ViewTransformController::scaleChanged, this,
          [&order]() { order << QStringLiteral("scale"); });
  connect(&controller, &ViewTransformController::positionChanged, this,
          [&order]() { order << QStringLiteral("position"); });
  connect(&controller, &ViewTransformController::anchoredZoomApplied, this,
          [&order]() { order << QStringLiteral("anchored"); });
  QSignalSpy scaleSpy(&controller, &ViewTransformController::scaleChanged);

  constexpr float kTargetScale = 0.4f;
  controller.setZoomAnchor(QPointF(400, 300));
  QVERIFY(order.isEmpty());
  controller.zoomTo(kTargetScale);
  QCOMPARE(order, QStringList({QStringLiteral("transform"), QStringLiteral("scale"),
                               QStringLiteral("position"), QStringLiteral("anchored")}));
  QCOMPARE(scaleSpy.count(), 1);
  VERIFY_NEAR(scaleSpy.at(0).at(0).toReal(), kTargetScale, kScaleTolerance);
}

void ViewTransformTests::controller_scrollNotifiesPositionOnly() {
  FakeViewSurface surface;
  ViewTransformController controller(surface);
  controller.showImage(kLargeImage);
  controller.setZoomAnchor(QPointF(400, 300));
  controller.zoomTo(1.0f);

  QSignalSpy scaleSpy(&controller, &ViewTransformController::scaleChanged);
  QSignalSpy positionSpy(&controller, &ViewTransformController::positionChanged);
  QSignalSpy anchoredSpy(&controller, &ViewTransformController::anchoredZoomApplied);
  controller.scrollBy(QPointF(10, 0));
  QCOMPARE(positionSpy.count(), 1);
  QCOMPARE(scaleSpy.count(), 0);
  QCOMPARE(anchoredSpy.count(), 0);

  // Nothing moves: nothing is announced.
  controller.saveViewportPosition();
  QCOMPARE(positionSpy.count(), 1);
}

void ViewTransformTests::controller_fitWindowNotifiesCentred() {
  FakeViewSurface surface;
  ViewTransformController controller(surface);
  controller.showImage(kLargeImage);
  controller.setZoomAnchor(QPointF(400, 300));
  controller.zoomTo(1.0f);

  QSignalSpy centredSpy(&controller, &ViewTransformController::imageCentered);
  controller.forceFitMode(FIT_WINDOW);
  QCOMPARE(centredSpy.count(), 1);
  QCOMPARE(controller.transform().fitMode(), FIT_WINDOW);
}

void ViewTransformTests::controller_samplesViewportAndPointer() {
  FakeViewSurface surface;
  ViewTransformController controller(surface);
  ViewTransformConfig config;
  config.focusPoint = FOCUS_CURSOR;
  controller.applyConfig(config);
  controller.showImage(kLargeImage);
  VERIFY_NEAR(controller.transform().scale(), 0.2, kScaleTolerance);

  // FIT_ORIGINAL with cursor focus keeps the image point under the pointer.
  surface.pointer = QPointF(100, 100);
  const QPointF imagePoint = imagePointAt(controller.transform(), surface.pointer);
  controller.setFitMode(FIT_ORIGINAL);
  QCOMPARE(controller.transform().scale(), 1.0f);
  VERIFY_NEAR(viewportPointOf(controller.transform(), imagePoint), surface.pointer,
              kPixelTolerance);

  // The viewport size is read from the surface on every operation.
  surface.size = kViewport / 2;
  controller.refitToViewport();
  controller.setFitMode(FIT_WINDOW);
  QCOMPARE(controller.transform().viewportSize(), kViewport / 2);
  VERIFY_NEAR(controller.transform().scale(), 0.1, kScaleTolerance);
}

void ViewTransformTests::controller_panoramaNotifies() {
  FakeViewSurface surface;
  ViewTransformController controller(surface);
  QSignalSpy panoramaSpy(&controller, &ViewTransformController::panoramaChanged);

  controller.dragPanorama(QPoint(80, 0));
  VERIFY_NEAR(controller.panorama().yaw(), -18.0, kScaleTolerance);
  controller.zoomPanoramaByWheel(1);
  controller.resetPanorama();
  QCOMPARE(panoramaSpy.count(), 3);
  QCOMPARE(controller.panorama().yaw(), 0.0f);
}

int runViewTransformTests(int argc, char **argv) {
  ViewTransformTests tests;
  return QTest::qExec(&tests, argc, argv);
}

#include "tst_viewtransform.moc"
