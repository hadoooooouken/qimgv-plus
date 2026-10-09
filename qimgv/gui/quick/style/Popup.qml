import QtQuick
import QtQuick.Templates as T
import qimgv.bridges

// Floating panel (FloatingWidget in the widget stylesheet): the widget
// surface with a thin border, rounded corners and a drop shadow.
T.Popup {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    padding: StyleConstants.popupPadding

    background: PopupBackground {}

    T.Overlay.modal: Rectangle {
        color: StyleConstants.modalDimColor
    }

    T.Overlay.modeless: Rectangle {
        color: StyleConstants.modelessDimColor
    }
}
