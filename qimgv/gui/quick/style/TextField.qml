import QtQuick
import QtQuick.Templates as T
import qimgv.bridges

// Text field (QLineEdit of floating panels and the folder view name
// filter): a rounded field in the button colour that lightens while hovered
// or focused.
T.TextField {
    id: control

    implicitWidth: implicitBackgroundWidth + leftInset + rightInset
                   || Math.max(contentWidth, placeholder.implicitWidth) + leftPadding + rightPadding
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             contentHeight + topPadding + bottomPadding,
                             placeholder.implicitHeight + topPadding + bottomPadding)

    topPadding: StyleConstants.fieldVerticalPadding + StyleConstants.fieldBorderWidth
    bottomPadding: StyleConstants.fieldVerticalPadding + StyleConstants.fieldBorderWidth
    leftPadding: StyleConstants.fieldHorizontalPadding + StyleConstants.fieldBorderWidth
    rightPadding: StyleConstants.fieldHorizontalPadding + StyleConstants.fieldBorderWidth

    color: Theme.colors.textHc
    selectionColor: Theme.colors.accent
    selectedTextColor: Theme.colors.textHc2
    placeholderTextColor: Color.transparent(Theme.colors.text, StyleConstants.secondaryTextOpacity)
    verticalAlignment: TextInput.AlignVCenter
    opacity: enabled ? 1.0 : StyleConstants.disabledOpacity

    Text {
        id: placeholder

        x: control.leftPadding
        y: control.topPadding
        width: control.width - (control.leftPadding + control.rightPadding)
        height: control.height - (control.topPadding + control.bottomPadding)

        text: control.placeholderText
        font: control.font
        color: control.placeholderTextColor
        verticalAlignment: control.verticalAlignment
        visible: !control.length && !control.preeditText
                 && (!control.activeFocus || control.horizontalAlignment !== Qt.AlignHCenter)
        elide: Text.ElideRight
        renderType: control.renderType
    }

    background: Rectangle {
        implicitWidth: Theme.metrics.contextMenuWidth
        radius: StyleConstants.controlRadius
        color: control.enabled && (control.hovered || control.activeFocus)
               ? Theme.colors.buttonHover : Theme.colors.button
        border.width: StyleConstants.fieldBorderWidth
        border.color: color
    }
}
