import QtQuick
import QtQuick.Templates as T
import qimgv.bridges

// Slider (global QSlider rules of the widget stylesheet): a thin groove in
// the dialog tint, the filled part in the accent colour and a round accent
// handle ringed with the widget colour.
T.Slider {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitHandleWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitHandleHeight + topPadding + bottomPadding)

    padding: 0

    handle: Rectangle {
        x: control.leftPadding + (control.horizontal
                                  ? control.visualPosition * (control.availableWidth - width)
                                  : (control.availableWidth - width) / 2)
        y: control.topPadding + (control.horizontal
                                 ? (control.availableHeight - height) / 2
                                 : control.visualPosition * (control.availableHeight - height))
        implicitWidth: StyleConstants.sliderHandleSize
        implicitHeight: StyleConstants.sliderHandleSize
        radius: width / 2
        color: !control.enabled ? Theme.colors.text
             : control.hovered || control.pressed ? Color.lighter(Theme.colors.accent, StyleConstants.accentLightFactor)
             : Theme.colors.accent
        border.width: StyleConstants.sliderHandleBorderWidth
        border.color: Theme.colors.widget
    }

    background: Rectangle {
        x: control.leftPadding + (control.horizontal ? 0 : (control.availableWidth - width) / 2)
        y: control.topPadding + (control.horizontal ? (control.availableHeight - height) / 2 : 0)
        implicitWidth: control.horizontal ? StyleConstants.sliderLength : StyleConstants.sliderThickness
        implicitHeight: control.horizontal ? StyleConstants.sliderThickness : StyleConstants.sliderLength
        width: control.horizontal ? control.availableWidth : StyleConstants.sliderGrooveThickness
        height: control.horizontal ? StyleConstants.sliderGrooveThickness : control.availableHeight
        radius: StyleConstants.sliderGrooveThickness / 2
        color: Theme.surfaces.tintedHc2
        scale: control.horizontal && control.mirrored ? -1 : 1

        // Filled part, from the start of the range to the handle.
        Rectangle {
            y: control.horizontal ? 0 : control.visualPosition * parent.height
            width: control.horizontal ? control.position * parent.width : parent.width
            height: control.horizontal ? parent.height : control.position * parent.height
            radius: parent.radius
            color: control.enabled ? Theme.colors.accent : Theme.colors.widgetBorder
        }
    }
}
