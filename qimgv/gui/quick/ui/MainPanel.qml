pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T
import qimgv.bridges
import qimgv.style

// Thumbnail panel of the document view (MainPanel and SlidePanel in the
// widget UI): the thumbnail strip with the settings, folder view, pin and
// (fullscreen) exit buttons, at the side of the window the settings name. It
// slides in and out by the controller's slide distance while fading, or
// appears at once, as the controller says; while it slides, no thumbnails
// are requested. The content
// is loaded asynchronously once the controller allows it, so a pinned panel
// reserves its space from the first frame without creating the strip.
Item {
    id: panel

    required property ThumbnailPanelController controller

    readonly property thumbnailStripLayout stripLayout: controller.layout
    readonly property bool horizontal: stripLayout.horizontal
    readonly property int extent: stripLayout.panelExtent
    readonly property bool atTop: controller.position === SettingsEnums.PanelPosition.Top
    readonly property bool atBottom: controller.position === SettingsEnums.PanelPosition.Bottom
    readonly property bool atLeft: controller.position === SettingsEnums.PanelPosition.Left
    readonly property bool atRight: controller.position === SettingsEnums.PanelPosition.Right
    // Space a docked (pinned) panel takes from the viewer at each side.
    readonly property int dockedTop: controller.docked && atTop ? extent : 0
    readonly property int dockedBottom: controller.docked && atBottom ? extent : 0
    readonly property int dockedLeft: controller.docked && atLeft ? extent : 0
    readonly property int dockedRight: controller.docked && atRight ? extent : 0

    // 0 while shown, 1 when slid out. Set from the controller, not bound to
    // it: a binding would jump to the end before the slide could start.
    property real slide: 1

    // MainPanel: border line and the strip above a bottom panel, button size
    // and the spacing of the button group.
    readonly property int borderWidth: 1
    readonly property int bottomStrip: 3
    readonly property int buttonSize: 30
    readonly property int buttonSpacing: 4

    x: atRight ? parent.width - width : 0
    y: atBottom ? parent.height - height : 0
    width: horizontal ? parent.width : extent
    height: horizontal ? extent : parent.height
    // Visible from the start of a slide in to the end of a slide out.
    visible: controller.enabled && (controller.shown || slide < 1)
    opacity: 1 - slide

    transform: Translate {
        x: panel.atLeft ? -panel.slide * panel.controller.slideDistance
         : panel.atRight ? panel.slide * panel.controller.slideDistance : 0
        y: panel.atTop ? -panel.slide * panel.controller.slideDistance
         : panel.atBottom ? panel.slide * panel.controller.slideDistance : 0
    }

    // The slide is driven here rather than by a binding Behavior: shown and
    // animated change together, so the order of two bindings must not decide
    // whether it animates.
    Connections {
        target: panel.controller

        function onShownChanged() {
            slideAnimation.stop();
            const target = panel.controller.shown ? 0 : 1;
            if (!panel.controller.animated) {
                panel.slide = target;
                return;
            }
            slideAnimation.to = target;
            slideAnimation.easing.type = panel.controller.shown ? Easing.OutCubic : Easing.InCubic;
            slideAnimation.start();
        }
    }

    Component.onCompleted: slide = controller.shown ? 0 : 1

    NumberAnimation {
        id: slideAnimation

        target: panel
        property: "slide"
        duration: panel.controller.animationDuration
        onRunningChanged: panel.controller.setAnimationRunning(running)
    }

    // The panel takes the pointer: nothing under it reacts.
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        onWheel: wheel => wheel.accepted = true
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.colors.thumbPanel
    }

    // Border towards the viewer.
    Rectangle {
        color: Theme.colors.thumbPanelHc2
        x: panel.atRight ? 0 : panel.atLeft ? parent.width - panel.borderWidth : 0
        y: panel.atTop ? parent.height - panel.borderWidth : 0
        width: panel.horizontal ? parent.width : panel.borderWidth
        height: panel.horizontal ? panel.borderWidth : parent.height
    }

    Loader {
        id: content

        anchors.fill: parent
        anchors.topMargin: panel.atBottom ? panel.bottomStrip : 0
        anchors.bottomMargin: panel.atTop ? panel.borderWidth : 0
        anchors.leftMargin: panel.atRight ? panel.borderWidth : 0
        anchors.rightMargin: panel.atLeft ? panel.borderWidth : 0
        active: panel.controller.enabled && panel.controller.creationAllowed
        asynchronous: true
        sourceComponent: Item {
            ThumbnailStrip {
                controller: panel.controller
                anchors.left: parent.left
                anchors.top: panel.horizontal ? parent.top : buttons.bottom
                anchors.right: panel.horizontal ? buttons.left : parent.right
                anchors.bottom: parent.bottom
            }

            Item {
                id: buttons

                width: panel.horizontal ? panel.buttonSize + panel.buttonSpacing : parent.width
                height: panel.horizontal ? parent.height : panel.buttonSize + panel.buttonSpacing
                anchors.right: parent.right
                anchors.top: parent.top

                // Near the strip end: exit, folder view, pin (top to bottom or
                // right to left); the settings button at the far end.
                Grid {
                    flow: panel.horizontal ? Grid.TopToBottom : Grid.LeftToRight
                    layoutDirection: panel.horizontal ? Qt.LeftToRight : Qt.RightToLeft
                    columns: panel.horizontal ? 1 : 3
                    anchors.right: parent.right
                    anchors.top: parent.top

                    PanelButton {
                        size: panel.buttonSize
                        glyph: FluentIcons.Dismiss16
                        glyphSize: Theme.compactIconSize
                        visible: panel.controller.exitButtonVisible
                        onPressed: Actions.invoke("exit")
                    }
                    PanelButton {
                        size: panel.buttonSize
                        glyph: FluentIcons.Grid20
                        onPressed: Actions.invoke("folderView")
                    }
                    PanelButton {
                        size: panel.buttonSize
                        glyph: FluentIcons.Pin20
                        active: panel.controller.pinned
                        onPressed: panel.controller.togglePinned()
                    }
                }

                PanelButton {
                    size: panel.buttonSize
                    glyph: FluentIcons.Settings20
                    anchors.bottom: panel.horizontal ? parent.bottom : undefined
                    anchors.right: panel.horizontal ? parent.right : undefined
                    anchors.left: panel.horizontal ? undefined : parent.left
                    anchors.top: panel.horizontal ? undefined : parent.top
                    onPressed: Actions.invoke("openSettings")
                }
            }
        }
    }

    // ButtonSmall of the widget stylesheet: hover and checked surfaces with
    // a 1 px frame of the same colour.
    component PanelButton: T.AbstractButton {
        id: button

        required property int glyph
        // Edge of the square button.
        required property int size
        property int glyphSize: Theme.standardIconSize
        // Checked look (the pinned state), not toggled by a press.
        property bool active: false
        readonly property real surfaceRadius: 1

        implicitWidth: size
        implicitHeight: size
        focusPolicy: Qt.NoFocus
        hoverEnabled: true

        Accessible.role: Accessible.Button

        background: Rectangle {
            readonly property color surface: button.active ? Theme.colors.folderViewHc
                                           : button.hovered ? Theme.colors.folderViewHc2
                                           : "transparent"

            radius: button.surfaceRadius
            color: surface
            border.width: 1
            border.color: surface
        }

        contentItem: IconGlyph {
            icon: button.glyph
            size: button.glyphSize
        }
    }
}
