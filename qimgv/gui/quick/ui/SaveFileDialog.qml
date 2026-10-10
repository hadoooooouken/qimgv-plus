import QtQuick
import QtQuick.Dialogs

// "Save File as..." in the platform's file dialog (QFileDialog::
// getSaveFileName() in the widget UI), modal to the main window. It starts
// at the suggested file with the filters of the writable formats, the
// suggested file's filter selected; the view-model turns the chosen file
// into the answer.
FileDialog {
    id: root

    required property SavePathDialogModel dialog

    // The filter index applies to the filters of the same request, so the
    // request is copied in one go before the dialog opens.
    function present() {
        root.nameFilters = root.dialog.nameFilters;
        root.selectedNameFilter.index = root.dialog.selectedFilterIndex;
        root.currentFolder = root.dialog.suggestedFolder;
        root.selectedFile = root.dialog.suggestedFile;
        root.open();
    }

    title: qsTr("Save File as...")
    fileMode: FileDialog.SaveFile

    onAccepted: root.dialog.accept(root.selectedFile)
    onRejected: root.dialog.reject()

    // Created on the first request, after it was opened.
    Component.onCompleted: {
        if (root.dialog.open)
            root.present();
    }

    property Connections requests: Connections {
        target: root.dialog

        function onOpenChanged() {
            if (root.dialog.open)
                root.present();
        }
    }
}
