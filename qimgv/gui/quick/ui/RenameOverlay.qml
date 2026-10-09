import QtQuick
import qimgv.bridges
import qimgv.style

// Rename prompt in the middle of the window (RenameOverlay in the widget UI).
// It opens with the file name, its base name selected. Enter renames, Escape
// or a click on the backdrop cancels; the exit and rename shortcuts still
// run their actions, every other key stays in the prompt. In the folder view
// a backdrop dims the window behind it.
FocusScope {
    id: root

    required property OverlayCoordinator coordinator
    readonly property RenamePromptController prompt: coordinator.renamePrompt

    readonly property real backdropOpacity: 0.8
    readonly property int padding: 9
    readonly property int contentSpacing: 6
    readonly property int headerSpacing: 10

    visible: coordinator.rename.open

    // Fills the field with the current name, base name selected.
    function startEditing() {
        nameField.text = root.prompt.name;
        nameField.select(0, root.prompt.selectionEnd);
        nameField.forceActiveFocus();
    }

    Keys.onPressed: event => event.accepted = true

    // Created asynchronously on first use, possibly after it was opened.
    Component.onCompleted: {
        if (root.coordinator.rename.open)
            root.startEditing();
    }

    Connections {
        target: root.coordinator.rename
        function onOpenChanged() {
            if (root.coordinator.rename.open)
                root.startEditing();
        }
    }

    Rectangle {
        anchors.fill: parent
        visible: root.prompt.backdrop
        color: Color.transparent(Theme.colors.folderViewHc2, root.backdropOpacity)

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.AllButtons
            onPressed: root.prompt.cancel()
            onWheel: wheel => wheel.accepted = true
        }
    }

    OverlayPanel {
        icon: FluentIcons.Rename20
        title: qsTr("Rename")
        width: Math.max(Theme.metrics.renameOverlayWidth, nameField.implicitWidth + 2 * root.padding)
        bottomPadding: root.padding
        placementX: (root.width - width) / 2
        placementY: (root.height - height) / 2
        onCloseClicked: root.prompt.cancel()

        Item {
            width: parent.width
            height: root.headerSpacing
        }

        Column {
            x: root.padding
            width: parent.width - 2 * root.padding
            spacing: root.contentSpacing

            TextField {
                id: nameField

                width: parent.width
                focus: true

                Keys.onPressed: event => {
                    if (event.key === Qt.Key_Escape) {
                        root.prompt.cancel();
                        event.accepted = true;
                    } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                        root.prompt.accept(nameField.text);
                        event.accepted = true;
                    } else if (root.prompt.passesThrough(Actions.shortcutText(event))) {
                        Actions.handleKeyEvent(event);
                        event.accepted = true;
                    }
                }
            }

            Row {
                anchors.right: parent.right

                Button {
                    text: qsTr("Rename")
                    highlighted: true
                    focusPolicy: Qt.NoFocus
                    onClicked: root.prompt.accept(nameField.text)
                }
                Button {
                    text: qsTr("Cancel")
                    focusPolicy: Qt.NoFocus
                    onClicked: root.prompt.cancel()
                }
            }
        }
    }
}
