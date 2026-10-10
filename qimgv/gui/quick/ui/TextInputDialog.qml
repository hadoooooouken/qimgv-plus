import QtQuick
import QtQuick.Layouts
import qimgv.style

// Single line text prompt (QInputDialog::getText() in the widget UI): the
// label above the field, OK and Cancel. The field starts with the initial
// text, selected; Enter accepts it, Escape and the close button cancel.
DialogWindow {
    id: root

    required property TextInputDialogModel dialog

    // Width of the field, like the line edit of a QInputDialog.
    readonly property int fieldWidth: 300

    title: root.dialog.title
    initialFocusItem: field
    visible: root.dialog.open

    onAcceptRequested: root.dialog.accept(field.text)
    onDismissRequested: root.dialog.reject()

    onVisibleChanged: {
        if (!root.visible)
            return;
        field.text = root.dialog.initialText;
        field.selectAll();
    }

    Label {
        Layout.fillWidth: true
        text: root.dialog.label
        textFormat: Text.PlainText
    }

    TextField {
        id: field

        Layout.fillWidth: true
        Layout.preferredWidth: root.fieldWidth
    }

    DialogButtonBox {
        Layout.fillWidth: true
        Layout.topMargin: StyleConstants.dialogSpacing
        standardButtons: DialogButtonBox.Ok | DialogButtonBox.Cancel
        onAccepted: root.dialog.accept(field.text)
        onRejected: root.dialog.reject()
    }
}
