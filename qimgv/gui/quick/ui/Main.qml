pragma ComponentBehavior: Bound

import QtQuick
import qimgv.bridges
import qimgv.style

// Main window of the Qt Quick UI (--ui=quick). The application style
// (qimgv.style, based on Basic) is imported directly instead of
// QtQuick.Controls: the style is selected at compile time, so qmlsc compiles
// the bindings ahead of time instead of resolving the style at runtime.
//
// Created hidden: the Quick UI host applies the graphics configuration first,
// and Core shows the window (geometry, fullscreen) through its window port.
ApplicationWindow {
    id: root

    // Provided by QuickUiHost (initial properties).
    required property ImageViewportController viewportController
    required property MainWindowShell windowShell
    required property OverlayCoordinator overlays
    required property ThumbnailPanelController thumbnailPanel
    required property ContextMenuModel contextMenu
    required property CropController crop

    // The context menu exists from its first opening on.
    property bool contextMenuCreated: false

    readonly property int initialWidth: 1280
    readonly property int initialHeight: 800

    width: initialWidth
    height: initialHeight
    visible: false
    // The title is kept current by the Quick UI host.
    title: Qt.application.name
    color: windowShell.fullscreen ? Theme.colors.backgroundFullscreen
                                  : Theme.colors.background

    // Document mode: the image viewer and the thumbnail panel, which takes
    // its space from the viewer while pinned and floats over it otherwise.
    // The side panel takes its space from both while the crop mode is
    // active; the crop overlay covers the viewer then.
    Item {
        anchors.fill: parent
        anchors.rightMargin: sidePanel.visible ? sidePanel.width : 0
        visible: !root.windowShell.folderViewActive

        ImageViewport {
            id: viewport

            anchors.fill: parent
            anchors.topMargin: mainPanel.dockedTop
            anchors.bottomMargin: mainPanel.dockedBottom
            anchors.leftMargin: mainPanel.dockedLeft
            anchors.rightMargin: mainPanel.dockedRight
            controller: root.viewportController
            focus: parent.visible && !root.overlays.keyboardOverlayOpen && !root.crop.active
        }

        CropOverlay {
            anchors.fill: viewport
            controller: root.crop
        }

        MainPanel {
            id: mainPanel

            controller: root.thumbnailPanel
        }
    }

    SidePanel {
        id: sidePanel

        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        crop: root.crop
        focus: root.crop.active && !root.overlays.keyboardOverlayOpen
    }

    // Folder mode. The folder view is not part of the Qt Quick UI yet; the
    // page keeps the action shortcuts working, so the user can leave it.
    FocusScope {
        anchors.fill: parent
        visible: root.windowShell.folderViewActive
        focus: visible && !root.overlays.keyboardOverlayOpen
        Keys.onPressed: event => event.accepted = Actions.handleKeyEvent(event)

        Label {
            anchors.centerIn: parent
            text: qsTr("The folder view is not available in this user interface yet.")
            color: Theme.colors.text
            font: Theme.fonts.base
        }
    }

    OverlayLayer {
        anchors.fill: parent
        coordinator: root.overlays
        focus: root.overlays.keyboardOverlayOpen
    }

    Connections {
        target: root.contextMenu

        function onOpenChanged() {
            if (root.contextMenu.open)
                root.contextMenuCreated = true;
        }
    }

    Loader {
        active: root.contextMenuCreated
        sourceComponent: ViewerContextMenu {
            menuModel: root.contextMenu
        }
    }

    // Files dropped anywhere on the window open like in the widget UI.
    DropArea {
        anchors.fill: parent
        onEntered: drag => drag.accepted = drag.hasUrls
        onDropped: drop => {
            root.windowShell.dropUrls(drop.urls, drop.source);
            drop.accept();
        }
    }
}
