pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import qimgv.bridges
import qimgv.style

// Settings page "Controls": the shortcut table (add, edit, remove, reset to
// defaults; a double click edits a row), clickable edges, image scrolling
// and mouse scrolling speed.
SettingsPage {
    id: root

    required property SettingsEditorModel editor

    readonly property int tableHeight: 300
    readonly property int rowHeight: 26

    title: qsTranslate("SettingsDialog", "Controls")

    Connections {
        target: root.editor

        function onShortcutPut(row) {
            shortcutList.currentIndex = row;
            shortcutList.positionViewAtIndex(row, ListView.Contain);
        }
    }

    SettingsSection {
        RowLayout {
            spacing: StyleConstants.dialogSpacing

            Button {
                text: qsTranslate("SettingsDialog", "Add")
                onClicked: root.editor.addShortcut()
            }
            Button {
                text: qsTranslate("SettingsDialog", "Edit")
                enabled: shortcutList.currentIndex >= 0
                onClicked: root.editor.editShortcut(shortcutList.currentIndex)
            }
            Button {
                text: qsTranslate("SettingsDialog", "Remove")
                enabled: shortcutList.currentIndex >= 0
                onClicked: root.editor.removeShortcut(shortcutList.currentIndex)
            }
            Item {
                Layout.fillWidth: true
            }
            Button {
                text: qsTranslate("SettingsDialog", "Reset to defaults")
                onClicked: root.editor.resetShortcuts()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: root.tableHeight
            color: Theme.colors.folderView
            border.color: Theme.colors.widgetBorder
            border.width: StyleConstants.separatorThickness

            ListView {
                id: shortcutList

                anchors.fill: parent
                anchors.margins: StyleConstants.separatorThickness
                clip: true
                model: root.editor.shortcuts
                currentIndex: -1
                boundsBehavior: Flickable.StopAtBounds
                headerPositioning: ListView.OverlayHeader
                ScrollBar.vertical: ScrollBar {}

                header: Rectangle {
                    z: 2
                    width: ListView.view.width
                    height: root.rowHeight
                    color: Theme.colors.widget

                    Row {
                        anchors.fill: parent

                        Label {
                            width: parent.width / 2
                            height: parent.height
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            font.weight: Font.DemiBold
                            text: qsTranslate("SettingsDialog", "Action")
                        }
                        Label {
                            width: parent.width / 2
                            height: parent.height
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            font.weight: Font.DemiBold
                            text: qsTranslate("SettingsDialog", "Shortcut")
                        }
                    }
                }

                delegate: Rectangle {
                    id: shortcutRow

                    required property int index
                    required property string action
                    required property string shortcut

                    width: ListView.view.width
                    height: root.rowHeight
                    color: ListView.isCurrentItem ? Theme.colors.accent
                         : index % 2 ? Theme.colors.folderViewHc : "transparent"

                    Row {
                        anchors.fill: parent

                        Label {
                            width: parent.width / 2
                            height: parent.height
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                            text: shortcutRow.action
                        }
                        Label {
                            width: parent.width / 2
                            height: parent.height
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                            text: shortcutRow.shortcut
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: shortcutList.currentIndex = shortcutRow.index
                        onDoubleClicked: {
                            shortcutList.currentIndex = shortcutRow.index;
                            root.editor.editShortcut(shortcutRow.index);
                        }
                    }
                }
            }
        }
    }

    SettingsSection {
        RowLayout {
            spacing: StyleConstants.dialogSpacing

            CheckBox {
                text: qsTranslate("SettingsDialog", "Switch image by clicking window edges")
                checked: root.editor.controls.clickableEdges
                onToggled: root.editor.controls.clickableEdges = checked
            }
            CheckBox {
                text: qsTranslate("SettingsDialog", "Visible edges")
                enabled: root.editor.controls.clickableEdges
                checked: root.editor.controls.clickableEdgesVisible
                onToggled: root.editor.controls.clickableEdgesVisible = checked
            }
        }
        RowLayout {
            spacing: StyleConstants.dialogSpacing

            Label {
                text: qsTranslate("SettingsDialog", "Scroll image with:")
            }
            SettingsComboBox {
                options: root.editor.imageScrollingModes
                value: root.editor.controls.imageScrolling
                onPicked: value => root.editor.controls.imageScrolling = value
            }
        }
        SettingsNote {
            text: qsTranslate("SettingsDialog", "Note: you can also zoom by holding RMB and moving the mouse")
        }
        SettingsSliderRow {
            label: qsTranslate("SettingsDialog", "Mouse scrolling speed:")
            range: root.editor.ranges.mouseScrollingSpeed
            value: root.editor.controls.mouseScrollingSpeedStep
            valueText: root.editor.mouseScrollingSpeedText(value)
            onMoved: value => root.editor.controls.mouseScrollingSpeedStep = value
        }
        CheckBox {
            text: qsTranslate("SettingsDialog", "Trackpad detection")
            checked: root.editor.controls.trackpadDetection
            onToggled: root.editor.controls.trackpadDetection = checked
        }
        SettingsNote {
            text: qsTranslate("SettingsDialog", "Disable if you have issues with mouse scrolling")
        }
    }
}
