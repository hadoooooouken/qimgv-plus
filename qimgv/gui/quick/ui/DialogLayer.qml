pragma ComponentBehavior: Bound

import QtQuick

// The modal dialogs of the dialog port. Each dialog is created the first
// time it is requested (its `created` state) and kept for later requests;
// none exists at startup. The dialogs are modal over the main window that
// holds this layer, which is set explicitly because a window created by a
// Loader does not find it on its own.
Item {
    id: dialogLayer

    required property DialogCoordinator coordinator

    Loader {
        active: dialogLayer.coordinator.confirmation.created
        sourceComponent: ConfirmationDialog {
            dialog: dialogLayer.coordinator.confirmation
            transientParent: dialogLayer.Window.window
        }
    }

    Loader {
        active: dialogLayer.coordinator.fileReplace.created
        sourceComponent: FileReplaceDialog {
            dialog: dialogLayer.coordinator.fileReplace
            transientParent: dialogLayer.Window.window
        }
    }

    Loader {
        active: dialogLayer.coordinator.resize.created
        sourceComponent: ResizeDialog {
            dialog: dialogLayer.coordinator.resize
            transientParent: dialogLayer.Window.window
        }
    }

    Loader {
        active: dialogLayer.coordinator.textInput.created
        sourceComponent: TextInputDialog {
            dialog: dialogLayer.coordinator.textInput
            transientParent: dialogLayer.Window.window
        }
    }

    Loader {
        active: dialogLayer.coordinator.batchConverter.created
        sourceComponent: BatchConverterDialog {
            dialog: dialogLayer.coordinator.batchConverter
            transientParent: dialogLayer.Window.window
        }
    }

    Loader {
        active: dialogLayer.coordinator.print.created
        sourceComponent: PrintDialog {
            dialog: dialogLayer.coordinator.print
            transientParent: dialogLayer.Window.window
        }
    }

    Loader {
        active: dialogLayer.coordinator.savePath.created
        sourceComponent: SaveFileDialog {
            dialog: dialogLayer.coordinator.savePath
            parentWindow: dialogLayer.Window.window
        }
    }
}
