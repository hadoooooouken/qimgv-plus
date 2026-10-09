import QtQuick
import QtQuick.Templates as T
import qimgv.bridges

// Spin box: the rounded field of the crop panel's QSpinBox, with chevron
// step buttons on either side (the crop panel hides them; the settings
// dialog's spin boxes have them, so they are kept here).
// Keep in sync with DoubleSpinBox.qml.
T.SpinBox {
    id: control

    // The step buttons are part of the horizontal padding.
    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            contentItem.implicitWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding,
                             StyleConstants.spinBoxMinimumContentHeight + topPadding + bottomPadding,
                             up.implicitIndicatorHeight, down.implicitIndicatorHeight)

    topPadding: StyleConstants.spinBoxVerticalPadding + StyleConstants.fieldBorderWidth
    bottomPadding: StyleConstants.spinBoxVerticalPadding + StyleConstants.fieldBorderWidth
    leftPadding: StyleConstants.fieldBorderWidth
                 + (control.mirrored ? (up.indicator ? up.indicator.width : 0)
                                     : (down.indicator ? down.indicator.width : 0))
    rightPadding: StyleConstants.fieldBorderWidth
                  + (control.mirrored ? (down.indicator ? down.indicator.width : 0)
                                      : (up.indicator ? up.indicator.width : 0))

    validator: IntValidator {
        locale: control.locale.name
        bottom: Math.min(control.from, control.to)
        top: Math.max(control.from, control.to)
    }

    contentItem: TextInput {
        z: 2
        text: control.displayText
        clip: width < implicitWidth

        font: control.font
        color: Theme.colors.text
        opacity: control.enabled ? 1.0 : StyleConstants.disabledOpacity
        selectionColor: Theme.colors.accent
        selectedTextColor: Theme.colors.textHc2
        horizontalAlignment: Qt.AlignHCenter
        verticalAlignment: Qt.AlignVCenter

        readOnly: !control.editable
        validator: control.validator
        inputMethodHints: control.inputMethodHints
    }

    up.indicator: SpinStepIndicator {
        x: control.mirrored ? StyleConstants.fieldBorderWidth
                            : control.width - width - StyleConstants.fieldBorderWidth
        y: StyleConstants.fieldBorderWidth
        height: control.height - 2 * StyleConstants.fieldBorderWidth
        icon: FluentIcons.ChevronUp20
        pressed: control.up.pressed
        hovered: control.up.hovered
        enabled: control.enabled
                 && (control.wrap || control.value < Math.max(control.from, control.to))
    }

    down.indicator: SpinStepIndicator {
        x: control.mirrored ? control.width - width - StyleConstants.fieldBorderWidth
                            : StyleConstants.fieldBorderWidth
        y: StyleConstants.fieldBorderWidth
        height: control.height - 2 * StyleConstants.fieldBorderWidth
        icon: FluentIcons.ChevronDown20
        pressed: control.down.pressed
        hovered: control.down.hovered
        enabled: control.enabled
                 && (control.wrap || control.value > Math.min(control.from, control.to))
    }

    background: Rectangle {
        implicitWidth: StyleConstants.comboImplicitWidth
        radius: StyleConstants.controlRadius
        color: control.enabled && (control.hovered || control.activeFocus)
               ? Theme.colors.buttonHover : Theme.colors.button
        border.width: StyleConstants.fieldBorderWidth
        border.color: color
    }
}
