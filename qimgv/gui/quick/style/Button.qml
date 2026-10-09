import QtQuick
import QtQuick.Controls.impl
import QtQuick.Templates as T
import qimgv.bridges

// Push button (QPushButton in the widget stylesheet). A flat button shows its
// surface only while hovered, pressed or checked.
T.Button {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    topPadding: StyleConstants.buttonVerticalPadding
    bottomPadding: StyleConstants.buttonVerticalPadding
    leftPadding: StyleConstants.buttonHorizontalPadding
    rightPadding: StyleConstants.buttonHorizontalPadding
    spacing: StyleConstants.indicatorSpacing

    icon.width: Theme.compactIconSize
    icon.height: Theme.compactIconSize
    icon.color: Theme.colors.icons

    contentItem: IconLabel {
        spacing: control.spacing
        mirrored: control.mirrored
        display: control.display
        opacity: control.enabled ? 1.0 : StyleConstants.disabledOpacity

        icon: control.icon
        text: control.text
        font: control.font
        color: control.down || control.checked ? Theme.colors.text
             : control.hovered ? Theme.colors.textHc2
             : Theme.colors.textHc
    }

    background: Rectangle {
        implicitHeight: Theme.metrics.buttonHeight
        radius: StyleConstants.controlRadius
        visible: !control.flat || control.down || control.checked || control.hovered
        color: control.down || control.checked ? Theme.colors.buttonPressed
             : control.hovered || control.highlighted ? Theme.colors.buttonHover
             : Theme.colors.button
        border.width: control.visualFocus ? StyleConstants.separatorThickness : 0
        border.color: Theme.colors.accent
    }
}
