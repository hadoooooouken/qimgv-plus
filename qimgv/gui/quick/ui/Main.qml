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

    readonly property int initialWidth: 1280
    readonly property int initialHeight: 800

    width: initialWidth
    height: initialHeight
    visible: false
    // The title is kept current by the Quick UI host.
    title: Qt.application.name
    color: windowShell.fullscreen ? Theme.colors.backgroundFullscreen
                                  : Theme.colors.background

    // The image viewer (document mode).
    ImageViewport {
        anchors.fill: parent
        controller: root.viewportController
        visible: !root.windowShell.folderViewActive
        focus: visible
    }

    // Folder mode. The folder view is not part of the Qt Quick UI yet; the
    // page keeps the action shortcuts working, so the user can leave it.
    FocusScope {
        anchors.fill: parent
        visible: root.windowShell.folderViewActive
        focus: visible
        Keys.onPressed: event => event.accepted = Actions.handleKeyEvent(event)

        Label {
            anchors.centerIn: parent
            text: qsTr("The folder view is not available in this user interface yet.")
            color: Theme.colors.text
            font: Theme.fonts.base
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
