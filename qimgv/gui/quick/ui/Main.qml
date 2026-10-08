pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import qimgv.bridges
import qimgv.render

// Main window of the Qt Quick UI (--ui=quick). The Basic style is imported
// explicitly: it is the base of the application style and lets qmlsc compile
// the bindings ahead of time instead of resolving the style at runtime.
ApplicationWindow {
    id: root

    readonly property int initialWidth: 1280
    readonly property int initialHeight: 800

    width: initialWidth
    height: initialHeight
    visible: true
    title: Qt.application.name
    color: Theme.colors.background

    // GPU image view. Images are supplied from C++ once Core drives the Quick
    // UI (S2.1); until then it shows the viewer background only.
    ImageRenderItem {
        id: imageView

        anchors.fill: parent
        backgroundColor: Theme.colors.background
        transparencyGrid: AppSettings.viewer.transparencyGrid
        // The scaling filter -> sampling / sharpening mapping
        // (imageFilterModeFor) is applied by the viewer port in S1.6.
        casSharpening: AppSettings.viewer.casSharpening
        casContrast: AppSettings.viewer.casContrast
    }
}
