import QtQuick
import QtTest
import qimgv.ui
import qimgv.tests

// ImageViewport over the Fixture's ImageViewportController: pointer input
// reaches the controller, and input it does not consume reaches the action
// shortcuts (the fake dispatcher of the Fixture). The viewport is a sibling
// of the TestCase, which is not visible and would not get pointer events.
Item {
    id: root

    width: 800
    height: 600

    ImageViewport {
        id: viewport

        anchors.fill: parent
        controller: Fixture.viewportController
    }

    TestCase {
        id: testCase

        // QEvent::Type values of mouse button events.
        readonly property int mouseButtonPress: 2
        readonly property int mouseButtonRelease: 3
        readonly property int mouseButtonDblClick: 4
        readonly property int wheelNotch: 120
        readonly property int centreX: 400
        readonly property int centreY: 300

        name: "ImageViewport"
        when: windowShown

        function init() {
            Fixture.showTestImage(1600, 1200);
            Fixture.clearInputLog();
        }

        function test_controllerDrivesTheView() {
            verify(Fixture.viewportController.view !== null);
            verify(Fixture.viewportController.hasImage);
            compare(Fixture.viewportController.zoomPercent, 50);
        }

        function test_keysGoToShortcuts() {
            viewport.forceActiveFocus();
            keyClick(Qt.Key_Right);
            compare(Fixture.lastKey, Qt.Key_Right);
        }

        function test_doubleClickGoesToShortcuts() {
            mouseDoubleClickSequence(viewport, centreX, centreY, Qt.LeftButton);
            // Both presses form the LMB shortcut, then the double click.
            compare(Fixture.mouseEventTypes, [mouseButtonPress, mouseButtonRelease,
                                              mouseButtonPress, mouseButtonDblClick,
                                              mouseButtonRelease]);
            verify(Fixture.mouseButtons.every(button => button === Qt.LeftButton));
        }

        function test_rightClickReleaseGoesToShortcuts() {
            mouseClick(viewport, centreX, centreY, Qt.RightButton);
            compare(Fixture.mouseEventTypes, [mouseButtonPress, mouseButtonRelease]);
            compare(Fixture.mouseButtons, [Qt.RightButton, Qt.RightButton]);
        }

        function test_rightDragZoomsWithoutMenu() {
            mousePress(viewport, centreX, centreY, Qt.RightButton);
            mouseMove(viewport, centreX, centreY - 10, -1, Qt.RightButton);
            mouseMove(viewport, centreX, centreY - 80, -1, Qt.RightButton);
            mouseRelease(viewport, centreX, centreY - 80, Qt.RightButton);
            verify(Fixture.viewportController.zoomPercent > 50);
            // The release ended the zoom; it is not a right click.
            compare(Fixture.mouseEventTypes, [mouseButtonPress]);
        }

        function test_unusedWheelGoesToShortcuts() {
            // The whole image is visible: nothing to scroll.
            mouseWheel(viewport, centreX, centreY, 0, -wheelNotch);
            compare(Fixture.lastWheelAngleDelta, Qt.point(0, -wheelNotch));
        }
    }
}
