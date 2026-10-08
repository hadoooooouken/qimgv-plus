pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic

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
}
