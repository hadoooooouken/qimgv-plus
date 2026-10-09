#include <QGuiApplication>
#include <QScreen>
#include <QSignalSpy>
#include <QTest>
#include <QWindow>

#include "components/windowstate/windowstatecontroller.h"
#include "testsuites.h"

namespace {
constexpr QRect kSavedGeometry(60, 40, 400, 300);
constexpr QRect kMovedGeometry(80, 70, 420, 310);
// Far outside any display of the offscreen platform.
constexpr QPoint kOffscreenPosition(50'000, 50'000);
// Longer than WindowStateController::kPlacementSettleDelayMs.
constexpr int kQuietPeriodMs = 150;

WindowPlacement savedPlacement() {
  return {.geometry = kSavedGeometry, .maximized = false, .display = 0};
}

QRect primaryScreenGeometry() {
  return QGuiApplication::primaryScreen()->geometry();
}
} // namespace

class WindowStateTests : public QObject {
  Q_OBJECT

private slots:
  void init() {
    QVERIFY2(QGuiApplication::primaryScreen(), "the test platform has no display");
  }

  void restoresTheSavedGeometryOnTheHiddenWindow() {
    QWindow window;
    WindowStateController controller(window, savedPlacement());
    QVERIFY(!window.isVisible());
    QCOMPARE(window.geometry(), kSavedGeometry);
    QCOMPARE(window.windowStates(), Qt::WindowStates(Qt::WindowNoState));
    QVERIFY(!controller.isFullscreen());
  }

  void restoresTheMaximizedState() {
    QWindow window;
    WindowPlacement placement = savedPlacement();
    placement.maximized = true;
    WindowStateController controller(window, placement);
    QVERIFY(window.windowStates().testFlag(Qt::WindowMaximized));
  }

  void movesAGeometryWithoutADisplayOntoThePrimaryDisplay() {
    QWindow window;
    WindowPlacement placement = savedPlacement();
    placement.geometry.moveTopLeft(kOffscreenPosition);
    WindowStateController controller(window, placement);
    QCOMPARE(window.geometry().size(), kSavedGeometry.size());
    QCOMPARE(window.geometry().center(),
             QGuiApplication::primaryScreen()->availableGeometry().center());
  }

  void showsWindowedWithTheSavedGeometry() {
    QWindow window;
    WindowStateController controller(window, savedPlacement());
    QSignalSpy fullscreenSpy(&controller, &WindowStateController::fullscreenChanged);
    controller.showWindowed();
    QVERIFY(window.isVisible());
    QCOMPARE(window.geometry(), kSavedGeometry);
    QVERIFY(!window.flags().testFlag(Qt::FramelessWindowHint));
    QCOMPARE(fullscreenSpy.count(), 0);
  }

  void pseudoFullscreenCoversTheDisplayAndReturns() {
    QWindow window;
    WindowStateController controller(window, savedPlacement());
    controller.showWindowed();
    QSignalSpy fullscreenSpy(&controller, &WindowStateController::fullscreenChanged);

    controller.showFullscreen();
    QVERIFY(controller.isFullscreen());
    QVERIFY(window.flags().testFlag(Qt::FramelessWindowHint));
    QCOMPARE(window.geometry(), primaryScreenGeometry());
    QCOMPARE(fullscreenSpy.count(), 1);
    QCOMPARE(fullscreenSpy.last().first().toBool(), true);

    controller.showWindowed();
    QVERIFY(!controller.isFullscreen());
    QVERIFY(!window.flags().testFlag(Qt::FramelessWindowHint));
    QCOMPARE(window.geometry(), kSavedGeometry);
    QCOMPARE(fullscreenSpy.count(), 2);
    QCOMPARE(fullscreenSpy.last().first().toBool(), false);
  }

  void toggleSwitchesBetweenTheStates() {
    QWindow window;
    WindowStateController controller(window, savedPlacement());
    controller.showWindowed();
    controller.toggleFullscreen();
    QVERIFY(controller.isFullscreen());
    controller.toggleFullscreen();
    QVERIFY(!controller.isFullscreen());
  }

  void startsInFullscreenAndReturnsToTheSavedGeometry() {
    QWindow window;
    WindowStateController controller(window, savedPlacement());
    controller.showFullscreen();
    QVERIFY(window.isVisible());
    QCOMPARE(window.geometry(), primaryScreenGeometry());
    controller.showWindowed();
    QCOMPARE(window.geometry(), kSavedGeometry);
  }

  void leavingFullscreenRestoresAMaximizedWindow() {
    QWindow window;
    WindowPlacement placement = savedPlacement();
    placement.maximized = true;
    WindowStateController controller(window, placement);
    controller.showWindowed();
    controller.showFullscreen();
    QCOMPARE(window.windowStates(), Qt::WindowStates(Qt::WindowNoState));
    controller.showWindowed();
    QVERIFY(window.windowStates().testFlag(Qt::WindowMaximized));
    QVERIFY(controller.placement().maximized);
  }

  void reportsTheGeometryOnceTheWindowRests() {
    QWindow window;
    WindowStateController controller(window, savedPlacement());
    controller.showWindowed();
    QSignalSpy placementSpy(&controller, &WindowStateController::placementChanged);

    window.setGeometry(kMovedGeometry);
    QVERIFY(placementSpy.wait());
    QCOMPARE(placementSpy.count(), 1);
    const auto reported = placementSpy.last().first().value<WindowPlacement>();
    QCOMPARE(reported.geometry, kMovedGeometry);
    QCOMPARE(controller.placement().geometry, kMovedGeometry);
  }

  void doesNotReportTheFullscreenGeometry() {
    QWindow window;
    WindowStateController controller(window, savedPlacement());
    controller.showWindowed();
    controller.showFullscreen();
    QSignalSpy placementSpy(&controller, &WindowStateController::placementChanged);

    window.setGeometry(kMovedGeometry);
    QTest::qWait(kQuietPeriodMs);
    QCOMPARE(placementSpy.count(), 0);
    QCOMPARE(controller.placement().geometry, kSavedGeometry);
  }

  void remembersAMaximizeWhileShown() {
    QWindow window;
    WindowStateController controller(window, savedPlacement());
    controller.showWindowed();
    window.setWindowStates(Qt::WindowMaximized);
    QTRY_VERIFY(controller.placement().maximized);
    // The normal geometry is kept for the way back.
    QCOMPARE(controller.placement().geometry, kSavedGeometry);

    window.setWindowStates(Qt::WindowNoState);
    QTRY_VERIFY(!controller.placement().maximized);
  }

  void savePlacementReportsImmediately() {
    QWindow window;
    WindowStateController controller(window, savedPlacement());
    controller.showWindowed();
    QSignalSpy placementSpy(&controller, &WindowStateController::placementChanged);
    window.setGeometry(kMovedGeometry);

    controller.savePlacement();
    QCOMPARE(placementSpy.count(), 1);
    QCOMPARE(placementSpy.last().first().value<WindowPlacement>().geometry, kMovedGeometry);
    QCOMPARE(placementSpy.last().first().value<WindowPlacement>().display, 0);
    // The pending debounced report was folded into this one.
    QTest::qWait(kQuietPeriodMs);
    QCOMPARE(placementSpy.count(), 1);
  }
};

int runWindowStateTests(int argc, char **argv) {
  WindowStateTests tests;
  return QTest::qExec(&tests, argc, argv);
}

#include "tst_windowstate.moc"
