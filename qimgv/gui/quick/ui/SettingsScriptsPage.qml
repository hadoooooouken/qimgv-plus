pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import qimgv.bridges
import qimgv.style

// Settings page "Scripts": the "Open with" entries (add, edit, remove; a
// double click edits one). Changes take effect at once.
SettingsPage {
    id: root

    required property SettingsEditorModel editor

    readonly property int listHeight: 300
    readonly property int rowHeight: 26

    title: qsTranslate("SettingsDialog", "Scripts")

    SettingsSection {
        SettingsNote {
            text: qsTranslate("SettingsDialog", "Note: these will appear in \"Open with\" menu.")
        }
        SettingsNote {
            text: qsTranslate("SettingsDialog", "Also, you can assign shortcuts to scripts (in \"Controls\" section).")
        }
        RowLayout {
            spacing: StyleConstants.dialogSpacing

            Button {
                text: qsTranslate("SettingsDialog", "Add")
                onClicked: root.editor.addScript()
            }
            Button {
                text: qsTranslate("SettingsDialog", "Edit")
                enabled: scriptList.currentIndex >= 0
                onClicked: root.editor.editScript(scriptList.currentIndex)
            }
            Button {
                text: qsTranslate("SettingsDialog", "Remove")
                enabled: scriptList.currentIndex >= 0
                onClicked: root.editor.removeScript(scriptList.currentIndex)
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: root.listHeight
            color: Theme.colors.folderView
            border.color: Theme.colors.widgetBorder
            border.width: StyleConstants.separatorThickness

            ListView {
                id: scriptList

                anchors.fill: parent
                anchors.margins: StyleConstants.separatorThickness
                clip: true
                model: root.editor.scripts
                currentIndex: -1
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar {}

                delegate: Rectangle {
                    id: scriptRow

                    required property int index
                    required property string display

                    width: ListView.view.width
                    height: root.rowHeight
                    color: ListView.isCurrentItem ? Theme.colors.accent
                         : index % 2 ? Theme.colors.folderViewHc : "transparent"

                    Label {
                        anchors.fill: parent
                        anchors.leftMargin: StyleConstants.itemHorizontalPadding
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                        text: scriptRow.display
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: scriptList.currentIndex = scriptRow.index
                        onDoubleClicked: {
                            scriptList.currentIndex = scriptRow.index;
                            root.editor.editScript(scriptRow.index);
                        }
                    }
                }
            }
        }
    }
}
