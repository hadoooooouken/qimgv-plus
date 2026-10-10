import QtQuick
import QtTest
import qimgv.bridges
import qimgv.ui
import qimgv.tests

// The folder view over the Fixture's FolderViewController, FolderGridController
// and their ThumbnailListModel: a large directory keeps the delegates and the
// requests to the visible rows, the pointer selects, activates, draws the
// rubber band and asks for the context menu, keys move and type ahead, drops
// keep their target, the name filter is published, the top bar compacts, and
// the places panel shows the bookmarks. The view is a sibling of the
// TestCase, which is not visible and would not get input.
Item {
    id: root

    width: 1200
    height: 800

    FolderView {
        id: view

        anchors.fill: parent
        controller: Fixture.folderView
        focus: true
    }

    TestCase {
        id: testCase

        readonly property int createTimeout: 3000
        readonly property int largeDirectory: 10000
        readonly property int dirCount: 3
        // More than a screen of cells plus the reuse pool, far fewer than the
        // directory.
        readonly property int delegateLimit: 200
        readonly property int compactWidth: 500
        readonly property int nameFilterTimeout: 1000
        readonly property url droppedFile: "file:///C:/images/dropped.png"
        readonly property int copyAction: 1

        name: "FolderView"
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
            if (item.text === text)
                return item;
            for (let i = 0; i < item.children.length; ++i) {
                const found = findText(item.children[i], text);
                if (found)
                    return found;
            }
            return null;
        }

        function grid() {
            return findType(view, "QQuickGridView");
        }

        function cellOf(index) {
            const children = grid().contentItem.children;
            for (let i = 0; i < children.length; ++i) {
                const child = children[i];
                if (child.toString().startsWith("ThumbnailWidget") && child.visible
                        && child.index === index)
                    return child;
            }
            return null;
        }

        function centreOf(index) {
            const cell = cellOf(index);
            verify(cell !== null, "no cell for item " + index);
            return cell.mapToItem(root, cell.width / 2, cell.height / 2);
        }

        function initTestCase() {
            Fixture.setShortcut("exit", "");
            Fixture.setFolderViewActive(true);
        }

        function init() {
            root.width = 1200;
            Fixture.populateFolder(40, dirCount, "C:/");
            Fixture.deliverFolderThumbnails();
            tryVerify(() => cellOf(0) !== null, createTimeout, "the grid shows no cells");
        }

        function test_compactTopBarHidesTheGridSize() {
            const label = findText(view, qsTranslate("FolderView", "Grid size"));
            verify(label !== null);
            verify(label.visible);
            root.width = compactWidth;
            tryVerify(() => !label.visible);
        }

        function test_doubleClickActivates() {
            const point = centreOf(5);
            mouseClick(root, point.x, point.y);
            compare(Fixture.folderView.grid.model.currentIndex, 5);
            compare(Fixture.lastFolderRequest, "");
            mouseDoubleClickSequence(root, point.x, point.y);
            compare(Fixture.lastFolderRequest, "activated:5");
        }

        function test_dropKeepsItsTarget() {
            const point = centreOf(4);
            verify(Fixture.dropExternalFile(root.Window.window, point, droppedFile));
            compare(Fixture.lastFolderRequest, "drop:4:" + copyAction);
        }

        function test_keysMoveAndTypeAhead() {
            const point = centreOf(2);
            mouseClick(root, point.x, point.y);
            keyClick(Qt.Key_Right);
            compare(Fixture.folderView.grid.model.currentIndex, 3);
            keyClick(Qt.Key_X);
            compare(Fixture.lastFolderRequest, "typeAhead:x");
        }

        function test_largeDirectoryKeepsTheDelegatesBounded() {
            Fixture.populateFolder(largeDirectory, dirCount, "C:/");
            tryVerify(() => Fixture.folderThumbnailRequestCount > 0, createTimeout);
            const delivered = Fixture.deliverFolderThumbnails();
            verify(delivered > 0 && delivered < largeDirectory / 10,
                   "requested " + delivered + " thumbnails");
            verify(grid().contentItem.children.length < delegateLimit);
            tryVerify(() => cellOf(0) !== null && cellOf(0).loaded, createTimeout);
            // Scrolling to the end requests the last rows.
            const point = centreOf(0);
            mouseClick(root, point.x, point.y);
            keyClick(Qt.Key_End);
            tryVerify(() => cellOf(largeDirectory - 1) !== null, createTimeout);
            verify(grid().contentItem.children.length < delegateLimit);
        }

        function test_nameFilterIsPublished() {
            const field = findType(view, "SearchField");
            verify(field !== null);
            mouseClick(field);
            keyClick(Qt.Key_A);
            tryCompare(Fixture, "lastFolderRequest", "nameFilter:a", nameFilterTimeout);
            // The field follows the controller.
            Fixture.folderView.editNameFilter("");
            compare(field.text, "");
        }

        function test_placesPanelShowsTheBookmarks() {
            const places = findType(view, "PlacesPanel");
            verify(places !== null);
            verify(!places.visible);
            Fixture.folderView.setPlacesPanelEnabled(true);
            tryVerify(() => places.visible);
            const list = findType(places, "QQuickListView");
            verify(list !== null);
            compare(list.count, Fixture.folderView.bookmarks.items.rowCount());
            verify(list.count > 0);
            // The grid keeps the remaining width.
            verify(grid().x > places.width);
            Fixture.folderView.setPlacesPanelEnabled(false);
            tryVerify(() => !places.visible);
        }

        function test_rightClickAsksForTheMenu() {
            const point = centreOf(6);
            mouseClick(root, point.x, point.y, Qt.RightButton);
            compare(Fixture.lastFolderRequest, "contextMenu");
            compare(Fixture.folderView.grid.model.currentIndex, 6);
            const gridView = findType(view, "FolderGridView");
            verify(gridView !== null);
            tryVerify(() => gridView.contextMenuCreated);
        }

        function test_rubberBandSelectsTheTouchedCells() {
            const first = cellOf(0);
            const topLeft = first.mapToItem(root, 0, 0);
            const second = centreOf(1);
            // From the left margin, beside the first cell, into the second one.
            mousePress(root, topLeft.x - 4, topLeft.y + 4);
            mouseMove(root, second.x, second.y, -1, Qt.LeftButton);
            const band = Fixture.folderView.grid.rubberBand;
            verify(band.width > 0 && band.height > 0);
            mouseRelease(root, second.x, second.y);
            compare(Fixture.folderView.grid.model.currentIndex, 1);
            verify(Fixture.folderView.grid.rubberBand.width === 0);
        }
    }
}
