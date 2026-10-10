import QtQuick
import QtQuick.Dialogs
import QtQuick.Layouts
import qimgv.bridges
import qimgv.style

// Script editor of the settings window (ScriptEditorDialog in the widget
// UI): name, command line ("..." picks an executable) and "Wait to finish".
// The accept button reads Create, Save or Replace, as the view-model says.
DialogWindow {
    id: root

    required property ScriptEditorModel dialog

    readonly property int fieldWidth: 360
    readonly property int pickButtonWidth: 60

    title: root.dialog.title
    initialFocusItem: nameField
    visible: root.dialog.open

    onAcceptRequested: root.dialog.accept()
    onDismissRequested: root.dialog.reject()

    onVisibleChanged: {
        if (!root.visible)
            return;
        nameField.text = root.dialog.name;
        commandField.text = root.dialog.command;
    }

    GridLayout {
        Layout.fillWidth: true
        columns: 3
        columnSpacing: StyleConstants.dialogSpacing

        Label {
            text: qsTranslate("ScriptEditorDialog", "Name:")
        }
        TextField {
            id: nameField

            Layout.columnSpan: 2
            Layout.fillWidth: true
            Layout.preferredWidth: root.fieldWidth
            onTextEdited: root.dialog.name = nameField.text
        }

        Label {
            text: qsTranslate("ScriptEditorDialog", "Command:")
        }
        TextField {
            id: commandField

            Layout.fillWidth: true
            onTextEdited: root.dialog.command = commandField.text
        }
        Button {
            Layout.preferredWidth: root.pickButtonWidth
            text: "..."
            onClicked: executableDialog.open()
        }
    }

    SettingsNote {
        text: root.dialog.keywordsText
    }

    CheckBox {
        text: qsTranslate("ScriptEditorDialog", "Wait to finish")
        checked: root.dialog.blocking
        onToggled: root.dialog.blocking = checked
    }

    Label {
        Layout.fillWidth: true
        text: root.dialog.message
        textFormat: Text.PlainText
        color: Theme.colors.textLc
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.topMargin: StyleConstants.dialogSpacing
        spacing: StyleConstants.dialogButtonSpacing

        Item {
            Layout.fillWidth: true
        }
        Button {
            text: root.dialog.acceptText
            enabled: root.dialog.canAccept
            onClicked: root.dialog.accept()
        }
        Button {
            text: qsTranslate("ScriptEditorDialog", "Cancel")
            onClicked: root.dialog.reject()
        }
    }

    FileDialog {
        id: executableDialog

        title: root.dialog.executableDialogTitle
        nameFilters: root.dialog.executableFilters
        fileMode: FileDialog.OpenFile
        onAccepted: {
            root.dialog.setExecutable(executableDialog.selectedFile);
            commandField.text = root.dialog.command;
        }
    }
}
