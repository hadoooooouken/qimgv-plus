pragma ComponentBehavior: Bound

import QtQuick
import qimgv.bridges
import qimgv.style

// Main window of the Qt Quick UI. The application style
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
    required property FolderViewController folderView
    required property DialogCoordinator dialogs
    required property SettingsDialogController settingsDialog

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

    // Folder mode. The folder view is created on its first activation, in
    // the background; until then the page keeps the action shortcuts
    // working.
    FocusScope {
        anchors.fill: parent
        visible: root.windowShell.folderViewActive
        focus: visible && !root.overlays.keyboardOverlayOpen
        Keys.onPressed: event => event.accepted = Actions.handleKeyEvent(event)

        Loader {
            anchors.fill: parent
            active: root.folderView.created
            asynchronous: true
            focus: true
            sourceComponent: FolderView {
                controller: root.folderView
                focus: true
            }
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

    // Modal dialog windows of the dialog port, created on first use.
    DialogLayer {
        coordinator: root.dialogs
    }

    // The settings window, created when it is first opened.
    Loader {
        active: root.settingsDialog.created
        sourceComponent: SettingsDialog {
            controller: root.settingsDialog
            transientParent: root
        }
    }

    // Files dropped anywhere on the window open like in the widget UI; the
    // drop areas of the folder view lie above it.
    DropArea {
        anchors.fill: parent
        z: -1
        onEntered: drag => drag.accepted = drag.hasUrls
        onDropped: drop => {
            root.windowShell.dropUrls(drop.urls, drop.source);
            drop.accept();
        }
    }
}
