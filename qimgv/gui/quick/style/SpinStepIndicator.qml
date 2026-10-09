import QtQuick
import qimgv.bridges

// Up or down step button of SpinBox and DoubleSpinBox: a chevron glyph on a
// translucent text-coloured tint while hovered or pressed, which stays
// visible on the field's own hover colour.
Rectangle {
    // Chevron glyph (a FluentIcons value).
    required property int icon
    property bool pressed: false
    property bool hovered: false

    implicitWidth: StyleConstants.spinStepIndicatorWidth
    implicitHeight: StyleConstants.spinBoxMinimumContentHeight
    radius: StyleConstants.controlRadius
    color: pressed ? Color.transparent(Theme.colors.textHc2, StyleConstants.stepPressedOpacity)
         : hovered ? Color.transparent(Theme.colors.textHc2, StyleConstants.stepHoverOpacity)
         : "transparent"
    opacity: enabled ? 1.0 : StyleConstants.disabledOpacity

    IconGlyph {
        anchors.centerIn: parent
        icon: parent.icon
        size: StyleConstants.chevronSize
    }
}
