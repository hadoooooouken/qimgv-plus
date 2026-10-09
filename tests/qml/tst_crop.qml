import QtQuick
import QtTest
import qimgv.bridges
import qimgv.ui
import qimgv.tests

// The crop mode over the Fixture's CropController, laid out as in Main.qml:
// the side panel at the right takes its space from the viewer, the overlay
// covers the viewer. The pointer draws the selection, the inputs show and
// edit it, the presets lock the ratio, the keys of the crop mode crop or
// cancel, and a right click picks the default action.
Item {
    id: root

    width: 800
    height: 600

    ImageViewport {
        id: viewport

        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: sidePanel.visible ? sidePanel.left : parent.right
        controller: Fixture.viewportController
        focus: !Fixture.crop.active
    }

    CropOverlay {
        id: overlay

        anchors.fill: viewport
        controller: Fixture.crop
    }

    SidePanel {
        id: sidePanel

        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        crop: Fixture.crop
        focus: Fixture.crop.active
    }

    TestCase {
        id: testCase

        readonly property int imageWidth: 400
        readonly property int imageHeight: 200
        readonly property size screenSize: Qt.size(1920, 1080)
        // Image pixels a pointer position may be off after rounding.
        readonly property int pixelTolerance: 2
        readonly property int wheelNotch: 120
        readonly property int timeout: 3000

        name: "Crop"
        when: windowShown

        function input(name) {
            const item = findChild(sidePanel, name);
            verify(item !== null, "no " + name + " in the crop panel");
            return item;
        }

        // Overlay position of a point given as fractions of the image area.
        function imagePoint(fx, fy) {
            const area = Fixture.crop.imageArea;
            return Qt.point(area.x + area.width * fx, area.y + area.height * fy);
        }

        function openCrop() {
            Fixture.toggleCrop(screenSize);
            verify(Fixture.crop.active);
            tryCompare(viewport, "width", root.width - sidePanel.width);
            tryVerify(() => Fixture.crop.imageArea.width === viewport.width, timeout,
                      "the image is not fitted to the narrower viewer");
        }

        function init() {
            Fixture.showTestImage(imageWidth, imageHeight);
            Fixture.clearInputLog();
            Fixture.clearCropRequest();
        }

        function cleanup() {
            if (Fixture.crop.active)
                Fixture.crop.cancel();
            Fixture.crop.chooseDefaultAction(SettingsEnums.CropAction.Crop);
            Fixture.clearCropRequest();
        }

        function test_opensWithThePanelAndTheWholeImage() {
            verify(!overlay.visible);
            verify(!sidePanel.visible);
            openCrop();
            verify(overlay.visible);
            verify(sidePanel.visible);
            // A small image is enlarged to the viewer.
            verify(Fixture.crop.imageArea.width > imageWidth);
            compare(input("widthInput").value, imageWidth);
            compare(input("heightInput").value, imageHeight);
            compare(input("xInput").value, 0);
            tryVerify(() => input("widthInput").activeFocus, timeout, "the width input has no focus");
        }

        function test_pointerDrawsASelection() {
            openCrop();
            const start = imagePoint(0.25, 0.25);
            const end = imagePoint(0.75, 0.75);
            mouseClick(overlay, start.x, start.y, Qt.RightButton);
            verify(!Fixture.crop.hasSelection);
            mousePress(overlay, start.x, start.y);
            mouseMove(overlay, (start.x + end.x) / 2, (start.y + end.y) / 2, -1, Qt.LeftButton);
            mouseMove(overlay, end.x, end.y, -1, Qt.LeftButton);
            mouseRelease(overlay, end.x, end.y);

            const selection = Fixture.crop.selection;
            fuzzyCompare(selection.x, imageWidth / 4, pixelTolerance);
            fuzzyCompare(selection.y, imageHeight / 4, pixelTolerance);
            fuzzyCompare(selection.width, imageWidth / 2, pixelTolerance);
            fuzzyCompare(selection.height, imageHeight / 2, pixelTolerance);
            compare(input("widthInput").value, selection.width);
            compare(input("yInput").value, selection.y);
            verify(Fixture.crop.handlesVisible);
        }

        function test_inputsEditTheSelection() {
            openCrop();
            const width = input("widthInput");
            tryVerify(() => width.activeFocus, timeout);
            width.contentItem.selectAll();
            keyClick(Qt.Key_1);
            keyClick(Qt.Key_2);
            keyClick(Qt.Key_0);
            keyClick(Qt.Key_Tab);
            compare(Fixture.crop.selection.width, 120);

            // Moved back into the image.
            const x = input("xInput");
            x.forceActiveFocus();
            x.contentItem.selectAll();
            keyClick(Qt.Key_3);
            keyClick(Qt.Key_9);
            keyClick(Qt.Key_0);
            keyClick(Qt.Key_Tab);
            compare(Fixture.crop.selection.x, imageWidth - 120);
            compare(x.value, imageWidth - 120);
        }

        function test_presetsLockTheRatio() {
            openCrop();
            const presets = input("aspectPresetInput");
            compare(presets.currentIndex, CropController.Free);
            presets.activated(CropController.Square);
            compare(Fixture.crop.aspectPreset, CropController.Square);
            compare(Fixture.crop.selection.width, imageHeight);
            compare(Fixture.crop.selection.height, imageHeight);
            compare(input("aspectWidthInput").value, 1);

            Fixture.crop.setCustomAspect(1, 2);
            compare(presets.currentIndex, CropController.Custom);
            compare(input("aspectHeightInput").value, 2);
        }

        function test_enterCropsAndEscapeCancels() {
            openCrop();
            Fixture.crop.setSelectionValues(10, 20, 30, 40);
            keyClick(Qt.Key_Return);
            compare(Fixture.lastCropRequest, "crop:10,20,30,40");
            verify(!Fixture.crop.active);
            tryCompare(viewport, "width", root.width);

            openCrop();
            Fixture.crop.setSelectionValues(10, 20, 30, 40);
            keyClick(Qt.Key_Return, Qt.ShiftModifier);
            compare(Fixture.lastCropRequest, "cropAndSave:10,20,30,40");

            Fixture.clearCropRequest();
            openCrop();
            keyClick(Qt.Key_Escape);
            verify(!Fixture.crop.active);
            compare(Fixture.lastCropRequest, "");
        }

        function test_rightClickPicksTheDefaultAction() {
            openCrop();
            const cropButton = input("cropButton");
            const saveButton = input("cropAndSaveButton");
            verify(findChild(cropButton, "defaultMarker").visible);
            verify(!findChild(saveButton, "defaultMarker").visible);
            mouseClick(saveButton, saveButton.width / 2, saveButton.height / 2, Qt.RightButton);
            compare(Fixture.lastCropRequest, "default:" + SettingsEnums.CropAction.CropAndSave);
            verify(Fixture.crop.active);
            verify(findChild(saveButton, "defaultMarker").visible);
            verify(!findChild(cropButton, "defaultMarker").visible);

            Fixture.crop.setSelectionValues(0, 0, 10, 10);
            keyClick(Qt.Key_Enter);
            compare(Fixture.lastCropRequest, "cropAndSave:0,0,10,10");
        }

        function test_panelTakesTheWheelAndTheOverlayForwardsIt() {
            openCrop();
            mouseWheel(sidePanel, sidePanel.width / 2, sidePanel.height / 2, 0, wheelNotch);
            compare(Fixture.lastWheelAngleDelta, Qt.point(0, 0));
            mouseWheel(overlay, overlay.width / 2, overlay.height / 2, 0, wheelNotch);
            compare(Fixture.lastWheelAngleDelta, Qt.point(0, wheelNotch));
        }

        // Saved next to the test executable for review.
        function test_rendersForReview() {
            openCrop();
            Fixture.crop.setSelectionValues(100, 40, 200, 120);
            waitForRendering(root);
            grabImage(root).save(Fixture.artifactPath("crop.png"));
        }

        function test_folderViewCloses() {
            openCrop();
            Fixture.setFolderViewActive(true);
            verify(!Fixture.crop.active);
            verify(!sidePanel.visible);
            Fixture.toggleCrop(screenSize);
            verify(!Fixture.crop.active);
            Fixture.setFolderViewActive(false);
        }
    }
}
