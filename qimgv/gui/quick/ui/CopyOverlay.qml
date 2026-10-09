pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Dialogs
import QtQuick.Layouts
import qimgv.bridges
import qimgv.style

// Copy or move the current file to one of nine folders (CopyOverlay in the
// widget UI), in the bottom left corner, or the top left when the thumbnail
// panel is at the bottom. Keys 1 - 9 pick a folder; other keys run their
// actions. The folder icon of a row picks another folder for it.
FocusScope {
    id: root

    required property OverlayCoordinator coordinator
    readonly property CopyTargetsModel targets: coordinator.copyTargets

    readonly property int panelWidth: 240
    readonly property int horizontalMargin: 20
    readonly property int verticalMargin: 35
    readonly property int contentSpacing: 8
    readonly property int bottomPadding: 4
    readonly property int rowPadding: 6
    readonly property int folderButtonPadding: 5
    readonly property string fileUrlPrefix: "file:///"

    visible: coordinator.copy.open

    // Every key is handled here, so Tab does not move the focus.
    Keys.onPressed: event => {
        if (!root.targets.activateShortcut(Actions.keyText(event)))
            Actions.handleKeyEvent(event);
        event.accepted = true;
    }

    FolderDialog {
        id: folderDialog

        property int row: -1

        title: qsTr("Select directory")

        onAccepted: root.targets.setDirectory(folderDialog.row, folderDialog.selectedFolder.toString())
    }

    OverlayPanel {
        icon: root.targets.mode === CopyTargetsModel.Move ? FluentIcons.Move20 : FluentIcons.CopyAdd20
        title: root.targets.mode === CopyTargetsModel.Move ? qsTr("Move to...") : qsTr("Copy to...")
        width: root.panelWidth
        bottomPadding: root.bottomPadding
        placementX: root.horizontalMargin
        placementY: root.coordinator.copyAtTop ? root.verticalMargin
                                               : root.height - height - root.verticalMargin
        onCloseClicked: root.coordinator.copy.close()

        Item {
            width: parent.width
            height: root.contentSpacing
        }

        Repeater {
            model: root.targets

            delegate: Item {
                id: target

                required property int index
                required property string directory
                required property string displayName
                required property string shortcut

                width: parent.width
                height: Theme.metrics.contextMenuItemHeight

                Rectangle {
                    anchors.fill: parent
                    visible: rowArea.containsMouse
                    radius: StyleConstants.itemRadius
                    color: Color.transparent(Theme.colors.accent, StyleConstants.accentHighlightOpacity)
                }

                MouseArea {
                    id: rowArea

                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: root.targets.activate(target.index)
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.rightMargin: root.rowPadding
                    spacing: 0

                    OverlayHeaderButton {
                        glyph: FluentIcons.Folder20
                        glyphSize: Theme.standardIconSize
                        size: Theme.metrics.contextMenuItemHeight
                        surfaceInset: root.folderButtonPadding / 2
                        Accessible.name: qsTr("Select directory")
                        onClicked: {
                            folderDialog.row = target.index;
                            folderDialog.currentFolder = root.fileUrlPrefix + target.directory;
                            folderDialog.open();
                        }
                    }

                    Label {
                        text: target.displayName
                        color: Theme.colors.textHc
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }

                    Label {
                        text: target.shortcut
                        color: Color.transparent(Theme.colors.text, StyleConstants.secondaryTextOpacity)
                    }
                }
            }
        }
    }
}
