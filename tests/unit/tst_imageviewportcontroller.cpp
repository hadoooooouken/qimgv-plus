#include <QFile>
#include <QImage>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

#include "gui/quick/render/imagerenderitem.h"
#include "gui/quick/ui/imageviewportcontroller.h"
#include "testanimations.h"
#include "testsuites.h"

namespace {
using namespace Qt::StringLiterals;

constexpr QSizeF kViewport(800, 600);
constexpr QSize kLargeImage(1600, 1200);
constexpr QSize kSmallImage(200, 150);
constexpr QSize kPanoramaImage(2000, 1000);
constexpr double kZoomStep = 0.25;
constexpr qreal kScaleTolerance = 1e-4;
constexpr QPointF kCentre(400, 300);
// Inside the left / right click zone of kViewport.
constexpr QPointF kLeftEdge(10, 300);
constexpr QPointF kRightEdge(790, 300);
constexpr int kLeft = Qt::LeftButton;
constexpr int kRight = Qt::RightButton;
constexpr int kNoModifiers = Qt::NoModifier;
const QString kFirstFile = u"C:/images/first.png"_s;
const QString kSecondFile = u"C:/images/second.png"_s;

UiSettingsSnapshot testSettings() {
  UiSettingsSnapshot settings;
  settings.viewer.fitMode = SettingsEnums::FitMode::Window;
  settings.viewer.zoomStep = kZoomStep;
  settings.viewer.scalingFilter = SettingsEnums::ScalingFilter::Mks2021Gpu;
  settings.viewer.casSharpening = 0.5;
  settings.viewer.casContrast = 0.25;
  settings.viewer.imageScrolling = SettingsEnums::ImageScrolling::ByTrackpadAndWheel;
  settings.viewer.mouseScrollingSpeed = 1.0;
  settings.viewer.trackpadDetection = true;
  settings.viewer.clickableEdgesVisible = true;
  settings.panel.enabled = true;
  settings.panel.position = SettingsEnums::PanelPosition::Bottom;
  settings.overlays.zoomIndicatorMode = SettingsEnums::ZoomIndicatorMode::Enabled;
  return settings;
}

std::shared_ptr<const QImage> testImage(QSize size) {
  QImage image(size, QImage::Format_ARGB32_Premultiplied);
  image.fill(Qt::darkCyan);
  return std::make_shared<const QImage>(std::move(image));
}

// A view and its controller; the view has no window, so no frames are
// rendered (renderingSettled() is covered by qimgv_render_tests).
struct Viewport {
  explicit Viewport(const UiSettingsSnapshot &settings = testSettings())
      : controller(settings) {
    item.setSize(kViewport);
    controller.setView(&item);
  }

  ImageRenderItem item;
  ImageViewportController controller;
};
} // namespace

class ImageViewportControllerTests : public QObject {
  Q_OBJECT

private slots:
  void showImageDrivesTheView() {
    Viewport viewport;
    QSignalSpy imageChanged(&viewport.controller, &ImageViewportController::imageChanged);
    viewport.controller.showImage(testImage(kLargeImage), kFirstFile);

    QVERIFY(viewport.controller.hasImage());
    QCOMPARE(imageChanged.count(), 1);
    QCOMPARE(viewport.item.imageSize(), kLargeImage);
    QCOMPARE(viewport.item.imageScale(), 0.5);
    QCOMPARE(viewport.item.imagePosition(), QPointF(0, 0));
    QCOMPARE(viewport.controller.zoomPercent(), 50);
    // The settle pass is being presented.
    QVERIFY(viewport.item.isSettled());
    QVERIFY(!viewport.controller.isRenderingSettled());
    // MKS2021 (GPU) is the configured filter.
    QCOMPARE(viewport.item.resampling(), RenderEnums::Resampling::Mks2021);
    QCOMPARE(viewport.item.sampling(), RenderEnums::TextureSampling::Trilinear);
  }

  void nullImageIsRejected() {
    Viewport viewport;
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(u"null image"_s));
    viewport.controller.showImage(nullptr, kFirstFile);
    QVERIFY(!viewport.controller.hasImage());
  }

  void fitAndZoomActions() {
    Viewport viewport;
    ImageViewportController &controller = viewport.controller;
    controller.showImage(testImage(kLargeImage), kFirstFile);

    controller.zoomIn();
    QCOMPARE(controller.currentScale(), 0.5f * (1.0f + kZoomStep));
    QCOMPARE(viewport.item.imageScale(), qreal(controller.currentScale()));
    controller.zoomOut();
    QVERIFY(qAbs(controller.currentScale() - 0.5f) < kScaleTolerance);

    controller.fitOriginal();
    QCOMPARE(controller.currentScale(), 1.0f);
    controller.switchFitMode();
    QCOMPARE(controller.currentScale(), 0.5f);
    controller.switchFitMode();
    QCOMPARE(controller.currentScale(), 1.0f);
    controller.fitWidth();
    QCOMPARE(controller.currentScale(), 0.5f);
    controller.fitOriginal();
    controller.fitHeight();
    QCOMPARE(controller.currentScale(), 0.5f);
    controller.fitOriginal();
    controller.fitWindow();
    QCOMPARE(controller.currentScale(), 0.5f);
  }

  void zoomKeepsTheViewportCentre() {
    Viewport viewport;
    ImageViewportController &controller = viewport.controller;
    controller.showImage(testImage(kLargeImage), kFirstFile);
    controller.fitOriginal();
    const auto imagePointAtCentre = [&viewport]() {
      return (kCentre - viewport.item.imagePosition()) / viewport.item.imageScale();
    };
    const QPointF before = imagePointAtCentre();
    controller.zoomIn();
    const QPointF after = imagePointAtCentre();
    // Positions are whole pixels: the anchor may move by one screen pixel.
    QVERIFY2((before - after).manhattanLength() <= 2.0 / viewport.item.imageScale(),
             qPrintable(u"%1,%2 -> %3,%4"_s.arg(before.x()).arg(before.y()).arg(after.x()).arg(after.y())));
  }

  void smoothZoomAnimates() {
    UiSettingsSnapshot settings = testSettings();
    settings.viewer.smoothZoom = true;
    Viewport viewport(settings);
    ImageViewportController &controller = viewport.controller;
    if (controller.reducedMotion())
      QSKIP("The platform asks for reduced motion: zoom is not animated");
    controller.showImage(testImage(kLargeImage), kFirstFile);

    controller.zoomIn();
    QVERIFY(controller.currentScale() < 0.5f * (1.0f + kZoomStep));
    // A second zoom continues from the target of the running one.
    controller.zoomIn();
    const float target = 0.5f * (1.0f + kZoomStep) * (1.0f + kZoomStep);
    QTRY_VERIFY(qAbs(controller.currentScale() - target) < kScaleTolerance);
  }

  void leftDragPansLargeImage() {
    Viewport viewport;
    ImageViewportController &controller = viewport.controller;
    controller.showImage(testImage(kLargeImage), kFirstFile);
    controller.fitOriginal();
    const QPointF start = viewport.item.imagePosition();

    QVERIFY(!controller.pointerPressed(kCentre, kLeft, kNoModifiers));
    controller.pointerMoved(kCentre + QPointF(-10, -20), kLeft);
    QCOMPARE(viewport.item.imagePosition(), start + QPointF(-10, -20));
    QCOMPARE(controller.cursorShape(), Qt::ClosedHandCursor);
    QVERIFY(controller.isBusyInteracting());
    // Panning drops the settle pass.
    QVERIFY(!viewport.item.isSettled());

    QVERIFY(controller.pointerReleased(kCentre + QPointF(-10, -20)));
    QCOMPARE(controller.cursorShape(), Qt::ArrowCursor);
    QVERIFY(!controller.isBusyInteracting());
    // The release settles the view again.
    QVERIFY(viewport.item.isSettled());
  }

  void leftDragOnFittingImageDragsOut() {
    Viewport viewport;
    ImageViewportController &controller = viewport.controller;
    QSignalSpy draggedOut(&controller, &ImageViewportController::draggedOut);
    controller.showImage(testImage(kLargeImage), kFirstFile);

    controller.pointerPressed(kCentre, kLeft, kNoModifiers);
    controller.pointerMoved(kCentre + QPointF(5, 0), kLeft);
    QCOMPARE(draggedOut.count(), 0);
    controller.pointerMoved(kCentre + QPointF(20, 0), kLeft);
    controller.pointerMoved(kCentre + QPointF(40, 0), kLeft);
    QCOMPARE(draggedOut.count(), 1);
    QVERIFY(!controller.pointerReleased(kCentre + QPointF(40, 0)));
  }

  void rightStrokeRequestsImages() {
    Viewport viewport;
    ImageViewportController &controller = viewport.controller;
    QSignalSpy next(&controller, &ImageViewportController::nextImageRequested);
    QSignalSpy prev(&controller, &ImageViewportController::prevImageRequested);
    controller.showImage(testImage(kLargeImage), kFirstFile);

    controller.pointerPressed(kCentre, kRight, kNoModifiers);
    controller.pointerMoved(kCentre + QPointF(-50, 0), kRight);
    QVERIFY(controller.pointerReleased(kCentre + QPointF(-50, 0)));
    controller.pointerPressed(kCentre, kRight, kNoModifiers);
    controller.pointerMoved(kCentre + QPointF(50, 0), kRight);
    QVERIFY(controller.pointerReleased(kCentre + QPointF(50, 0)));
    QCOMPARE(next.count(), 1);
    QCOMPARE(prev.count(), 1);
  }

  void rightDragZoomsAndClickFallsThrough() {
    UiSettingsSnapshot settings = testSettings();
    settings.viewer.useUpscayl = true;
    Viewport viewport(settings);
    ImageViewportController &controller = viewport.controller;
    QSignalSpy scaling(&controller, &ImageViewportController::scalingRequested);
    controller.showImage(testImage(kSmallImage), kFirstFile);
    QCOMPARE(controller.currentScale(), 1.0f);

    controller.pointerPressed(kCentre, kRight, kNoModifiers);
    controller.pointerMoved(kCentre + QPointF(0, -10), kRight);
    controller.pointerMoved(kCentre + QPointF(0, -60), kRight);
    QVERIFY(controller.currentScale() > 1.0f);
    QCOMPARE(controller.cursorShape(), Qt::SizeVerCursor);
    QVERIFY(controller.isBusyInteracting());
    // No upscale source while the zoom continues.
    QCOMPARE(scaling.count(), 0);
    QVERIFY(controller.pointerReleased(kCentre + QPointF(0, -60)));
    QCOMPARE(scaling.count(), 1);

    // A right click without movement opens the menu (shortcut).
    controller.pointerPressed(kCentre, kRight, kNoModifiers);
    QVERIFY(!controller.pointerReleased(kCentre));
  }

  void upscaleSourceIsRequestedAboveOneToOne() {
    UiSettingsSnapshot settings = testSettings();
    settings.viewer.useUpscayl = true;
    Viewport viewport(settings);
    ImageViewportController &controller = viewport.controller;
    QSignalSpy scaling(&controller, &ImageViewportController::scalingRequested);
    controller.showImage(testImage(kSmallImage), kFirstFile);
    QCOMPARE(scaling.count(), 0);

    controller.zoomIn();
    QCOMPARE(scaling.count(), 1);
    const QSize expected =
        (QSizeF(kSmallImage) * (1.0 + kZoomStep)).toSize();
    QCOMPARE(scaling.at(0).at(0).toSize(), expected);
    // The CPU kernel of the GPU MKS2021 filter.
    QCOMPARE(scaling.at(0).at(1).value<ScalingFilter>(), QI_FILTER_MKS2021);

    settings.viewer.useUpscayl = false;
    controller.applySettings(settings);
    controller.zoomIn();
    QCOMPARE(scaling.count(), 1);
  }

  void wheelScrollsZoomsOrFallsThrough() {
    Viewport viewport;
    ImageViewportController &controller = viewport.controller;
    controller.showImage(testImage(kLargeImage), kFirstFile);
    const QPoint notchDown(0, -120);
    const QPoint notchUp(0, 120);

    // The whole image is visible: the wheel goes to the shortcuts.
    QVERIFY(!controller.wheelTurned(kCentre, notchDown, QPoint(), Qt::NoButton, kNoModifiers));
    // Modified wheels always do.
    controller.fitOriginal();
    QVERIFY(!controller.wheelTurned(kCentre, notchDown, QPoint(), Qt::NoButton, Qt::ControlModifier));

    // A trackpad scrolls at once.
    const qreal top = viewport.item.imagePosition().y();
    QVERIFY(controller.wheelTurned(kCentre, QPoint(0, -30), QPoint(), Qt::NoButton, kNoModifiers));
    QCOMPARE(viewport.item.imagePosition().y(), top - 21);

    // A mouse wheel scrolls an image that extends past the viewport.
    const qreal afterTrackpad = viewport.item.imagePosition().y();
    QTest::qWait(WheelClassifier::kTrackpadCooldownMs + 1);
    QVERIFY(controller.wheelTurned(kCentre, notchDown, QPoint(), Qt::NoButton, kNoModifiers));
    QTRY_VERIFY(viewport.item.imagePosition().y() <= afterTrackpad - 200);

    // Right button + wheel zooms at the pointer.
    const float scale = controller.currentScale();
    controller.pointerEntered(kCentre);
    controller.pointerPressed(kCentre, kRight, kNoModifiers);
    QVERIFY(controller.wheelTurned(kCentre, notchUp, QPoint(), Qt::RightButton, kNoModifiers));
    QVERIFY(controller.currentScale() > scale);
    QVERIFY(controller.pointerReleased(kCentre));
  }

  void wheelScrollingCanBeOff() {
    UiSettingsSnapshot settings = testSettings();
    settings.viewer.imageScrolling = SettingsEnums::ImageScrolling::None;
    Viewport viewport(settings);
    ImageViewportController &controller = viewport.controller;
    controller.showImage(testImage(kLargeImage), kFirstFile);
    controller.fitOriginal();
    const QPointF start = viewport.item.imagePosition();
    // Trackpad events are still taken (not turned into zoom shortcuts).
    QVERIFY(controller.wheelTurned(kCentre, QPoint(0, -30), QPoint(), Qt::NoButton, kNoModifiers));
    QCOMPARE(viewport.item.imagePosition(), start);
    QTest::qWait(WheelClassifier::kTrackpadCooldownMs + 1);
    QVERIFY(!controller.wheelTurned(kCentre, QPoint(0, -120), QPoint(), Qt::NoButton, kNoModifiers));
  }

  void panoramaMode() {
    Viewport viewport;
    ImageViewportController &controller = viewport.controller;
    controller.togglePanorama();
    QVERIFY2(!controller.panoramaMode(), "panorama mode needs an image");

    controller.showImage(testImage(kPanoramaImage), kFirstFile);
    QVERIFY(controller.isZoomIndicatorVisible());
    controller.togglePanorama();
    QVERIFY(controller.panoramaMode());
    QCOMPARE(viewport.item.projection(), RenderEnums::Projection::Equirectangular);
    QVERIFY(!controller.isZoomIndicatorVisible());

    controller.pointerPressed(kCentre, kLeft, kNoModifiers);
    controller.pointerMoved(kCentre + QPointF(40, 0), kLeft);
    controller.pointerReleased(kCentre + QPointF(40, 0));
    QVERIFY(viewport.item.panoramaYaw() != 0.0);
    QVERIFY(controller.wheelTurned(kCentre, QPoint(0, 120), QPoint(), Qt::NoButton, kNoModifiers));
    QVERIFY(viewport.item.panoramaFov() != PanoramaView::kDefaultFov);

    // Double click resets the camera.
    QVERIFY(controller.pointerDoubleClicked(kCentre, kLeft, kNoModifiers));
    QCOMPARE(viewport.item.panoramaYaw(), 0.0);
    QCOMPARE(viewport.item.panoramaFov(), qreal(PanoramaView::kDefaultFov));

    controller.togglePanorama();
    QCOMPARE(viewport.item.projection(), RenderEnums::Projection::Flat);
    QVERIFY(!controller.pointerDoubleClicked(kCentre, kLeft, kNoModifiers));
  }

  void clickZones() {
    UiSettingsSnapshot settings = testSettings();
    settings.viewer.clickableEdges = true;
    Viewport viewport(settings);
    ImageViewportController &controller = viewport.controller;
    QSignalSpy next(&controller, &ImageViewportController::nextImageRequested);
    QSignalSpy prev(&controller, &ImageViewportController::prevImageRequested);
    QVERIFY(controller.clickZonesEnabled());

    controller.pointerEntered(kLeftEdge);
    QCOMPARE(controller.highlightedClickZone(), ImageViewportController::ClickZone::Left);
    QCOMPARE(controller.cursorShape(), Qt::PointingHandCursor);
    controller.pointerMoved(kCentre, Qt::NoButton);
    QCOMPARE(controller.highlightedClickZone(), ImageViewportController::ClickZone::None);
    QCOMPARE(controller.cursorShape(), Qt::ArrowCursor);

    QVERIFY(controller.pointerPressed(kRightEdge, kLeft, kNoModifiers));
    QVERIFY(controller.isClickZonePressed());
    // The second press of a double click acts, the double click itself not.
    QVERIFY(controller.pointerDoubleClicked(kRightEdge, kLeft, kNoModifiers));
    controller.pointerReleased(kRightEdge);
    QVERIFY(!controller.isClickZonePressed());
    QVERIFY(controller.pointerPressed(kLeftEdge, kLeft, kNoModifiers));
    controller.pointerReleased(kLeftEdge);
    QCOMPARE(next.count(), 1);
    QCOMPARE(prev.count(), 1);

    // Modified clicks and other buttons are not zone clicks.
    QVERIFY(!controller.pointerPressed(kLeftEdge, kLeft, Qt::ControlModifier));
    QVERIFY(!controller.pointerPressed(kLeftEdge, kRight, kNoModifiers));
    controller.pointerReleased(kLeftEdge);
    QCOMPARE(prev.count(), 1);

    // A side panel or a narrow viewport disables them.
    viewport.item.setWidth(ImageViewportController::kClickZonesMinViewportWidth);
    QVERIFY(!controller.clickZonesEnabled());
    viewport.item.setWidth(kViewport.width());
    QVERIFY(controller.clickZonesEnabled());
    settings.panel.position = SettingsEnums::PanelPosition::Left;
    controller.applySettings(settings);
    QVERIFY(!controller.clickZonesEnabled());
  }

  void zoomIndicatorModes() {
    UiSettingsSnapshot settings = testSettings();
    settings.overlays.zoomIndicatorMode = SettingsEnums::ZoomIndicatorMode::Disabled;
    Viewport viewport(settings);
    ImageViewportController &controller = viewport.controller;
    controller.showImage(testImage(kLargeImage), kFirstFile);
    QVERIFY(!controller.isZoomIndicatorVisible());

    settings.overlays.zoomIndicatorMode = SettingsEnums::ZoomIndicatorMode::Enabled;
    controller.applySettings(settings);
    QVERIFY(controller.isZoomIndicatorVisible());

    settings.overlays.zoomIndicatorMode = SettingsEnums::ZoomIndicatorMode::Auto;
    controller.applySettings(settings);
    controller.zoomIn();
    QVERIFY(controller.isZoomIndicatorVisible());
    QTRY_VERIFY(!controller.isZoomIndicatorVisible());

    controller.closeImage();
    QVERIFY(!controller.isZoomIndicatorVisible());
  }

  void sessionOverridesFollowViewerSettings() {
    UiSettingsSnapshot settings = testSettings();
    Viewport viewport(settings);
    ImageViewportController &controller = viewport.controller;

    controller.setScalingFilter(QI_FILTER_NEAREST);
    QCOMPARE(viewport.item.sampling(), RenderEnums::TextureSampling::Nearest);
    QCOMPARE(viewport.item.resampling(), RenderEnums::Resampling::None);
    controller.setScalingFilter(QI_FILTER_CAS);
    QCOMPARE(viewport.item.sharpening(), RenderEnums::Sharpening::Cas);
    QCOMPARE(viewport.item.casSharpening(), 0.5);
    controller.toggleTransparencyGrid();
    QVERIFY(viewport.item.transparencyGrid());

    // Unrelated settings keep the overrides.
    settings.overlays.showSaveOverlay = !settings.overlays.showSaveOverlay;
    controller.applySettings(settings);
    QCOMPARE(controller.scalingFilter(), QI_FILTER_CAS);
    QVERIFY(viewport.item.transparencyGrid());

    // Viewer settings restore them.
    settings.viewer.casContrast = 0.75;
    controller.applySettings(settings);
    QCOMPARE(controller.scalingFilter(), QI_FILTER_MKS2021_GPU);
    QCOMPARE(viewport.item.casContrast(), 0.75);
    QVERIFY(!viewport.item.transparencyGrid());
  }

  void visibleOriginalImageRect() {
    Viewport viewport;
    ImageViewportController &controller = viewport.controller;
    QCOMPARE(controller.visibleOriginalImageRect(), QRect());
    controller.showImage(testImage(kLargeImage), kFirstFile);
    QCOMPARE(controller.visibleOriginalImageRect(), QRect(QPoint(0, 0), kLargeImage));

    controller.fitOriginal();
    const QPoint offset = (-viewport.item.imagePosition()).toPoint();
    QCOMPARE(controller.visibleOriginalImageRect(),
             QRect(offset, kViewport.toSize()));
  }

  void rotationKeepsTheView() {
    Viewport viewport;
    ImageViewportController &controller = viewport.controller;
    controller.showImage(testImage(kLargeImage), kFirstFile);
    controller.fitOriginal();

    const QSize rotated = kLargeImage.transposed();
    controller.showImage(testImage(rotated), kFirstFile);
    QCOMPARE(controller.currentScale(), 1.0f);
    QCOMPARE(viewport.item.imageSize(), rotated);

    controller.showImage(testImage(rotated), kSecondFile);
    QCOMPARE(controller.currentScale(), 600.0f / 1600.0f);
  }

  void closeImageClearsTheView() {
    Viewport viewport;
    ImageViewportController &controller = viewport.controller;
    controller.showImage(testImage(kLargeImage), kFirstFile);
    QSignalSpy imageChanged(&controller, &ImageViewportController::imageChanged);
    controller.closeImage();
    QVERIFY(!controller.hasImage());
    QCOMPARE(imageChanged.count(), 1);
    QCOMPARE(viewport.item.imageSize(), QSize());
  }

  void animationPlaysIntoTheView() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(u"three.gif"_s);
    {
      QFile file(path);
      QVERIFY(file.open(QIODevice::WriteOnly));
      const auto size = qsizetype(sizeof(TestAnimations::kThreeFrameGif));
      QCOMPARE(file.write(reinterpret_cast<const char *>(TestAnimations::kThreeFrameGif), size),
               size);
    }
    Viewport viewport;
    ImageViewportController &controller = viewport.controller;
    QSignalSpy imageChanged(&controller, &ImageViewportController::imageChanged);
    QSignalSpy errors(&controller, &ImageViewportController::playbackError);

    controller.showAnimation(path, u"gif"_s);
    QVERIFY(controller.hasImage());
    QCOMPARE(imageChanged.count(), 1);
    QCOMPARE(viewport.item.imageSize(), QSize(4, 4));
    // The settle pass does not run during playback.
    QVERIFY(!viewport.item.isSettled());
    controller.closeImage();
    QVERIFY(!controller.hasImage());

    controller.showAnimation(dir.filePath(u"missing.gif"_s), u"gif"_s);
    QCOMPARE(errors.count(), 1);
    QVERIFY(!controller.hasImage());
  }
};

int runImageViewportControllerTests(int argc, char **argv) {
  ImageViewportControllerTests tests;
  return QTest::qExec(&tests, argc, argv);
}

#include "tst_imageviewportcontroller.moc"
