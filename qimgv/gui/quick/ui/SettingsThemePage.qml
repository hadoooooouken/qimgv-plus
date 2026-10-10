import QtQuick
import QtQuick.Dialogs
import QtQuick.Layouts
import qimgv.style

// Settings page "Theme": theme mode and custom accent, window and thumbnail
// bar opacity, black background. The theme mode, the accent, the black
// background and the thumbnail bar opacity are previewed at once (on
// release while the slider is dragged).
SettingsPage {
    id: root

    required property SettingsEditorModel editor

    readonly property int opacityLabelWidth: 140
    readonly property int swatchWidth: 40
    readonly property int swatchHeight: 22

    title: qsTranslate("SettingsDialog", "Theme")

    SettingsSection {
        RowLayout {
            spacing: StyleConstants.dialogSpacing

            Label {
                text: qsTranslate("SettingsDialog", "Theme mode:")
            }
            SettingsComboBox {
                options: root.editor.themeModes
                value: root.editor.theme.themeMode
                onPicked: value => root.editor.setThemeMode(value)
            }
        }
        RowLayout {
            spacing: StyleConstants.dialogPadding

            CheckBox {
                text: qsTranslate("SettingsDialog", "Use custom accent")
                checked: root.editor.theme.customAccent
                onToggled: root.editor.setCustomAccent(checked)
            }
            Button {
                id: accentButton

                enabled: root.editor.theme.customAccent
                Accessible.name: qsTranslate("SettingsDialog", "Accent color")
                ToolTip.text: qsTranslate("SettingsDialog", "Accent color")
                ToolTip.visible: hovered
                contentItem: Rectangle {
                    implicitWidth: root.swatchWidth
                    implicitHeight: root.swatchHeight
                    radius: StyleConstants.controlRadius
                    color: root.editor.theme.accentColor
                    opacity: accentButton.enabled ? 1.0 : StyleConstants.disabledOpacity
                }
                onClicked: {
                    accentDialog.selectedColor = root.editor.theme.accentColor;
                    accentDialog.open();
                }
            }
        }
    }

    SettingsSection {
        title: qsTranslate("SettingsDialog", "Other window tweaks")

        SettingsSliderRow {
            label: qsTranslate("SettingsDialog", "Window opacity:")
            labelWidth: root.opacityLabelWidth
            range: root.editor.ranges.opacity
            value: root.editor.theme.backgroundOpacityPercent
            valueText: root.editor.percentText(value)
            onMoved: value => root.editor.theme.backgroundOpacityPercent = value
        }
        SettingsSliderRow {
            id: thumbnailOpacityRow

            label: qsTranslate("SettingsDialog", "Thumbnail bar opacity:")
            labelWidth: root.opacityLabelWidth
            range: root.editor.ranges.opacity
            value: root.editor.theme.thumbnailOpacityPercent
            valueText: root.editor.percentText(value)
            onMoved: value => root.editor.setThumbnailOpacityPercent(value, !thumbnailOpacityRow.pressed)
            onReleased: value => root.editor.setThumbnailOpacityPercent(value, true)
        }
        CheckBox {
            text: qsTranslate("SettingsDialog", "Use black for background and thumbnail bar")
            checked: root.editor.theme.useBlackBackground
            onToggled: root.editor.setUseBlackBackground(checked)
        }
    }

    ColorDialog {
        id: accentDialog

        title: qsTranslate("SettingsDialog", "Accent color")
        onAccepted: root.editor.setAccentColor(accentDialog.selectedColor)
    }
}
