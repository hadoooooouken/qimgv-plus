pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import qimgv.bridges

// Main window of the Qt Quick UI (--ui=quick). The Basic style is imported
// explicitly: it is the base of the application style and lets qmlsc compile
// the bindings ahead of time instead of resolving the style at runtime.
ApplicationWindow {
    id: root

    // Provided by QuickUiHost (initial properties).
    required property ImageViewportController viewportController

    readonly property int initialWidth: 1280
    readonly property int initialHeight: 800

    width: initialWidth
    height: initialHeight
    visible: true
    title: Qt.application.name
    color: Theme.colors.background

    // The image viewer. Images are supplied from C++ once Core drives the
    // Quick UI (S2.1); until then it shows the viewer background only.
    ImageViewport {
        anchors.fill: parent
        controller: root.viewportController
    }
}
