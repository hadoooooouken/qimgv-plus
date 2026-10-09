#include <QTest>

#include "components/scalingfilter/scalingfilterselection.h"
#include "components/viewtransform/viewportinteraction.h"
#include "gui/quick/ui/framepresentationtracker.h"
#include "testsuites.h"

#include <QSignalSpy>

Q_DECLARE_METATYPE(InteractionStep::Kind)

namespace {
constexpr QPoint kPress(400, 300);
// Trackpad cooldown boundary of WheelClassifier.
constexpr qint64 kCooldown = WheelClassifier::kTrackpadCooldownMs;

using Kind = InteractionStep::Kind;
using Mode = ViewportInteraction::Mode;

InteractionContext largeImage() {
  return {.hasImage = true, .panorama = false, .imageFits = false, .dragsEnabled = true};
}

InteractionContext fittingImage() {
  return {.hasImage = true, .panorama = false, .imageFits = true, .dragsEnabled = true};
}

ViewportInteraction pressedAt(QPoint pos, qreal dpr = 1.0) {
  ViewportInteraction interaction;
  interaction.setThresholds(InteractionThresholds::forDevicePixelRatio(dpr));
  interaction.press(pos);
  return interaction;
}
} // namespace

class ViewportInteractionTests : public QObject {
  Q_OBJECT

private slots:
  void thresholdsFollowDevicePixelRatio_data() {
    QTest::addColumn<qreal>("dpr");
    QTest::addColumn<int>("zoom");
    QTest::addColumn<int>("gesture");
    QTest::newRow("1.0") << 1.0 << 4 << 40;
    QTest::newRow("1.5") << 1.5 << 6 << 60;
    QTest::newRow("2.0") << 2.0 << 8 << 80;
  }

  void thresholdsFollowDevicePixelRatio() {
    QFETCH(qreal, dpr);
    QFETCH(int, zoom);
    QFETCH(int, gesture);
    const InteractionThresholds thresholds = InteractionThresholds::forDevicePixelRatio(dpr);
    QCOMPARE(thresholds.zoom, zoom);
    QCOMPARE(thresholds.gesture, gesture);
    // The drag-out threshold is not scaled (as in the widget viewer).
    QCOMPARE(thresholds.dragOut, 10);
  }

  void leftButtonPansLargeImage() {
    ViewportInteraction interaction = pressedAt(kPress);
    const InteractionStep first =
        interaction.move(kPress + QPoint(-10, -5), Qt::LeftButton, largeImage());
    QCOMPARE(first, (InteractionStep{.kind = Kind::Pan, .delta = QPoint(10, 5)}));
    QCOMPARE(interaction.mode(), Mode::Pan);
    QVERIFY(interaction.isBusy());
    const InteractionStep second =
        interaction.move(kPress + QPoint(-4, -5), Qt::LeftButton, largeImage());
    QCOMPARE(second, (InteractionStep{.kind = Kind::Pan, .delta = QPoint(-6, 0)}));

    const ViewportInteraction::Release release = interaction.release(true);
    QVERIFY(release.consumed);
    QVERIFY(release.rescale);
    QCOMPARE(interaction.mode(), Mode::None);
  }

  void panStopsWhenImageFits() {
    ViewportInteraction interaction = pressedAt(kPress);
    QCOMPARE(interaction.move(kPress + QPoint(5, 0), Qt::LeftButton, largeImage()).kind,
             Kind::Pan);
    QCOMPARE(interaction.move(kPress + QPoint(9, 0), Qt::LeftButton, fittingImage()).kind,
             Kind::None);
    QCOMPARE(interaction.mode(), Mode::Pan);
  }

  void leftButtonDragsFittingImageOut() {
    ViewportInteraction interaction = pressedAt(kPress);
    QCOMPARE(interaction.move(kPress + QPoint(10, 10), Qt::LeftButton, fittingImage()).kind,
             Kind::None);
    QCOMPARE(interaction.mode(), Mode::DragBegin);
    QVERIFY(interaction.isBusy());
    QCOMPARE(interaction.move(kPress + QPoint(0, 11), Qt::LeftButton, fittingImage()).kind,
             Kind::DragOut);
    QCOMPARE(interaction.mode(), Mode::DragOut);
    QVERIFY(!interaction.isBusy());
    // One drag per press.
    QCOMPARE(interaction.move(kPress + QPoint(0, 40), Qt::LeftButton, fittingImage()).kind,
             Kind::None);

    const ViewportInteraction::Release release = interaction.release(true);
    QVERIFY(!release.consumed);
    QVERIFY(!release.rescale);
  }

  void dragOutNeedsDragsEnabled() {
    ViewportInteraction interaction = pressedAt(kPress);
    InteractionContext context = fittingImage();
    context.dragsEnabled = false;
    QCOMPARE(interaction.move(kPress + QPoint(50, 0), Qt::LeftButton, context).kind, Kind::None);
    QCOMPARE(interaction.mode(), Mode::None);
    QVERIFY(!interaction.release(true).consumed);
  }

  void rightButtonVerticalStrokeZooms() {
    ViewportInteraction interaction = pressedAt(kPress);
    // Within the threshold nothing is decided.
    QCOMPARE(interaction.move(kPress + QPoint(0, -4), Qt::RightButton, largeImage()).kind,
             Kind::None);
    QCOMPARE(interaction.mode(), Mode::None);
    QCOMPARE(interaction.move(kPress + QPoint(0, -6), Qt::RightButton, largeImage()).kind,
             Kind::None);
    QCOMPARE(interaction.mode(), Mode::Zoom);
    // The first zoom step covers the movement since the press.
    QCOMPARE(interaction.move(kPress + QPoint(0, -10), Qt::RightButton, largeImage()),
             (InteractionStep{.kind = Kind::GestureZoom, .distance = 10}));
    QCOMPARE(interaction.move(kPress + QPoint(0, -8), Qt::RightButton, largeImage()),
             (InteractionStep{.kind = Kind::GestureZoom, .distance = -2}));

    const ViewportInteraction::Release release = interaction.release(true);
    QVERIFY(release.consumed);
    QVERIFY(release.rescale);
  }

  void rightButtonThresholdScalesWithDpr() {
    ViewportInteraction interaction = pressedAt(kPress, 2.0);
    QCOMPARE(interaction.move(kPress + QPoint(0, -7), Qt::RightButton, largeImage()).kind,
             Kind::None);
    QCOMPARE(interaction.mode(), Mode::None);
    QCOMPARE(interaction.move(kPress + QPoint(0, -9), Qt::RightButton, largeImage()).kind,
             Kind::None);
    QCOMPARE(interaction.mode(), Mode::Zoom);
  }

  void rightButtonHorizontalStrokeIsGesture_data() {
    QTest::addColumn<int>("dx");
    QTest::addColumn<InteractionStep::Kind>("kind");
    QTest::newRow("leftwards: next") << -41 << Kind::NextImage;
    QTest::newRow("rightwards: previous") << 41 << Kind::PrevImage;
  }

  void rightButtonHorizontalStrokeIsGesture() {
    QFETCH(int, dx);
    QFETCH(InteractionStep::Kind, kind);
    ViewportInteraction interaction = pressedAt(kPress);
    // Dominant but short: undecided.
    QCOMPARE(interaction.move(kPress + QPoint(dx / 2, 0), Qt::RightButton, largeImage()).kind,
             Kind::None);
    QCOMPARE(interaction.mode(), Mode::None);
    QCOMPARE(interaction.move(kPress + QPoint(dx, 0), Qt::RightButton, largeImage()).kind, kind);
    QCOMPARE(interaction.mode(), Mode::Gesture);
    QCOMPARE(interaction.move(kPress + QPoint(dx * 2, 0), Qt::RightButton, largeImage()).kind,
             Kind::None);
    QVERIFY(interaction.release(true).consumed);
  }

  void diagonalStrokeWithoutDominanceZooms() {
    ViewportInteraction interaction = pressedAt(kPress);
    // |dx| = 10 is not more than twice |dy| = 6.
    QCOMPARE(interaction.move(kPress + QPoint(10, -6), Qt::RightButton, largeImage()).kind,
             Kind::None);
    QCOMPARE(interaction.mode(), Mode::Zoom);
  }

  void panoramaDragRotatesCamera() {
    ViewportInteraction interaction = pressedAt(kPress);
    InteractionContext context = largeImage();
    context.panorama = true;
    QCOMPARE(interaction.move(kPress + QPoint(7, -3), Qt::LeftButton, context),
             (InteractionStep{.kind = Kind::PanoramaDrag, .delta = QPoint(7, -3)}));
    QCOMPARE(interaction.move(kPress + QPoint(9, -3), Qt::LeftButton, context),
             (InteractionStep{.kind = Kind::PanoramaDrag, .delta = QPoint(2, 0)}));
  }

  void nothingHappensWithoutImage() {
    ViewportInteraction interaction = pressedAt(kPress);
    const InteractionContext empty{};
    QCOMPARE(interaction.move(kPress + QPoint(0, -50), Qt::RightButton, empty).kind, Kind::None);
    QCOMPARE(interaction.move(kPress + QPoint(50, 0), Qt::LeftButton, empty).kind, Kind::None);
    QVERIFY(!interaction.release(false).consumed);
  }

  void wheelZoomBlocksMovesUntilRelease() {
    ViewportInteraction interaction = pressedAt(kPress);
    interaction.beginWheelZoom();
    QVERIFY(interaction.isBusy());
    QCOMPARE(interaction.move(kPress + QPoint(0, -50), Qt::RightButton, largeImage()).kind,
             Kind::None);
    const ViewportInteraction::Release release = interaction.release(true);
    QVERIFY(release.consumed);
    QVERIFY(release.rescale);
  }

  void releaseWithoutInteractionIsNotConsumed() {
    ViewportInteraction interaction = pressedAt(kPress);
    const ViewportInteraction::Release release = interaction.release(true);
    QVERIFY(!release.consumed);
    QVERIFY(!release.rescale);
  }

  void resetPressPositionAvoidsStaleDragOut() {
    ViewportInteraction interaction = pressedAt(kPress);
    interaction.resetPressPosition(kPress + QPoint(100, 0));
    QCOMPARE(interaction.move(kPress + QPoint(95, 0), Qt::LeftButton, fittingImage()).kind,
             Kind::None);
    QCOMPARE(interaction.mode(), Mode::DragBegin);
  }

  void wheelClassifier() {
    WheelClassifier classifier;
    QVERIFY(classifier.isMouseWheel(QPoint(0, 120), true, 0));
    QVERIFY(classifier.isMouseWheel(QPoint(0, -240), true, 1));
    QVERIFY(classifier.isMouseWheel(QPoint(0, 180), true, 2));
    // Half a notch, odd values and horizontal-only events are trackpads.
    QVERIFY(!classifier.isMouseWheel(QPoint(0, 60), true, 3));
    // A trackpad stream suppresses wheel detection during the cooldown.
    QVERIFY(!classifier.isMouseWheel(QPoint(0, 120), true, 3 + kCooldown));
    QVERIFY(classifier.isMouseWheel(QPoint(0, 120), true, 3 + 2 * kCooldown + 1));
    QVERIFY(!classifier.isMouseWheel(QPoint(0, 100), true, 10'000));
    QVERIFY(!classifier.isMouseWheel(QPoint(120, 0), true, 20'000));
    // Without detection everything is a wheel.
    WheelClassifier undetected;
    QVERIFY(undetected.isMouseWheel(QPoint(0, 7), false, 0));
    QVERIFY(undetected.isMouseWheel(QPoint(0, 120), false, 1));
  }

  void scrollDeltas() {
    QCOMPARE(trackpadScrollDelta(QPoint(0, 10), QPoint(0, 30)), QPointF(0.0, -21.0));
    QCOMPARE(trackpadScrollDelta(QPoint(20, 0), QPoint(5, 0)), QPointF(-14.0, 0.0));
    QCOMPARE(wheelScrollDistance(-120, 1.0), 240);
    QCOMPARE(wheelScrollDistance(120, 0.5), -120);

    const QRect tall(0, -100, 800, 1000);
    QVERIFY(wheelCanScroll(-120, tall, 600));
    QVERIFY(wheelCanScroll(120, tall, 600));
    const QRect atTop(0, 0, 800, 1000);
    QVERIFY(!wheelCanScroll(120, atTop, 600));
    QVERIFY(wheelCanScroll(-120, atTop, 600));
    // Two pixels of misalignment do not count.
    QVERIFY(!wheelCanScroll(-120, QRect(0, 0, 800, 602), 600));
    QVERIFY(!wheelCanScroll(120, QRect(0, -2, 800, 600), 600));
  }

  void scalingFilterSelection() {
    QCOMPARE(ScalingFilterSelection::toggled(QI_FILTER_MKS2021_GPU, QI_FILTER_MKS2021_GPU),
             QI_FILTER_NEAREST);
    QCOMPARE(ScalingFilterSelection::toggled(QI_FILTER_NEAREST, QI_FILTER_MKS2021_GPU),
             QI_FILTER_MKS2021_GPU);
    QCOMPARE(ScalingFilterSelection::toggled(QI_FILTER_CAS, QI_FILTER_BILINEAR),
             QI_FILTER_BILINEAR);
    QCOMPARE(ScalingFilterSelection::toggled(QI_FILTER_NEAREST, QI_FILTER_NEAREST),
             QI_FILTER_NEAREST);
    QCOMPARE(ScalingFilterSelection::next(QI_FILTER_NEAREST), QI_FILTER_BILINEAR);
    QCOMPARE(ScalingFilterSelection::next(QI_FILTER_MKS2021), QI_FILTER_MKS2021_GPU);
    QCOMPARE(ScalingFilterSelection::next(QI_FILTER_MKS2021_GPU), QI_FILTER_NEAREST);
  }

  // The render-thread entry points are called directly here; the frame loop
  // integration is covered by qimgv_render_tests.
  void framePresentationNeedsAFrameSynchronizedAfterTheRequest() {
    FramePresentationTracker tracker;
    QSignalSpy presented(&tracker, &FramePresentationTracker::presented);

    // A frame without a request reports nothing.
    tracker.frameSynchronized();
    tracker.frameEnded();
    QCoreApplication::processEvents();
    QCOMPARE(presented.count(), 0);

    // A frame synchronized before the request does not complete it.
    tracker.frameSynchronized();
    tracker.requestPresentation();
    QVERIFY(tracker.isPending());
    tracker.frameEnded();
    QCoreApplication::processEvents();
    QCOMPARE(presented.count(), 0);

    tracker.frameSynchronized();
    tracker.frameEnded();
    QCoreApplication::processEvents();
    QCOMPARE(presented.count(), 1);
    QVERIFY(!tracker.isPending());

    // Further frames do not repeat it.
    tracker.frameSynchronized();
    tracker.frameEnded();
    QCoreApplication::processEvents();
    QCOMPARE(presented.count(), 1);
  }

  void framePresentationSupersededAndCancelled() {
    FramePresentationTracker tracker;
    QSignalSpy presented(&tracker, &FramePresentationTracker::presented);

    tracker.requestPresentation();
    tracker.frameSynchronized();
    // A newer request arrives while the frame of the first one renders.
    tracker.requestPresentation();
    tracker.frameEnded();
    QCoreApplication::processEvents();
    QCOMPARE(presented.count(), 0);
    tracker.frameSynchronized();
    tracker.frameEnded();
    QCoreApplication::processEvents();
    QCOMPARE(presented.count(), 1);

    tracker.requestPresentation();
    tracker.cancel();
    QVERIFY(!tracker.isPending());
    tracker.frameSynchronized();
    tracker.frameEnded();
    QCoreApplication::processEvents();
    QCOMPARE(presented.count(), 1);
  }
};

int runViewportInteractionTests(int argc, char **argv) {
  ViewportInteractionTests tests;
  return QTest::qExec(&tests, argc, argv);
}

#include "tst_viewportinteraction.moc"
