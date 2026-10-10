import QtQuick
import QtQuick.Controls
import QtTest
import qimgv.tests

// The modal dialog windows of the dialog port (DialogLayer): each opens with
// its request, answers through its view-model and closes with it.
TestCase {
    id: testCase

    name: "Dialogs"
    when: windowShown

    // Positions of the dialogs in DialogLayer.
    readonly property int confirmationIndex: 0
    readonly property int fileReplaceIndex: 1
    readonly property int resizeIndex: 2
    readonly property int textInputIndex: 3

    // FileReplaceMode values.
    readonly property int fileToFile: 0
    readonly property int dirToDir: 1

    // Row of "1920 x 1080 (FullHD)" in ResizeDialogModel.commonSizes.
    readonly property int fullHdRow: 5
    readonly property int activationTimeoutMs: 5000

    // ScalingFilter value of Magic Kernel Sharp 2021 (QI_FILTER_MKS2021).
    readonly property int magicKernelFilter: 5

    property var dialogLayer: null

    function initTestCase() {
        const component = Qt.createComponent("qimgv.ui", "DialogLayer");
        compare(component.status, Component.Ready, component.errorString());
        dialogLayer = component.createObject(testCase, {coordinator: Fixture.dialogs});
        verify(dialogLayer !== null, "qimgv.ui/DialogLayer could not be instantiated");
    }

    function cleanupTestCase() {
        dialogLayer.destroy();
    }

    function init() {
        Fixture.clearDialogAnswer();
    }

    // A failed test must not leave a modal window over the next one.
    function cleanup() {
        Fixture.abandonDialogs();
    }

    function findType(item, typeName) {
        if (item.toString().startsWith(typeName))
            return item;
        for (let i = 0; i < item.children.length; ++i) {
            const found = findType(item.children[i], typeName);
            if (found)
                return found;
        }
        return null;
    }

    function findAll(item, typeName, found) {
        if (item.toString().startsWith(typeName))
            found.push(item);
        for (let i = 0; i < item.children.length; ++i)
            findAll(item.children[i], typeName, found);
        return found;
    }

    // The window of the dialog at index, once open and active.
    function openedWindow(index) {
        const loader = dialogLayer.children[index];
        tryVerify(() => loader.item !== null && loader.item.visible);
        const window = loader.item;
        tryVerify(() => window.active, activationTimeoutMs, "the dialog window was not activated");
        tryVerify(() => window.activeFocusItem !== null, activationTimeoutMs, "nothing in the dialog has the focus");
        return window;
    }

    function standardButton(window, button) {
        const box = findType(window.contentItem, "DialogButtonBox");
        verify(box !== null, "no DialogButtonBox");
        return box.standardButton(button);
    }

    function test_dialogsAreCreatedOnTheirFirstRequest() {
        compare(dialogLayer.children[resizeIndex].item, null);
        verify(Fixture.requestResize(Qt.size(100, 50), Qt.size(1920, 1080), [], false));
        const window = openedWindow(resizeIndex);
        verify(window !== null);
        Fixture.sendKey(window, Qt.Key_Escape);
        tryCompare(window, "visible", false);
        compare(Fixture.lastDialogAnswer, "resize:none");
    }

    function test_confirmationEnterAcceptsEscapeRejects() {
        verify(Fixture.requestConfirmation("Delete permanently", "Delete photo.png?"));
        let window = openedWindow(confirmationIndex);
        compare(window.title, "Delete permanently");
        verify(findType(window.contentItem, "Label").text.length > 0);
        // Yes is the default button.
        Fixture.sendKey(window, Qt.Key_Return);
        tryCompare(window, "visible", false);
        compare(Fixture.lastDialogAnswer, "confirm:1");

        verify(Fixture.requestConfirmation("Move to trash", "Move photo.png to trash?"));
        window = openedWindow(confirmationIndex);
        compare(window.title, "Move to trash");
        Fixture.sendKey(window, Qt.Key_Escape);
        tryCompare(window, "visible", false);
        compare(Fixture.lastDialogAnswer, "confirm:0");
    }

    function test_confirmationButtons() {
        verify(Fixture.requestConfirmation("File exists", "Overwrite file?"));
        const window = openedWindow(confirmationIndex);
        mouseClick(standardButton(window, DialogButtonBox.No));
        tryCompare(window, "visible", false);
        compare(Fixture.lastDialogAnswer, "confirm:0");

        verify(Fixture.requestConfirmation("File exists", "Overwrite file?"));
        mouseClick(standardButton(openedWindow(confirmationIndex), DialogButtonBox.Yes));
        tryCompare(window, "visible", false);
        compare(Fixture.lastDialogAnswer, "confirm:1");
    }

    function test_closingTheConfirmationWindowRejects() {
        verify(Fixture.requestConfirmation("File exists", "Overwrite file?"));
        const window = openedWindow(confirmationIndex);
        window.close();
        tryCompare(window, "visible", false);
        compare(Fixture.lastDialogAnswer, "confirm:0");
    }

    function test_fileReplaceSingleCollision() {
        verify(Fixture.requestFileReplace("C:/a/photo.png", "C:/b/photo.png", fileToFile, false));
        const window = openedWindow(fileReplaceIndex);
        compare(window.title, "File already exists");
        const applyAll = findType(window.contentItem, "CheckBox");
        verify(!applyAll.visible);
        mouseClick(standardButton(window, DialogButtonBox.No));
        tryCompare(window, "visible", false);
        compare(Fixture.lastDialogAnswer, "replace:0:0:0");
    }

    function test_fileReplaceApplyToAllAndCancel() {
        verify(Fixture.requestFileReplace("C:/a/dir", "C:/b/dir", dirToDir, true));
        let window = openedWindow(fileReplaceIndex);
        compare(window.title, "Directory already exists");
        const applyAll = findType(window.contentItem, "CheckBox");
        verify(applyAll.visible);
        verify(!applyAll.checked);
        mouseClick(applyAll);
        verify(applyAll.checked);
        mouseClick(standardButton(window, DialogButtonBox.Yes));
        tryCompare(window, "visible", false);
        compare(Fixture.lastDialogAnswer, "replace:1:1:0");

        // The next question starts with "Apply to all" cleared.
        verify(Fixture.requestFileReplace("C:/a/dir2", "C:/b/dir2", dirToDir, true));
        window = openedWindow(fileReplaceIndex);
        verify(!applyAll.checked);
        mouseClick(standardButton(window, DialogButtonBox.Cancel));
        tryCompare(window, "visible", false);
        compare(Fixture.lastDialogAnswer, "replace:0:0:1");
    }

    function test_fileReplaceEscapeSkipsTheItem() {
        verify(Fixture.requestFileReplace("C:/a/photo.png", "C:/b/photo.png", fileToFile, true));
        const window = openedWindow(fileReplaceIndex);
        Fixture.sendKey(window, Qt.Key_Escape);
        tryCompare(window, "visible", false);
        compare(Fixture.lastDialogAnswer, "replace:0:0:0");
    }

    function test_resizeTypedWidthIsCommittedByEnter() {
        verify(Fixture.requestResize(Qt.size(1000, 500), Qt.size(1920, 1080), [], false));
        const window = openedWindow(resizeIndex);
        const spinBoxes = findAll(window.contentItem, "SpinBox", []);
        compare(spinBoxes.length, 2);
        const widthBox = spinBoxes[0];
        compare(widthBox.value, 1000);
        mouseClick(widthBox);
        tryVerify(() => widthBox.activeFocus);
        widthBox.contentItem.selectAll();
        Fixture.sendText(window, "2000");
        Fixture.sendKey(window, Qt.Key_Return);
        tryCompare(window, "visible", false);
        compare(Fixture.lastDialogAnswer, "resize:2000x1000:" + magicKernelFilter + ":0:");
    }

    function test_resizeCommonSizeAndUpscaylModel() {
        verify(Fixture.requestResize(Qt.size(1000, 500), Qt.size(1920, 1080), ["modelA", "modelB"], true));
        const window = openedWindow(resizeIndex);
        const dialog = Fixture.dialogs.resize;
        // The aspect ratio is kept.
        dialog.selectCommonSize(fullHdRow);
        compare(dialog.targetWidth, 1920);
        compare(dialog.targetHeight, 960);
        verify(dialog.upscaylApplies);
        dialog.setUpscaylModelIndex(1);
        mouseClick(standardButton(window, DialogButtonBox.Ok));
        tryCompare(window, "visible", false);
        compare(Fixture.lastDialogAnswer, "resize:1920x960:" + magicKernelFilter + ":1:modelB");
    }

    function test_textInputStartsSelectedAndReturnsTheText() {
        verify(Fixture.requestText("Add folder", "Folder name:", "New folder"));
        let window = openedWindow(textInputIndex);
        compare(window.title, "Add folder");
        const field = findType(window.contentItem, "TextField");
        compare(field.text, "New folder");
        compare(field.selectedText, "New folder");
        tryVerify(() => field.activeFocus);
        Fixture.sendText(window, "photos");
        Fixture.sendKey(window, Qt.Key_Return);
        tryCompare(window, "visible", false);
        compare(Fixture.lastDialogAnswer, "text:1:photos");

        verify(Fixture.requestText("Add folder", "Folder name:", ""));
        window = openedWindow(textInputIndex);
        compare(field.text, "");
        mouseClick(standardButton(window, DialogButtonBox.Cancel));
        tryCompare(window, "visible", false);
        compare(Fixture.lastDialogAnswer, "text:0:");
    }

    function test_aSecondRequestWhileOpenIsDeclined() {
        verify(Fixture.requestConfirmation("First", "First question"));
        const window = openedWindow(confirmationIndex);
        ignoreWarning(/the confirmation dialog is already open/);
        verify(!Fixture.requestConfirmation("Second", "Second question"));
        compare(window.title, "First");
        Fixture.sendKey(window, Qt.Key_Escape);
        tryCompare(window, "visible", false);
    }
}
