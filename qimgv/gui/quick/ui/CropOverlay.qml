pragma ComponentBehavior: Bound

import QtQuick
import qimgv.bridges

// The crop selection over the viewer (CropOverlay in the widget UI): a tint
// over the image outside the selection (over the whole image without one),
// the selection outline and its eight handles. Covers the viewer while the
// crop mode is active and reports the pointer to CropController, which
// decides what it does; the wheel goes on to the action shortcuts.
//
// Keys of the crop mode, wherever the focus is: Enter runs the default
// action, Shift+Enter crop and save, Escape cancels, Ctrl+A selects the
// whole image (unless an input edits text).
Item {
    id: root

    required property CropController controller

    // The widget overlay's colours.
    readonly property color tintColor: Qt.rgba(0, 0, 0, 160 / 255)
    readonly property color handleColor: Qt.rgba(150 / 255, 150 / 255, 150 / 255, 160 / 255)
    readonly property color outlineColor: "white"
    readonly property int outlineWidth: 1

    readonly property rect image: controller.imageArea
    readonly property rect selection: controller.selectionArea
    readonly property bool hasSelection: controller.hasSelection

    // Leaves the input that has the focus, so its edit is applied, before a
    // key of the crop mode acts.
    function commitInputs() {
        root.forceActiveFocus();
    }

    visible: controller.active

    Keys.onPressed: event => event.accepted = Actions.handleKeyEvent(event)

    Shortcut {
        sequences: ["Return", "Enter"]
        enabled: root.controller.active
        onActivated: {
            root.commitInputs();
            root.controller.cropDefault();
        }
    }

    Shortcut {
        sequences: ["Shift+Return", "Shift+Enter"]
        enabled: root.controller.active
        onActivated: {
            root.commitInputs();
            root.controller.cropAndSave();
        }
    }

    Shortcut {
        sequence: "Esc"
        enabled: root.controller.active
        onActivated: root.controller.cancel()
    }

    Shortcut {
        sequence: StandardKey.SelectAll
        enabled: root.controller.active
        onActivated: root.controller.selectAll()
    }

    // Tint: the whole image, or the four bands around the selection.
    TintBand {
        visible: !root.hasSelection
        x: root.image.x
        y: root.image.y
        width: root.image.width
        height: root.image.height
    }

    TintBand {
        visible: root.hasSelection
        x: root.image.x
        y: root.image.y
        width: root.image.width
        height: root.selection.y - root.image.y
    }

    TintBand {
        visible: root.hasSelection
        x: root.image.x
        y: root.selection.y + root.selection.height
        width: root.image.width
        height: root.image.y + root.image.height - y
    }

    TintBand {
        visible: root.hasSelection
        x: root.image.x
        y: root.selection.y
        width: root.selection.x - root.image.x
        height: root.selection.height
    }

    TintBand {
        visible: root.hasSelection
        x: root.selection.x + root.selection.width
        y: root.selection.y
        width: root.image.x + root.image.width - x
        height: root.selection.height
    }

    Rectangle {
        visible: root.hasSelection
        x: root.selection.x
        y: root.selection.y
        width: root.selection.width
        height: root.selection.height
        color: "transparent"
        border.width: root.outlineWidth
        border.color: root.outlineColor
    }

    Repeater {
        model: root.controller.handlesVisible ? root.controller.handles : []

        delegate: Rectangle {
            required property rect modelData

            x: modelData.x
            y: modelData.y
            width: modelData.width
            height: modelData.height
            color: root.handleColor
            border.width: root.outlineWidth
            border.color: root.outlineColor
        }
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        hoverEnabled: true
        cursorShape: root.controller.cursorShape

        onPressed: mouse => root.controller.pointerPressed(Qt.point(mouse.x, mouse.y), mouse.button)
        onPositionChanged: mouse => root.controller.pointerMoved(Qt.point(mouse.x, mouse.y), mouse.buttons)
        onReleased: mouse => root.controller.pointerReleased(Qt.point(mouse.x, mouse.y))
        onWheel: wheel => wheel.accepted = Actions.handleWheelEvent(wheel)
    }

    component TintBand: Rectangle {
        color: root.tintColor
    }
}
