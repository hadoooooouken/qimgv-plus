import QtQuick
import qimgv.bridges
import qimgv.style

// Offer to save or discard unsaved edits (SaveConfirmOverlay in the widget
// UI), centred at the bottom of the window, or at the top when the
// thumbnail panel would cover it. Its buttons never take the focus.
Item {
    id: root

    required property OverlayCoordinator coordinator

    readonly property int topMargin: 35
    // Clears the taskbar of an auto-hiding Windows shell.
    readonly property int bottomMargin: 80
    readonly property int padding: 9
    readonly property int groupSpacing: 8
    readonly property int contentSpacing: 10

    visible: coordinator.saveConfirm.open

    OverlayPanel {
        id: panel

        icon: FluentIcons.Edit20
        title: qsTr("Unsaved edits")
        width: buttons.implicitWidth + 2 * root.padding
        bottomPadding: root.padding
        placementX: (root.width - width) / 2
        placementY: root.coordinator.saveConfirmAtTop ? root.topMargin
                                                      : root.height - height - root.bottomMargin
        onCloseClicked: root.coordinator.saveConfirm.close()

        Item {
            width: parent.width
            height: root.contentSpacing
        }

        Row {
            id: buttons

            x: root.padding
            spacing: root.groupSpacing

            Row {
                Button {
                    text: qsTr("Save")
                    focusPolicy: Qt.NoFocus
                    onClicked: root.coordinator.requestSave()
                }
                Button {
                    text: qsTr("Save as")
                    focusPolicy: Qt.NoFocus
                    onClicked: root.coordinator.requestSaveAs()
                }
            }

            Button {
                text: qsTr("Discard")
                focusPolicy: Qt.NoFocus
                onClicked: root.coordinator.requestDiscard()
            }
        }
    }
}
