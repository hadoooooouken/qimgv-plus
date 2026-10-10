import QtQuick
import QtQuick.Layouts
import qimgv.style

// Name collision question of a copy or move (FileReplaceDialog in the widget
// UI): the question for the kind of collision, the source and destination
// paths, "Apply to all" while more collisions may follow, and Yes / No /
// Cancel. Yes is the default button; Escape and the close button answer No
// for this item.
DialogWindow {
    id: root

    required property FileReplaceDialogModel dialog

    // Minimum width of the widget dialog.
    readonly property int minimumDialogWidth: 380
    // Longer source paths are elided in the middle, longer destinations wrap.
    readonly property int maximumPathWidth: 600

    title: {
        switch (root.dialog.collision) {
        case FileReplaceDialogModel.DirectoryOverDirectory:
            return qsTr("Directory already exists");
        case FileReplaceDialogModel.FileOverDirectory:
        case FileReplaceDialogModel.DirectoryOverFile:
            return qsTr("Destination already exists");
        default:
            return qsTr("File already exists");
        }
    }
    visible: root.dialog.open
    minimumContentWidth: root.minimumDialogWidth - 2 * StyleConstants.dialogPadding

    onAcceptRequested: root.dialog.answer(true, applyAll.checked)
    onDismissRequested: root.dialog.dismiss()

    // Every question starts with "Apply to all" cleared.
    onVisibleChanged: {
        if (root.visible)
            applyAll.checked = false;
    }

    Label {
        Layout.fillWidth: true
        text: {
            switch (root.dialog.collision) {
            case FileReplaceDialogModel.DirectoryOverDirectory:
                return qsTr("Merge directories?");
            case FileReplaceDialogModel.FileOverDirectory:
                return qsTr("There is a folder with that name. Replace?");
            case FileReplaceDialogModel.DirectoryOverFile:
                return qsTr("There is a file with that name. Replace?");
            default:
                return qsTr("Replace destination file?");
            }
        }
    }

    Label {
        text: qsTr("Source:")
        font.bold: true
    }

    Label {
        Layout.fillWidth: true
        Layout.maximumWidth: root.maximumPathWidth
        text: root.dialog.source
        textFormat: Text.PlainText
        elide: Text.ElideMiddle
    }

    Label {
        text: qsTr(">>>")
    }

    Label {
        text: qsTr("Destination:")
        font.bold: true
    }

    Label {
        Layout.fillWidth: true
        Layout.maximumWidth: root.maximumPathWidth
        text: root.dialog.destination
        textFormat: Text.PlainText
        wrapMode: Text.WrapAnywhere
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.topMargin: StyleConstants.dialogSpacing
        spacing: StyleConstants.dialogButtonSpacing

        Item {
            Layout.fillWidth: true
        }

        CheckBox {
            id: applyAll

            visible: root.dialog.multiple
            text: qsTr("Apply to all")
        }

        DialogButtonBox {
            id: buttons

            standardButtons: DialogButtonBox.Yes | DialogButtonBox.No | DialogButtonBox.Cancel
            defaultStandardButton: DialogButtonBox.Yes
            onClicked: button => {
                if (button === buttons.standardButton(DialogButtonBox.Yes))
                    root.dialog.answer(true, applyAll.checked);
                else if (button === buttons.standardButton(DialogButtonBox.No))
                    root.dialog.answer(false, applyAll.checked);
                else
                    root.dialog.cancel();
            }
        }
    }
}
