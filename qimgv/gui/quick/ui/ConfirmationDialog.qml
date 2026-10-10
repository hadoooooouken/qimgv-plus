import QtQuick
import QtQuick.Layouts
import qimgv.bridges
import qimgv.style

// Yes / No question with a warning icon (the QMessageBox of the widget UI).
// Yes is the default button; Escape and the close button answer No.
DialogWindow {
    id: root

    required property ConfirmationDialogModel dialog

    readonly property int iconSize: 32
    // Longer messages wrap, as in a QMessageBox.
    readonly property int maximumTextWidth: 480

    title: root.dialog.title
    visible: root.dialog.open

    onAcceptRequested: root.dialog.accept()
    onDismissRequested: root.dialog.reject()

    RowLayout {
        Layout.fillWidth: true
        spacing: StyleConstants.popupPadding

        IconGlyph {
            Layout.alignment: Qt.AlignTop
            icon: FluentIcons.Warning20
            size: root.iconSize
        }

        Label {
            Layout.fillWidth: true
            Layout.maximumWidth: root.maximumTextWidth
            text: root.dialog.message
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
        }
    }

    DialogButtonBox {
        Layout.fillWidth: true
        standardButtons: DialogButtonBox.Yes | DialogButtonBox.No
        defaultStandardButton: DialogButtonBox.Yes
        onAccepted: root.dialog.accept()
        onRejected: root.dialog.reject()
    }
}
