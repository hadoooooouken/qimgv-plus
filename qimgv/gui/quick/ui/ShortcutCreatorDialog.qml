import QtQuick
import QtQuick.Layouts
import qimgv.style

// Shortcut creator of the settings window (ShortcutCreatorDialog in the
// widget UI): an action or a script, and the shortcut captured below. A
// shortcut the application already binds shows a warning; OK replaces it.
DialogWindow {
    id: root

    required property ShortcutEditorModel dialog

    readonly property int comboWidth: 200
    readonly property int captureSpacing: 10
    readonly property int warningMinimumHeight: 36
    readonly property int bodyWidth: 316

    title: root.dialog.title
    minimumContentWidth: root.bodyWidth
    initialFocusItem: captureField
    visible: root.dialog.open

    onAcceptRequested: root.dialog.accept()
    onDismissRequested: root.dialog.reject()

    GridLayout {
        Layout.fillWidth: true
        columns: 2
        columnSpacing: StyleConstants.dialogSpacing

        RadioButton {
            Layout.fillWidth: true
            text: qsTranslate("ShortcutCreatorDialog", "Action:")
            checkable: false
            checked: !root.dialog.scriptSelected
            onClicked: root.dialog.scriptSelected = false
        }
        ComboBox {
            Layout.preferredWidth: root.comboWidth
            enabled: !root.dialog.scriptSelected
            model: root.dialog.actions
            currentIndex: root.dialog.actionIndex
            onActivated: index => root.dialog.actionIndex = index
        }
        RadioButton {
            Layout.fillWidth: true
            text: qsTranslate("ShortcutCreatorDialog", "Script:")
            checkable: false
            checked: root.dialog.scriptSelected
            onClicked: root.dialog.scriptSelected = true
        }
        ComboBox {
            Layout.preferredWidth: root.comboWidth
            enabled: root.dialog.scriptSelected
            model: root.dialog.scripts
            currentIndex: root.dialog.scriptIndex
            onActivated: index => root.dialog.scriptIndex = index
        }
    }

    Label {
        Layout.topMargin: root.captureSpacing
        Layout.alignment: Qt.AlignHCenter
        text: qsTranslate("ShortcutCreatorDialog", "Shortcut:")
    }

    ShortcutCaptureField {
        id: captureField

        Layout.fillWidth: true
        text: root.dialog.shortcutText
        onCaptured: shortcut => root.dialog.setShortcut(shortcut)
    }

    Label {
        Layout.fillWidth: true
        Layout.minimumHeight: root.warningMinimumHeight
        text: root.dialog.warning
        textFormat: Text.PlainText
        wrapMode: Text.WordWrap
    }

    DialogButtonBox {
        id: buttons

        Layout.fillWidth: true
        standardButtons: DialogButtonBox.Ok | DialogButtonBox.Cancel
        onAccepted: root.dialog.accept()
        onRejected: root.dialog.reject()

        Component.onCompleted: buttons.standardButton(DialogButtonBox.Ok).enabled
                               = Qt.binding(() => root.dialog.canAccept)
    }
}
