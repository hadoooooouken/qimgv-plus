import QtQuick
import QtTest
import qimgv.bridges
import qimgv.ui
import qimgv.tests

// The thumbnail panel over the Fixture's ThumbnailPanelController and
// ThumbnailListModel: the content waits for the creation permission, a
// pinned panel takes its place at each side, a large directory keeps the
// delegates and the requests to the visible range, delivered thumbnails are
// drawn, pointer input reaches the model, and a floating panel slides in.
// The panel is a sibling of the TestCase, which is not visible and would not
// get input.
Item {
    id: root

    width: 1200
    height: 800

    MainPanel {
        id: panel

        controller: Fixture.thumbnailPanel
    }

    TestCase {
        id: testCase

        readonly property int createTimeout: 3000
        readonly property int largeDirectory: 10000
        // More than a screen of cells plus the reuse pool, far fewer than
        // the directory.
        readonly property int delegateLimit: 60
        // The preloaded range of kStripPreloadDistance on both sides, in cells
        // of the test layout (cell width 80 + 2 * (9 + 2) = 102 pixels).
        readonly property int requestLimit: 80

        name: "ThumbnailStrip"
        when: windowShown

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

        // The strip's ListView once the asynchronous content exists.
        function list() {
            tryVerify(() => findType(panel, "QQuickListView") !== null, createTimeout,
                      "the strip was not created");
            return findType(panel, "QQuickListView");
        }

        function cells(view) {
            const result = [];
            const children = view.contentItem.children;
            for (let i = 0; i < children.length; ++i) {
                if (children[i].toString().startsWith("ThumbnailWidget") && children[i].visible)
                    result.push(children[i]);
            }
            return result;
        }

        function cellOf(view, index) {
            const all = cells(view);
            for (let i = 0; i < all.length; ++i) {
                if (all[i].index === index)
                    return all[i];
            }
            return null;
        }

        function pinnedAt(position) {
            Fixture.configureThumbnailPanel(true, position, false);
            Fixture.setPanelWindowSize(root.width, root.height);
        }

        function initTestCase() {
            Fixture.setShortcut("folderView", "");
            Fixture.setShortcut("openSettings", "");
        }

        function cleanup() {
            Fixture.configureThumbnailPanel(true, SettingsEnums.PanelPosition.Bottom, false);
        }

        // Runs first (alphabetical order): creation was not allowed yet.
        function test_aaContentWaitsForTheFirstDocument() {
            pinnedAt(SettingsEnums.PanelPosition.Bottom);
            Fixture.populateThumbnails(20);
            verify(panel.visible);
            verify(panel.controller.docked);
            compare(panel.dockedBottom, panel.controller.layout.panelExtent);
            compare(findType(panel, "QQuickListView"), null);
            compare(Fixture.thumbnailRequestCount, 0);

            Fixture.allowThumbnailPanelCreation();
            list();
            tryVerify(() => Fixture.thumbnailRequestCount > 0, createTimeout);
            compare(Fixture.requestedThumbnailCount, 20);
        }

        function test_pinnedPanelTakesItsSide_data() {
            return [
                { tag: "top", position: SettingsEnums.PanelPosition.Top, horizontal: true },
                { tag: "bottom", position: SettingsEnums.PanelPosition.Bottom, horizontal: true },
                { tag: "left", position: SettingsEnums.PanelPosition.Left, horizontal: false },
                { tag: "right", position: SettingsEnums.PanelPosition.Right, horizontal: false }
            ];
        }

        function test_pinnedPanelTakesItsSide(data) {
            pinnedAt(data.position);
            const extent = panel.controller.layout.panelExtent;
            const view = list();
            compare(view.orientation, data.horizontal ? ListView.Horizontal : ListView.Vertical);
            if (data.horizontal) {
                compare(panel.width, root.width);
                compare(panel.height, extent);
                compare(panel.y, data.position === SettingsEnums.PanelPosition.Top ? 0
                                                                                 : root.height - extent);
            } else {
                compare(panel.height, root.height);
                compare(panel.width, extent);
                compare(panel.x, data.position === SettingsEnums.PanelPosition.Left ? 0
                                                                                  : root.width - extent);
            }
            const docked = panel.dockedTop + panel.dockedBottom + panel.dockedLeft + panel.dockedRight;
            compare(docked, extent);
        }

        function test_largeDirectoryKeepsToTheVisibleRange() {
            pinnedAt(SettingsEnums.PanelPosition.Bottom);
            const view = list();
            Fixture.populateThumbnails(largeDirectory);
            const atStart = Fixture.requestedThumbnailCount;
            verify(atStart > 0 && atStart <= requestLimit, "requested " + atStart);
            Fixture.selectThumbnail(largeDirectory / 2);
            const atSelection = Fixture.requestedThumbnailCount - atStart;
            verify(atSelection > 0 && atSelection <= requestLimit, "requested " + atSelection);
            verify(cells(view).length <= delegateLimit, "delegates " + cells(view).length);
            verify(view.contentX > 0, "the selection was not brought into view");
            Fixture.deliverRequestedThumbnails();

            // Scrolling with the wheel moves by items and requests only what
            // came into range.
            const before = Fixture.requestedThumbnailCount;
            const start = view.contentX;
            for (let i = 0; i < 20; ++i)
                mouseWheel(view, view.width / 2, view.height / 2, 0, -120);
            verify(view.contentX > start);
            verify(Fixture.requestedThumbnailCount - before <= requestLimit);
            verify(cells(view).length <= delegateLimit, "delegates " + cells(view).length);
        }

        function test_deliveredThumbnailsAreDrawn() {
            pinnedAt(SettingsEnums.PanelPosition.Bottom);
            const view = list();
            Fixture.populateThumbnails(30);
            Fixture.selectThumbnail(0);
            tryVerify(() => Fixture.thumbnailRequestCount > 0, createTimeout);
            verify(Fixture.deliverRequestedThumbnails() > 0);
            const cell = cellOf(view, 0);
            verify(cell !== null);
            verify(cell.loaded);
            verify(cell.selected);
            const image = findType(cell, "ThumbnailItem");
            verify(image.visible);
            verify(image.paintedRect.width > 0 && image.paintedRect.height > 0);
            // The 4:3 thumbnail fills the 4:3 image area.
            compare(image.paintedRect.width, panel.controller.layout.imageArea.width);
        }

        function test_extendedStyleShowsTheLabels() {
            Fixture.configureThumbnailPanel(true, SettingsEnums.PanelPosition.Left, true);
            Fixture.setPanelWindowSize(root.width, root.height);
            list();
            Fixture.populateThumbnails(5);
            Fixture.selectThumbnail(1);
            tryVerify(() => Fixture.thumbnailRequestCount > 0, createTimeout);
            Fixture.deliverRequestedThumbnails();
            tryVerify(() => findText(panel, "item 1") !== null, createTimeout);
            verify(findText(panel, "64 x 48") !== null);
        }

        function test_pressActivatesTheItemUnderThePointer() {
            pinnedAt(SettingsEnums.PanelPosition.Bottom);
            const view = list();
            Fixture.populateThumbnails(10);
            Fixture.selectThumbnail(0);
            tryVerify(() => cellOf(view, 2) !== null, createTimeout);
            const cell = cellOf(view, 2);
            mousePress(cell, cell.width / 2, cell.height / 2);
            mouseRelease(cell, cell.width / 2, cell.height / 2);
            compare(Fixture.lastActivatedThumbnail, 2);
        }

        function test_pinButtonPublishesTheChange() {
            pinnedAt(SettingsEnums.PanelPosition.Bottom);
            list();
            // The panel buttons have a glyph and the checked look (active).
            const buttons = [];
            const collect = item => {
                if (item.glyph !== undefined && item.active !== undefined && item.visible)
                    buttons.push(item);
                for (let i = 0; i < item.children.length; ++i)
                    collect(item.children[i]);
            };
            collect(panel);
            compare(buttons.length, 3);
            let pinButton = null;
            for (let i = 0; i < buttons.length; ++i) {
                if (buttons[i].active)
                    pinButton = buttons[i];
            }
            verify(pinButton !== null, "the pin button does not show the pinned state");
            mouseClick(pinButton);
            compare(Fixture.lastPinRequest, 0);
            verify(!panel.controller.pinned);
            verify(!pinButton.active);
        }

        function test_floatingPanelSlidesIn() {
            Fixture.configureThumbnailPanel(false, SettingsEnums.PanelPosition.Bottom, false);
            Fixture.setPanelWindowSize(root.width, root.height);
            // An unpinned panel stays until the pointer is away from it.
            Fixture.panelPointerMoved(Qt.point(root.width / 2, root.height / 2), Qt.NoButton);
            tryCompare(panel, "visible", false, createTimeout);
            compare(panel.dockedBottom, 0);
            Fixture.panelPointerMoved(Qt.point(root.width / 2, root.height - 2), Qt.NoButton);
            verify(panel.controller.shown);
            verify(panel.visible);
            tryCompare(panel, "slide", 0, createTimeout);
            compare(panel.dockedBottom, 0);
            // Away from the panel it slides out after the hide delay.
            Fixture.panelPointerMoved(Qt.point(root.width / 2, root.height / 2), Qt.NoButton);
            tryCompare(panel, "visible", false, createTimeout);
        }
    }
}
