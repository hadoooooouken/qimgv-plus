import QtQuick
import QtTest
import qimgv.ui
import qimgv.tests

// The overlay layer over the Fixture's OverlayCoordinator: every overlay is
// created on first use and reachable from its request, the keyboard overlays
// take the focus, the folder view closes the document overlays, and the
// overlays report their requests. The layer is a sibling of the TestCase,
// which is not visible and would not get input.
Item {
    id: root

    width: 800
    height: 600

    FocusScope {
        id: viewer

        anchors.fill: parent
        focus: !Fixture.overlays.keyboardOverlayOpen
    }

    OverlayLayer {
        id: layer

        anchors.fill: parent
        coordinator: Fixture.overlays
        focus: Fixture.overlays.keyboardOverlayOpen
    }

    TestCase {
        id: testCase

        // Loader order in OverlayLayer.qml.
        readonly property int infoBarLoader: 0
        readonly property int controlsLoader: 1
        readonly property int imageInfoLoader: 2
        readonly property int colorAdjustmentsLoader: 3
        readonly property int casSettingsLoader: 4
        readonly property int saveConfirmLoader: 5
        readonly property int copyLoader: 6
        readonly property int renameLoader: 7
        readonly property int messageLoader: 8
        readonly property int loaderCount: 9

        readonly property int messageTimeout: 3000
        readonly property int chromeTimeout: 4000

        name: "Overlays"
        when: windowShown

        function loader(index) {
            return layer.children[index];
        }

        // Waits for the asynchronous creation of the overlay of loader index.
        function overlay(index) {
            const item = loader(index);
            tryVerify(() => item.status === Loader.Ready, messageTimeout,
                      "overlay " + index + " was not created");
            return item.item;
        }

        function findText(item, text) {
            if (item.text === text && item.visible)
                return item;
            for (let i = 0; i < item.children.length; ++i) {
                const found = findText(item.children[i], text);
                if (found)
                    return found;
            }
            return null;
        }

        function init() {
            Fixture.showTestImage(400, 300);
        }

        function cleanup() {
            Fixture.closeOverlays();
            Fixture.setFolderViewActive(false);
            Fixture.setFullscreen(false);
        }

        // Runs first (alphabetical order): nothing was requested yet.
        function test_aaNothingIsCreatedAtStartup() {
            compare(layer.children.length, loaderCount);
            for (let i = 0; i < loaderCount; ++i)
                verify(loader(i).item === null, "overlay " + i + " exists at startup");
        }

        function test_messageShowsAndFades() {
            Fixture.showMessage("Hello");
            const message = overlay(messageLoader);
            verify(findText(message, "Hello") !== null);
            verify(Fixture.overlays.messages.visible);
            tryCompare(Fixture.overlays.messages, "visible", false, messageTimeout);
            tryVerify(() => findText(message, "Hello") === null, messageTimeout);
        }

        function test_imageInfoListsMetadata() {
            Fixture.setMetadataEntries(3);
            Fixture.toggleImageInfo();
            const info = overlay(imageInfoLoader);
            verify(info.visible);
            verify(findText(info, "Name 2") !== null);
            verify(findText(info, "Value 0") !== null);

            Fixture.setMetadataEntries(0);
            verify(findText(info, "<no metadata found>") !== null);

            Fixture.toggleImageInfo();
            verify(!info.visible);
        }

        function test_folderViewHidesAndRestoresImageInfo() {
            Fixture.toggleImageInfo();
            const info = overlay(imageInfoLoader);
            Fixture.setFolderViewActive(true);
            verify(!info.visible);
            Fixture.toggleImageInfo();
            verify(!Fixture.overlays.imageInfo.open, "image info opened in folder view");
            Fixture.setFolderViewActive(false);
            verify(info.visible);
        }

        function test_copyTakesFocusAndDigitsPickFolders() {
            Fixture.toggleCopy();
            const copy = overlay(copyLoader);
            verify(copy.visible);
            verify(findText(copy, "Copy to...") !== null);
            tryVerify(() => copy.activeFocus);

            keyClick(Qt.Key_2);
            compare(Fixture.lastFileRequest, "copy:C:/fixture/second");

            Fixture.clearInputLog();
            keyClick(Qt.Key_Right);
            compare(Fixture.lastKey, Qt.Key_Right);

            Fixture.toggleMove();
            verify(copy.visible);
            verify(findText(copy, "Move to...") !== null);
            keyClick(Qt.Key_1);
            compare(Fixture.lastFileRequest, "move:C:/fixture/first");

            Fixture.toggleMove();
            verify(!copy.visible);
            tryVerify(() => viewer.activeFocus);
        }

        function test_copyRowClickRequestsTheFolder() {
            Fixture.toggleCopy();
            const copy = overlay(copyLoader);
            const row = findText(copy, "third");
            verify(row !== null);
            mouseClick(row);
            compare(Fixture.lastFileRequest, "copy:C:/fixture/third");
        }

        function test_renameEditsTheBaseName() {
            Fixture.toggleRename("photo.jpg");
            const rename = overlay(renameLoader);
            verify(rename.visible);
            tryVerify(() => rename.activeFocus);
            const field = findText(rename, "photo.jpg");
            verify(field !== null);
            compare(field.selectedText, "photo");

            keyClick(Qt.Key_X);
            keyClick(Qt.Key_Return);
            compare(Fixture.lastFileRequest, "rename:x.jpg");
            verify(!Fixture.overlays.rename.open);
        }

        function test_renameEscapeCancelsAndEmptyIsRejected() {
            Fixture.toggleRename("photo.jpg");
            const rename = overlay(renameLoader);
            tryVerify(() => rename.activeFocus);
            keyClick(Qt.Key_A, Qt.ControlModifier);
            keyClick(Qt.Key_Delete);
            keyClick(Qt.Key_Return);
            verify(Fixture.overlays.rename.open, "an empty name was accepted");
            keyClick(Qt.Key_Escape);
            verify(!Fixture.overlays.rename.open);
            compare(Fixture.lastFileRequest, "");
        }

        function test_colorAdjustmentsOpenAtThePointer() {
            Fixture.pointerMoved(Qt.point(100, 120));
            Fixture.toggleColorAdjustments();
            const adjustments = overlay(colorAdjustmentsLoader);
            verify(adjustments.visible);
            const title = findText(adjustments, "Color adjustments");
            verify(title !== null);
            const origin = title.mapToItem(root, 0, 0);
            verify(origin.x >= 100 && origin.x < 200, "panel x " + origin.x);
            verify(origin.y >= 120 && origin.y < 170, "panel y " + origin.y);
            verify(findText(adjustments, "+0.00") !== null);
            Fixture.toggleColorAdjustments();
            verify(!adjustments.visible);
        }

        function test_casSettingsShowTheParameters() {
            Fixture.toggleCasSettings();
            const cas = overlay(casSettingsLoader);
            verify(cas.visible);
            verify(findText(cas, "CAS Settings") !== null);
            verify(findText(cas, "Sharpening") !== null);
        }

        function test_saveConfirmFollowsTheSetting() {
            // The fixture settings have showSaveOverlay off.
            Fixture.setSaveConfirmVisible(true);
            verify(!Fixture.overlays.saveConfirm.open);
            verify(loader(saveConfirmLoader).item === null);
        }

        function test_fullscreenControlsShowAndFade() {
            Fixture.setFullscreen(true);
            const controls = overlay(controlsLoader);
            verify(Fixture.overlays.fullscreenChrome.controlsShown);
            tryCompare(Fixture.overlays.fullscreenChrome, "controlsShown", false, chromeTimeout);
            Fixture.pointerMoved(Qt.point(10, 10));
            verify(Fixture.overlays.fullscreenChrome.controlsShown);
            Fixture.setFullscreen(false);
            verify(!Fixture.overlays.fullscreenChrome.controlsActive);
            tryVerify(() => loader(controlsLoader).item === null);
            verify(controls !== null);
        }
    }
}
