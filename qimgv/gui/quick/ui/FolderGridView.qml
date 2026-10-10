pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T
import qimgv.bridges
import qimgv.style

// The thumbnail grid of the folder view (FolderGridView in the widget UI): a
// GridView of ThumbnailWidget cells that reuses its delegates, with the
// rubber band, the scroll bar and the context menu. FolderGridController
// lays it out and decides what to select, load and where to scroll; the grid
// reports its width, viewport, pointer, keys and drops, and runs the scroll
// animations the model asks for. Its strings keep the widget grid's
// translation context.
FocusScope {
    id: root

    required property FolderGridController controller

    readonly property ThumbnailListModel thumbnails: controller.model
    readonly property folderGridLayout gridLayout: controller.layout
    // Item under the pointer, -1 for none.
    property int hoveredIndex: -1
    // The context menu exists from its first opening on.
    property bool contextMenuCreated: false
    property point contextMenuPosition

    // QRubberBand look: a translucent accent fill with an accent frame.
    readonly property real rubberBandFillOpacity: 0.25
    readonly property int rubberBandFrameWidth: 1

    // Content coordinates of the model start at the first row's top margin.
    function reportViewport() {
        thumbnails.setViewport(grid.contentY + grid.topMargin, grid.height);
    }

    function itemAt(x: real, y: real): int {
        const gridX = x - grid.x;
        if (gridX < 0 || gridX >= grid.width)
            return -1;
        return grid.indexAt(gridX + grid.contentX, y + grid.contentY);
    }

    function updateHoveredIndex() {
        hoveredIndex = pointerArea.containsMouse ? itemAt(pointerArea.mouseX, pointerArea.mouseY) : -1;
    }

    function scrollTo(offset: real, durationMs: int) {
        scrollAnimation.stop();
        const contentY = offset - grid.topMargin;
        if (durationMs <= 0) {
            grid.contentY = contentY;
            return;
        }
        scrollAnimation.to = contentY;
        scrollAnimation.duration = durationMs;
        scrollAnimation.start();
    }

    clip: true
    onVisibleChanged: thumbnails.setActive(visible)
    onActiveFocusChanged: {
        if (!activeFocus)
            controller.focusLost();
    }
    Component.onCompleted: {
        controller.setViewWidth(pointerArea.width);
        reportViewport();
        thumbnails.setActive(visible);
    }
    Component.onDestruction: thumbnails.setActive(false)

    Keys.onPressed: event => {
        event.accepted = root.controller.keyPressed(event.key, event.modifiers, event.text)
                         || Actions.handleKeyEvent(event);
    }
    Keys.onReleased: event => root.controller.keyReleased(event.key)

    Connections {
        target: root.thumbnails

        function onScrollRequested(offset: real, durationMs: int) {
            root.scrollTo(offset, durationMs);
        }
    }

    Connections {
        target: root.controller

        function onContextMenuRequested(position: point) {
            root.contextMenuPosition = position;
            const menu = menuLoader.item as FolderContextMenu;
            if (menu)
                menu.popup(root, position);
            else
                root.contextMenuCreated = true;
        }
    }

    NumberAnimation {
        id: scrollAnimation

        target: grid
        property: "contentY"
        easing.type: Easing.OutSine
        onFinished: root.thumbnails.scrollAnimationFinished()
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.colors.folderView
    }

    GridView {
        id: grid

        x: root.gridLayout.left
        width: root.gridLayout.columns * root.gridLayout.cellWidth
        height: parent.height
        topMargin: root.gridLayout.top
        cellWidth: root.gridLayout.cellWidth
        cellHeight: root.gridLayout.cellHeight
        model: root.thumbnails
        interactive: false
        boundsBehavior: Flickable.StopAtBounds
        reuseItems: true

        ScrollBar.vertical: scrollBar

        onContentYChanged: {
            root.reportViewport();
            root.updateHoveredIndex();
        }
        onHeightChanged: root.reportViewport()

        delegate: ThumbnailWidget {
            cellLayout: root.gridLayout.cell
            hoveredIndex: root.hoveredIndex
            surfaceColor: Theme.colors.folderView
            hoverColor: Theme.colors.folderViewHc
            labelOnSurface: root.controller.labelTextColor(Theme.colors.folderView)
            labelOnHover: root.controller.labelTextColor(Theme.colors.folderViewHc)
            selectedLabelColor: Theme.colors.textHc2
        }
    }

    MouseArea {
        id: pointerArea

        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: scrollBar.left
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.BackButton | Qt.ForwardButton

        onWidthChanged: root.controller.setViewWidth(width)
        onContainsMouseChanged: root.updateHoveredIndex()
        onPressed: mouse => {
            root.forceActiveFocus(Qt.MouseFocusReason);
            root.controller.press(root.itemAt(mouse.x, mouse.y), mouse.button, mouse.modifiers,
                                  Qt.point(mouse.x, mouse.y));
        }
        onPositionChanged: mouse => {
            root.updateHoveredIndex();
            root.controller.move(Qt.point(mouse.x, mouse.y), mouse.buttons, mouse.modifiers);
        }
        onReleased: mouse => root.controller.release(root.itemAt(mouse.x, mouse.y), mouse.button,
                                                     Qt.point(mouse.x, mouse.y))
        onDoubleClicked: mouse => root.controller.doubleClick(root.itemAt(mouse.x, mouse.y),
                                                              mouse.button)
        onWheel: wheel => {
            root.controller.wheel(wheel.angleDelta, wheel.pixelDelta, wheel.modifiers);
            wheel.accepted = true;
        }
    }

    Rectangle {
        readonly property rect band: root.controller.rubberBand

        visible: band.width > 0 || band.height > 0
        x: band.x
        y: band.y
        width: band.width
        height: band.height
        color: Color.transparent(Theme.colors.accent, root.rubberBandFillOpacity)
        border.width: root.rubberBandFrameWidth
        border.color: Theme.colors.accent
    }

    // Copy and move drops onto folders or into the current directory.
    DropArea {
        anchors.fill: pointerArea
        onEntered: drag => {
            drag.accepted = drag.hasUrls && root.controller.acceptsDropAction(drag.proposedAction);
        }
        onPositionChanged: drag => {
            if (!root.controller.acceptsDropAction(drag.proposedAction)) {
                drag.accepted = false;
                root.controller.dragLeft();
                return;
            }
            root.controller.dragMoved(root.itemAt(drag.x, drag.y));
        }
        onExited: root.controller.dragLeft()
        onDropped: drop => {
            if (!root.controller.acceptsDropAction(drop.proposedAction)) {
                root.controller.dragLeft();
                return;
            }
            root.controller.drop(drop.urls, drop.source, root.itemAt(drop.x, drop.y),
                                 drop.proposedAction);
            drop.accept(drop.proposedAction);
        }
    }

    ScrollBar {
        id: scrollBar

        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        orientation: Qt.Vertical
        policy: ScrollBar.AlwaysOn
    }

    Loader {
        id: menuLoader

        active: root.contextMenuCreated
        sourceComponent: FolderContextMenu {}
        onLoaded: (item as FolderContextMenu).popup(root, root.contextMenuPosition)
    }

    // FolderGridView::mouseReleaseEvent(): the selection rows are enabled
    // while something is selected; the actions run after the menu closed.
    component FolderContextMenu: Menu {
        id: folderMenu

        readonly property bool hasSelection: root.thumbnails.currentIndex >= 0

        popupType: T.Popup.Window
        width: Theme.metrics.contextMenuWidth

        MenuRow {
            text: qsTr("Open only selected")
            glyph: FluentIcons.OpenOnlySelected20
            enabled: folderMenu.hasSelection
            onTriggered: Qt.callLater(root.controller.openSelected)
        }
        MenuRow {
            text: qsTr("Batch convert")
            glyph: FluentIcons.BatchConvert20
            enabled: folderMenu.hasSelection
            onTriggered: root.controller.requestBatchConversion()
        }
        MenuRow {
            text: qsTr("Add folder")
            glyph: FluentIcons.FolderAdd20
            shortcutText: ShortcutText.of(Actions.shortcutFor("createDirectory"))
            onTriggered: Actions.invoke("createDirectory")
        }
        MenuRow {
            text: qsTr("Show in folder")
            glyph: FluentIcons.ShowInFolder20
            shortcutText: ShortcutText.of(Actions.shortcutFor("showInDirectory"))
            enabled: folderMenu.hasSelection
            onTriggered: Actions.invoke("showInDirectory")
        }
        MenuRow {
            text: qsTr("Rename")
            glyph: FluentIcons.Rename20
            enabled: folderMenu.hasSelection
            onTriggered: Actions.invoke("renameFile")
        }
        MenuSeparator {}
        MenuRow {
            text: qsTr("Move to trash")
            glyph: FluentIcons.Delete20
            glyphColor: Theme.colors.trash
            labelColor: Theme.colors.trash
            enabled: folderMenu.hasSelection
            onTriggered: Actions.invoke("moveToTrash")
        }
        MenuRow {
            text: qsTr("Delete permanently")
            glyph: FluentIcons.Dismiss20
            glyphColor: Theme.colors.danger
            labelColor: Theme.colors.danger
            enabled: folderMenu.hasSelection
            onTriggered: Actions.invoke("removeFile")
        }
    }

    component MenuRow: MenuItem {
        hasGlyph: true
        shortcutText: ""
    }
}
