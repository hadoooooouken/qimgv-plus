pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.impl
import QtQuick.Dialogs
import QtQuick.Layouts
import QtQuick.Templates as T
import QtQml.Models
import qimgv.bridges
import qimgv.style

// The places panel of the folder view (FolderView's placesPanel in the widget
// UI): the bookmarks (BookmarksWidget) and the folder tree (TreeViewCustom),
// each under a header that collapses it. Bookmarks open on click, show their
// move and remove buttons under the pointer, reorder by dragging and take
// dropped folders (bookmarked) and files (copied or moved into them). The
// tree follows the directory of the grid, opens a folder on click or Enter
// and takes dropped files. Its strings keep the widget view's translation
// context.
Rectangle {
    id: root

    required property FolderViewController controller

    readonly property BookmarksModel bookmarks: controller.bookmarks

    // FolderView::setupUi() and stylesheet sizes.
    readonly property int topMargin: 7
    readonly property int sectionSpacing: 6
    readonly property int sectionGap: 10
    readonly property int headerLeftMargin: 8
    readonly property int headerRightMargin: 5
    readonly property int headerButtonSize: 26
    readonly property int headerSpacing: 16
    readonly property int panelRightMargin: 6
    readonly property int rightBorderWidth: 1
    readonly property int buttonRadius: 4
    // BookmarksItem layout margins and the translucent text_hc2 shades of its
    // buttons.
    readonly property int bookmarkMarginH: 10
    readonly property int bookmarkMarginV: 6
    readonly property int bookmarkSpacing: 6
    readonly property real bookmarkButtonHoverOpacity: 0.14
    readonly property real bookmarkButtonPressedOpacity: 0.26
    readonly property int dropIndicatorHeight: 2
    // QTreeView rows: 24 px plus 2 px padding, 20 px indentation.
    readonly property int treeRowHeight: 28
    readonly property int treeIndentation: 20
    readonly property int treeBranchPadding: 6
    readonly property int treeTextSpacing: 4
    readonly property real collapsedRotation: -StyleConstants.quarterTurn

    color: Theme.colors.folderView

    Rectangle {
        anchors.right: parent.right
        anchors.rightMargin: root.panelRightMargin
        width: root.rightBorderWidth
        height: parent.height
        color: Theme.colors.folderViewHc
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: root.topMargin
        anchors.rightMargin: root.panelRightMargin + root.rightBorderWidth
        spacing: root.sectionSpacing

        SectionHeader {
            text: qsTranslate("FolderView", "Bookmarks")
            buttonGlyph: FluentIcons.BookmarkAdd20
            acceptsFolders: true
            onHeaderClicked: root.controller.toggleBookmarks()
            onButtonClicked: folderDialog.open()
        }

        ListView {
            id: bookmarkList

            // Row the dragged bookmark would go to, -1 while none is dragged.
            property int dropRow: -1

            Layout.fillWidth: true
            Layout.preferredHeight: contentHeight
            visible: root.controller.bookmarksExpanded
            interactive: false
            model: root.bookmarks.items

            delegate: BookmarkRow {}

            // Folders dropped between the rows are bookmarked too.
            DropArea {
                anchors.fill: parent
                z: -1
                onEntered: drag => drag.accepted = drag.hasUrls
                onDropped: drop => {
                    if (root.controller.dropFoldersOnBookmarks(drop.urls))
                        drop.acceptProposedAction();
                }
            }

            // Row in front of which a bookmark dropped at y goes.
            function dropRowAt(y: real): int {
                for (let target = 0; target < count; ++target) {
                    const item = itemAtIndex(target);
                    if (item && y < item.y + item.height / 2)
                        return target;
                }
                return count;
            }

            function dropIndicatorY(): real {
                const item = dropRow < count ? itemAtIndex(dropRow) : null;
                return item ? item.y : contentHeight - root.dropIndicatorHeight;
            }

            Rectangle {
                visible: bookmarkList.dropRow >= 0
                width: parent.width
                height: root.dropIndicatorHeight
                y: bookmarkList.dropRow >= 0 ? bookmarkList.dropIndicatorY() : 0
                color: Theme.colors.accent
            }
        }

        Item {
            Layout.preferredHeight: root.sectionGap - root.sectionSpacing
        }

        SectionHeader {
            text: qsTranslate("FolderView", "Filesystem")
            buttonGlyph: FluentIcons.Home20
            buttonToolTip: qsTranslate("FolderView", "Home")
            onHeaderClicked: root.controller.toggleTree()
            onButtonClicked: root.controller.goHome()
        }

        TreeView {
            id: tree

            function followCurrentFolder() {
                if (!root.controller.hasCurrentFolder)
                    return;
                const folder = root.controller.currentFolder;
                expandToIndex(folder);
                selectionModel.setCurrentIndex(folder, ItemSelectionModel.ClearAndSelect);
            }

            function scrollToCurrentFolder() {
                if (!root.controller.hasCurrentFolder)
                    return;
                const row = rowAtIndex(root.controller.currentFolder);
                if (row >= 0)
                    positionViewAtRow(row, TableView.AlignVCenter);
            }

            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.controller.treeExpanded
            clip: true
            model: root.controller.folderTree
            boundsBehavior: Flickable.StopAtBounds
            selectionModel: ItemSelectionModel {
                model: tree.model
                onModelChanged: {
                    tree.followCurrentFolder();
                    Qt.callLater(tree.scrollToCurrentFolder);
                }
            }
            // Only the name column (QFileSystemModel has size, type and date).
            columnWidthProvider: column => column === 0 ? tree.width : 0

            ScrollBar.vertical: ScrollBar {}

            Component.onCompleted: {
                followCurrentFolder();
                Qt.callLater(scrollToCurrentFolder);
            }
            // QFileSystemModel lists a folder once asked (QTreeView did so
            // when it expanded one).
            onExpanded: (row, depth) => root.controller.listFolder(index(row, 0))
            Keys.onPressed: event => {
                if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                    const current = tree.selectionModel.currentIndex;
                    if (tree.rowAtIndex(current) >= 0)
                        root.controller.openFolder(current);
                    event.accepted = true;
                }
            }

            Connections {
                target: root.controller

                function onCurrentFolderChanged() {
                    tree.followCurrentFolder();
                }

                function onCurrentFolderScrollRequested() {
                    tree.scrollToCurrentFolder();
                }
            }

            delegate: TreeRow {}
        }

        Item {
            Layout.fillHeight: true
            visible: !tree.visible
        }
    }

    FolderDialog {
        id: folderDialog

        title: qsTranslate("FolderView", "Select directory")
        currentFolder: root.controller.bookmarkDialogFolder
        onAccepted: root.controller.addBookmark(selectedFolder)
    }

    // PanelSectionHeader with its PlacesPanelButton.
    component SectionHeader: Item {
        id: header

        required property string text
        required property int buttonGlyph
        property string buttonToolTip
        // Folders dropped onto the header are bookmarked.
        property bool acceptsFolders: false

        signal headerClicked
        signal buttonClicked

        Layout.fillWidth: true
        Layout.leftMargin: root.headerLeftMargin
        Layout.rightMargin: root.headerRightMargin
        implicitHeight: headerRow.implicitHeight

        DropArea {
            anchors.fill: parent
            enabled: header.acceptsFolders
            onEntered: drag => drag.accepted = drag.hasUrls
            onDropped: drop => {
                if (root.controller.dropFoldersOnBookmarks(drop.urls))
                    drop.acceptProposedAction();
            }
        }

        RowLayout {
            id: headerRow

            anchors.fill: parent
            spacing: root.headerSpacing

            Text {
                Layout.fillWidth: true
                text: header.text
                color: headerArea.containsMouse ? Theme.colors.textHc2 : Theme.colors.textHc
                font: Theme.fonts.section

                MouseArea {
                    id: headerArea

                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: header.headerClicked()
                }
            }

            T.AbstractButton {
                id: headerButton

                Layout.preferredWidth: root.headerButtonSize
                Layout.preferredHeight: root.headerButtonSize
                focusPolicy: Qt.NoFocus
                hoverEnabled: true
                onClicked: header.buttonClicked()

                ToolTip.visible: hovered && header.buttonToolTip.length > 0
                ToolTip.text: header.buttonToolTip

                background: Rectangle {
                    radius: root.buttonRadius
                    color: headerButton.down ? Theme.colors.folderViewButtonPressed
                         : headerButton.hovered ? Theme.colors.folderViewButtonHover
                         : "transparent"
                }
                contentItem: IconGlyph {
                    icon: header.buttonGlyph
                }
            }
        }
    }

    // BookmarksItem: folder glyph, name, and the move and remove buttons
    // while the pointer is over the row.
    component BookmarkRow: Rectangle {
        id: row

        required property int index
        required property string name
        required property string path

        readonly property bool highlighted: path === root.bookmarks.currentPath
        readonly property bool showButtons: rowHover.hovered || rowArea.dragging

        width: ListView.view.width
        implicitHeight: content.implicitHeight + 2 * root.bookmarkMarginV
        color: highlighted ? Theme.colors.folderViewHc2
             : rowHover.hovered || dropArea.containsDrag ? Theme.colors.folderViewHc
             : "transparent"

        HoverHandler {
            id: rowHover
        }

        MouseArea {
            id: rowArea

            property bool dragging: false
            property point pressPosition

            anchors.fill: parent
            hoverEnabled: true
            onPressed: mouse => {
                pressPosition = Qt.point(mouse.x, mouse.y);
                dragging = false;
            }
            onPositionChanged: mouse => {
                if (!(mouse.buttons & Qt.LeftButton))
                    return;
                if (!dragging && Math.abs(mouse.x - pressPosition.x) + Math.abs(mouse.y - pressPosition.y)
                        >= Application.styleHints.startDragDistance)
                    dragging = true;
                if (dragging) {
                    const point = mapToItem(bookmarkList.contentItem, mouse.x, mouse.y);
                    bookmarkList.dropRow = bookmarkList.dropRowAt(point.y);
                }
            }
            onReleased: mouse => {
                if (dragging) {
                    const target = bookmarkList.dropRow;
                    dragging = false;
                    bookmarkList.dropRow = -1;
                    // The row goes in front of the target row.
                    root.bookmarks.move(row.index, target > row.index ? target - 1 : target);
                } else if (mouse.button === Qt.LeftButton && containsMouse) {
                    root.controller.openBookmark(row.path);
                }
            }
            onCanceled: {
                dragging = false;
                bookmarkList.dropRow = -1;
            }
        }

        DropArea {
            id: dropArea

            anchors.fill: parent
            onEntered: drag => drag.accepted = drag.hasUrls
            onDropped: drop => {
                root.controller.dropOnBookmark(drop.urls, row.path, drop.proposedAction);
                drop.acceptProposedAction();
            }
        }

        RowLayout {
            id: content

            anchors.fill: parent
            anchors.leftMargin: root.bookmarkMarginH
            anchors.rightMargin: root.bookmarkMarginH
            spacing: root.bookmarkSpacing

            IconGlyph {
                icon: FluentIcons.Folder16
                size: Theme.compactIconSize
                color: Theme.colors.folderIcons
            }

            Label {
                Layout.fillWidth: true
                text: row.name
                color: Theme.colors.textHc2
                elide: Text.ElideRight
            }

            BookmarkButton {
                visible: row.showButtons
                glyph: FluentIcons.ChevronUp20
                onClicked: root.bookmarks.moveUp(row.path)
            }
            BookmarkButton {
                visible: row.showButtons
                glyph: FluentIcons.ChevronDown20
                onClicked: root.bookmarks.moveDown(row.path)
            }
            BookmarkButton {
                visible: row.showButtons
                glyph: FluentIcons.BookmarkRemove20
                onClicked: root.bookmarks.remove(row.path)
            }
        }
    }

    component BookmarkButton: T.AbstractButton {
        id: bookmarkButton

        required property int glyph

        implicitWidth: Theme.compactIconSize
        implicitHeight: Theme.compactIconSize
        focusPolicy: Qt.NoFocus
        hoverEnabled: true

        background: Rectangle {
            radius: root.buttonRadius
            color: bookmarkButton.down
                   ? Color.transparent(Theme.colors.textHc2, root.bookmarkButtonPressedOpacity)
                   : bookmarkButton.hovered
                     ? Color.transparent(Theme.colors.textHc2, root.bookmarkButtonHoverOpacity)
                     : "transparent"
        }
        contentItem: IconGlyph {
            icon: bookmarkButton.glyph
            size: Theme.compactIconSize
        }
    }

    // A folder of the tree: branch chevron, folder glyph and name; hover and
    // current row shades across the full width. A click on the chevron only
    // expands or collapses (QTreeView), elsewhere it opens the folder; a
    // double click expands or collapses.
    component TreeRow: Rectangle {
        id: treeRow

        required property TreeView treeView
        required property bool isTreeNode
        required property bool expanded
        required property bool hasChildren
        required property int depth
        required property int row
        required property int column
        required property bool current
        required property string display

        implicitWidth: treeView.width
        implicitHeight: root.treeRowHeight
        color: current ? Theme.colors.folderViewHc2
             : treeHover.hovered || treeDrop.containsDrag ? Theme.colors.folderViewHc
             : "transparent"

        readonly property real branchLeft: root.treeBranchPadding + depth * root.treeIndentation
        readonly property real branchWidth: root.treeIndentation - root.treeBranchPadding

        HoverHandler {
            id: treeHover
        }

        MouseArea {
            anchors.fill: parent
            onClicked: mouse => {
                if (treeRow.hasChildren && mouse.x >= treeRow.branchLeft
                        && mouse.x < treeRow.branchLeft + treeRow.branchWidth) {
                    treeRow.treeView.toggleExpanded(treeRow.row);
                    return;
                }
                const index = treeRow.treeView.index(treeRow.row, treeRow.column);
                treeRow.treeView.selectionModel.setCurrentIndex(index,
                                                                ItemSelectionModel.ClearAndSelect);
                root.controller.openFolder(index);
            }
            onDoubleClicked: treeRow.treeView.toggleExpanded(treeRow.row)
        }

        DropArea {
            id: treeDrop

            anchors.fill: parent
            onEntered: drag => drag.accepted = drag.hasUrls
            onDropped: drop => {
                root.controller.dropOnFolder(drop.urls,
                                             treeRow.treeView.index(treeRow.row, treeRow.column),
                                             drop.proposedAction);
                drop.acceptProposedAction();
            }
        }

        Row {
            x: treeRow.branchLeft
            height: parent.height
            spacing: root.treeTextSpacing

            Item {
                width: treeRow.branchWidth
                height: parent.height

                IconGlyph {
                    anchors.centerIn: parent
                    visible: treeRow.hasChildren
                    icon: FluentIcons.ChevronDown12
                    size: StyleConstants.chevronSize
                    rotation: treeRow.expanded ? 0 : root.collapsedRotation
                }
            }

            IconGlyph {
                anchors.verticalCenter: parent.verticalCenter
                icon: FluentIcons.Folder16
                size: Theme.compactIconSize
                color: Theme.colors.folderIcons
            }

            Label {
                anchors.verticalCenter: parent.verticalCenter
                text: treeRow.display
                color: Theme.colors.textHc2
            }
        }
    }
}
