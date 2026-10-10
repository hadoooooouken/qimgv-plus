import QtQuick
import QtQuick.Controls
import QtTest
import qimgv.tests

// The batch converter and print windows of the dialog port (DialogLayer):
// created on their first request, driven through their view-models, closed
// with them. The batch converter converts through the fixture's fake
// service; printing is never started on a real printer.
TestCase {
    id: testCase

    name: "BatchPrint"
    when: windowShown

    // Positions of the dialogs in DialogLayer.
    readonly property int batchIndex: 4
    readonly property int printIndex: 5
    readonly property int activationTimeoutMs: 5000
    readonly property int colorSliderCount: 7

    // BatchQueueModel.ItemState values.
    readonly property int processingState: 1
    readonly property int doneState: 2

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

    function findNamed(item, objectName) {
        if (item.objectName === objectName)
            return item;
        const children = item.children ?? [];
        for (let i = 0; i < children.length; ++i) {
            const found = findNamed(children[i], objectName);
            if (found)
                return found;
        }
        return null;
    }

    function findAll(item, typeName, found) {
        if (item.toString().startsWith(typeName) && item.visible)
            found.push(item);
        for (let i = 0; i < item.children.length; ++i)
            findAll(item.children[i], typeName, found);
        return found;
    }

    // A window declared inside another window is in its `data`.
    function childWindow(window, objectName) {
        for (let i = 0; i < window.data.length; ++i) {
            if (window.data[i].objectName === objectName)
                return window.data[i];
        }
        return null;
    }

    function openedWindow(index) {
        const loader = dialogLayer.children[index];
        tryVerify(() => loader.item !== null && loader.item.visible);
        const window = loader.item;
        tryVerify(() => window.active, activationTimeoutMs, "the dialog window was not activated");
        tryVerify(() => window.activeFocusItem !== null, activationTimeoutMs, "nothing in the dialog has the focus");
        return window;
    }

    // Sources next to the test executable, so the initial output folder
    // exists.
    function sources() {
        return [Fixture.artifactPath("a.png"), Fixture.artifactPath("b.png"),
                Fixture.artifactPath("c.png")];
    }

    function test_batchWindowShowsTheQueue() {
        verify(Fixture.requestBatchConversion(sources()));
        const window = openedWindow(batchIndex);
        const queue = findNamed(window.contentItem, "batchQueue");
        verify(queue !== null);
        compare(queue.count, 3);
        tryVerify(() => queue.itemAtIndex(1) !== null);
        compare(queue.itemAtIndex(1).name, "b.png");
        compare(queue.itemAtIndex(1).stateText, "Pending");
        compare(findNamed(window.contentItem, "batchStatus").text, "Ready to convert.");
        // The colour sliders are collapsed until the section is enabled.
        compare(findAll(window.contentItem, "LinkedSliderSpin", []).length, 0);
        Fixture.dialogs.batchConverter.setColorEnabled(true);
        tryCompare(findAll(window.contentItem, "LinkedSliderSpin", []), "length", colorSliderCount);
        Fixture.sendKey(window, Qt.Key_Escape);
        tryCompare(window, "visible", false);
        compare(Fixture.lastDialogAnswer, "batch:0");
    }

    function test_batchWarnsWithoutSelectedFiles() {
        const startsBefore = Fixture.batchStartCount();
        verify(Fixture.requestBatchConversion(sources()));
        const window = openedWindow(batchIndex);
        Fixture.dialogs.batchConverter.queue.setAllChecked(false);
        findNamed(window.contentItem, "convertButton").clicked();
        const message = childWindow(window, "batchMessage");
        verify(message !== null);
        tryCompare(message, "visible", true);
        compare(message.title, "No files");
        compare(Fixture.batchStartCount(), startsBefore);
        tryVerify(() => message.active, activationTimeoutMs);
        tryVerify(() => message.activeFocusItem !== null, activationTimeoutMs);
        Fixture.sendKey(message, Qt.Key_Return);
        tryCompare(message, "visible", false);
        verify(window.visible);
    }

    function test_batchRunsStopsAndFinishes() {
        const startsBefore = Fixture.batchStartCount();
        const cancelsBefore = Fixture.batchCancelCount();
        verify(Fixture.requestBatchConversion(sources()));
        const window = openedWindow(batchIndex);
        const convertButton = findNamed(window.contentItem, "convertButton");
        const cancelButton = findNamed(window.contentItem, "cancelButton");
        const status = findNamed(window.contentItem, "batchStatus");
        compare(cancelButton.text, "Cancel");

        convertButton.clicked();
        compare(Fixture.batchStartCount(), startsBefore + 1);
        compare(status.text, "Processing...");
        compare(cancelButton.text, "Stop");
        verify(!convertButton.enabled);

        Fixture.reportBatchProgress(0, processingState, "");
        Fixture.reportBatchProgress(0, doneState, "JPG • 40x20");
        const queue = findNamed(window.contentItem, "batchQueue");
        compare(queue.itemAtIndex(0).stateText, "Done");
        compare(queue.itemAtIndex(0).details, "JPG • 40x20");
        compare(status.text, "Processed 1 / 3 files.");

        cancelButton.clicked();
        compare(Fixture.batchCancelCount(), cancelsBefore + 1);
        compare(cancelButton.text, "Stopping...");
        verify(!cancelButton.enabled);
        Fixture.cancelBatch(1, 0, 3);
        compare(cancelButton.text, "Cancel");
        compare(status.text, "Stopped by user. Success: 1, Failed: 0");
        verify(convertButton.enabled);

        convertButton.clicked();
        Fixture.finishBatch(3, 0, 3);
        const message = childWindow(window, "batchMessage");
        tryCompare(message, "visible", true);
        compare(message.title, "Batch Conversion Complete");
        Fixture.dialogs.batchConverter.dismissMessage();
        tryCompare(message, "visible", false);

        cancelButton.clicked();
        tryCompare(window, "visible", false);
        compare(Fixture.lastDialogAnswer, "batch:1");
    }

    function test_printWithoutPrintersOffersOnlyTheExport() {
        verify(Fixture.requestPrint([]));
        const window = openedWindow(printIndex);
        compare(window.title, "Print image");
        verify(!findNamed(window.contentItem, "printButton").enabled);
        compare(window.activeFocusItem, findNamed(window.contentItem, "exportPdfButton"));
        const preview = findNamed(window.contentItem, "printPreview");
        tryVerify(() => preview.paintedRect.height > 0);
        // A4 portrait fills the preview's height.
        compare(preview.paintedRect.height, Fixture.dialogs.print.previewExtent);
        verify(preview.paintedRect.width < preview.paintedRect.height);
        Fixture.dialogs.print.setLandscape(true);
        tryVerify(() => preview.paintedRect.width > preview.paintedRect.height);
        Fixture.sendKey(window, Qt.Key_Escape);
        tryCompare(window, "visible", false);
        compare(Fixture.lastDialogAnswer, "print:0");
    }

    function test_printWithPrintersOffersPrinting() {
        verify(Fixture.requestPrint(["First", "Second"]));
        const window = openedWindow(printIndex);
        const printButton = findNamed(window.contentItem, "printButton");
        verify(printButton.enabled);
        compare(window.activeFocusItem, printButton);
        compare(Fixture.dialogs.print.printerIndex, 0);
        window.close();
        tryCompare(window, "visible", false);
        compare(Fixture.lastDialogAnswer, "print:0");
    }
}
