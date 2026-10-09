import QtQuick
import QtQuick.Templates as T
import QtTest
import qimgv.ui
import qimgv.tests

// The viewer's context menu over the Fixture's ContextMenuModel: it opens in
// its own window (larger than the small test window), shows the shortcuts of
// the action system, runs the rows' actions, expands "More" in place and
// lists the scripts with "Configure menu".
Item {
    id: root

    // Smaller than the menu, which must not be confined to it.
    width: 300
    height: 200

    ViewerContextMenu {
        id: menu

        menuModel: Fixture.contextMenu
    }

    TestCase {
        id: testCase

        // Items before the instantiated rows: the button rows and their
        // separator (ViewerContextMenu.fixedItemCount).
        readonly property int fixedItems: 2
        // Rows of ContextMenuModel.items.
        readonly property int panoramaRow: 1
        readonly property int copyRow: 5
        readonly property int expanderRow: 12
        readonly property int submenuRow: 13
        readonly property int renameRow: 14
        readonly property int rowCount: 20
        readonly property int timeout: 3000

        name: "ContextMenu"
        when: windowShown

        function row(index) {
            return menu.itemAt(fixedItems + index);
        }

        function openMenu(scripts) {
            Fixture.toggleContextMenu(scripts, false);
            tryVerify(() => menu.opened, timeout, "the menu did not open");
            // The menu window is exposed with its first frame.
            waitForRendering(menu.contentItem);
        }

        function closeMenu() {
            if (Fixture.contextMenu.open)
                Fixture.toggleContextMenu([], false);
            tryVerify(() => !menu.visible, timeout);
        }

        function init() {
            Fixture.showTestImage(64, 64);
            Fixture.clearInputLog();
        }

        function cleanup() {
            closeMenu();
        }

        function test_opensInItsOwnWindowBeyondTheWindowEdges() {
            openMenu([]);
            compare(menu.popupType, T.Popup.Window);
            compare(menu.count, fixedItems + rowCount);
            verify(menu.height > root.height,
                   "the menu (" + menu.height + ") is confined to the window (" + root.height + ")");
        }

        function test_shownShortcutsAreTheActionShortcuts() {
            Fixture.setShortcut("copyFile", "C");
            Fixture.setShortcut("removeFile", "Shift+Del");
            openMenu([]);
            let checked = 0;
            for (let i = 0; i < rowCount; ++i) {
                const item = row(i);
                if (item.entryAction === undefined || item.entryAction === "")
                    continue;
                compare(item.shortcutText, Actions.shortcutFor(item.entryAction), item.entryAction);
                ++checked;
            }
            verify(checked > 10);
            compare(row(copyRow).shortcutText, "C");

            // Edits reach the open menu.
            Fixture.setShortcut("copyFile", "Ctrl+Shift+C");
            compare(row(copyRow).shortcutText, "Ctrl+Shift+C");
        }

        function test_rowRunsItsActionAndCloses() {
            // The fake dispatcher runs the actions it has a shortcut for.
            Fixture.setShortcut("copyFile", "C");
            openMenu([]);
            const copy = row(copyRow);
            compare(copy.text, "Quick copy");
            verify(copy.enabled);
            mouseClick(copy);
            compare(Fixture.lastInvoked, "copyFile");
            tryVerify(() => !menu.visible, timeout);
            verify(!Fixture.contextMenu.open);
        }

        function test_buttonsActOnPress() {
            Fixture.setShortcut("fitWindow", "1");
            openMenu([]);
            const rows = menu.itemAt(0);
            const zoomRow = rows.children[0];
            let fitWindow = null;
            for (let i = 0; i < zoomRow.children.length; ++i) {
                if (zoomRow.children[i].entryAction === "fitWindow")
                    fitWindow = zoomRow.children[i];
            }
            verify(fitWindow !== null, "no fitWindow button");
            tryVerify(() => fitWindow.Window.window !== null, timeout, "the button is in no window");
            mousePress(fitWindow);
            compare(Fixture.lastInvoked, "fitWindow");
            tryVerify(() => !menu.visible, timeout);
            // The menu window is gone; the release ends over the window.
            mouseRelease(root);
        }

        function test_moreExpandsInPlace() {
            openMenu([]);
            const rename = row(renameRow);
            verify(!rename.visible);
            compare(rename.height, 0);
            const heightBefore = menu.height;
            mousePress(row(expanderRow));
            mouseRelease(row(expanderRow));
            tryVerify(() => rename.visible && rename.height > 0, timeout);
            verify(menu.opened, "More closed the menu");
            tryVerify(() => menu.height > heightBefore, timeout);
            compare(menu.width, menu.implicitWidth);

            // Collapsed again when the menu opens next time.
            closeMenu();
            openMenu([]);
            verify(!row(renameRow).visible);
        }

        function test_imageRowsAreDisabledWithoutAnImage() {
            Fixture.closeTestImage();
            openMenu([]);
            verify(!row(copyRow).enabled);
            verify(!row(panoramaRow).enabled);
        }

        function test_scriptsSubmenu() {
            openMenu(["Edit", "Upload"]);
            mousePress(row(expanderRow));
            mouseRelease(row(expanderRow));
            const scriptsRow = row(submenuRow);
            tryVerify(() => scriptsRow.visible, timeout);
            // The menu window grows to the expanded rows.
            tryVerify(() => scriptsRow.mapToItem(null, 0, scriptsRow.height).y <= scriptsRow.Window.height,
                      timeout, "the expanded rows lie outside the menu window");
            verify(scriptsRow.subMenu !== null);
            const scripts = scriptsRow.subMenu;
            compare(scripts.title, "Open with...");
            // Two scripts, the separator, "Configure menu".
            compare(scripts.count, 4);
            compare(scripts.itemAt(1).text, "Upload");
            // A click on its row opens it as a cascade (the offscreen platform
            // delivers no hover).
            mouseClick(scriptsRow);
            tryVerify(() => scripts.opened, timeout, "the scripts submenu did not open");
            waitForRendering(scripts.contentItem);
            mouseClick(scripts.itemAt(3));
            compare(Fixture.scriptSettingsRequests, 1);
            tryVerify(() => !menu.visible, timeout);
        }

        // Saved next to the test executable for review.
        function test_rendersForReview() {
            Fixture.setShortcut("copyFile", "C");
            openMenu(["Edit"]);
            mousePress(row(expanderRow));
            mouseRelease(row(expanderRow));
            tryVerify(() => row(renameRow).mapToItem(null, 0, row(renameRow).height).y
                            <= row(renameRow).Window.height, timeout);
            waitForRendering(menu.contentItem);
            grabImage(menu.contentItem).save(Fixture.artifactPath("contextmenu.png"));
        }

        function test_escapeAndTheFolderViewClose() {
            openMenu([]);
            keyClick(Qt.Key_Escape);
            tryVerify(() => !menu.visible, timeout);
            verify(!Fixture.contextMenu.open);

            openMenu([]);
            Fixture.setFolderViewActive(true);
            tryVerify(() => !menu.visible, timeout);
            Fixture.toggleContextMenu([], false);
            verify(!Fixture.contextMenu.open);
            Fixture.setFolderViewActive(false);
        }
    }
}
