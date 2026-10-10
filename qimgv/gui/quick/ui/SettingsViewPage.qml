import QtQuick
import QtQuick.Dialogs
import QtQuick.Layouts
import qimgv.style

// Settings page "View": fit and focus, zoom, the scaling filter, colour
// management and HDR tone mapping.
SettingsPage {
    id: root

    required property SettingsEditorModel editor

    readonly property int zoomLevelsWidth: 380

    title: qsTranslate("SettingsDialog", "View")

    SettingsSection {
        title: qsTranslate("SettingsDialog", "Display options")

        SettingsRadioRow {
            label: qsTranslate("SettingsDialog", "Image fit:")
            options: root.editor.fitModes
            value: root.editor.view.imageFitMode
            onPicked: value => root.editor.view.imageFitMode = value
        }
        CheckBox {
            text: qsTranslate("SettingsDialog", "Keep fit mode selected via hotkey when switching images")
            checked: root.editor.view.keepFitMode
            onToggled: root.editor.view.keepFitMode = checked
        }
        SettingsRadioRow {
            label: qsTranslate("SettingsDialog", "Focus in 1:1 mode:")
            options: root.editor.focusPoints
            value: root.editor.view.focusPoint
            onPicked: value => root.editor.view.focusPoint = value
        }
        SettingsNote {
            text: qsTranslate("SettingsDialog", "Part of image that's focused after switching to 1:1")
        }
        CheckBox {
            text: qsTranslate("SettingsDialog", "Grid background on images with transparency")
            checked: root.editor.view.transparencyGrid
            onToggled: root.editor.view.transparencyGrid = checked
        }
        RowLayout {
            spacing: StyleConstants.dialogSpacing

            CheckBox {
                text: qsTranslate("SettingsDialog", "Expand images, up to:")
                checked: root.editor.view.expandImage
                onToggled: root.editor.view.expandImage = checked
            }
            SettingsSliderRow {
                enabled: root.editor.view.expandImage
                range: root.editor.ranges.expandLimit
                value: root.editor.view.expandLimit
                valueText: root.editor.expandLimitText(value)
                onMoved: value => root.editor.view.expandLimit = value
            }
        }
        SettingsNote {
            text: qsTranslate("SettingsDialog", "Images smaller than window will be zoomed in")
        }
    }

    SettingsSection {
        title: qsTranslate("SettingsDialog", "Zoom options")

        CheckBox {
            text: qsTranslate("SettingsDialog", "Unlock minimum zoom")
            checked: root.editor.view.unlockMinZoom
            onToggled: root.editor.view.unlockMinZoom = checked
        }
        SettingsNote {
            text: qsTranslate("SettingsDialog", "Always allow zooming below 100%")
        }
        SettingsSliderRow {
            label: qsTranslate("SettingsDialog", "Zoom step:")
            range: root.editor.ranges.zoomStep
            value: root.editor.view.zoomStepPercent
            valueText: root.editor.zoomStepText(value)
            onMoved: value => root.editor.view.zoomStepPercent = value
        }
        CheckBox {
            text: qsTranslate("SettingsDialog", "Use fixed zoom levels:")
            checked: root.editor.view.useFixedZoomLevels
            onToggled: root.editor.view.useFixedZoomLevels = checked
        }
        RowLayout {
            spacing: StyleConstants.dialogSpacing
            enabled: root.editor.view.useFixedZoomLevels

            TextField {
                id: zoomLevelsField

                Layout.preferredWidth: root.zoomLevelsWidth
                text: root.editor.view.zoomLevels
                onTextEdited: root.editor.view.zoomLevels = zoomLevelsField.text
            }
            Button {
                text: qsTranslate("SettingsDialog", "Load defaults")
                onClicked: {
                    root.editor.resetZoomLevels();
                    zoomLevelsField.text = root.editor.view.zoomLevels;
                }
            }
        }
    }

    SettingsSection {
        title: qsTranslate("SettingsDialog", "Scaling quality")

        RowLayout {
            spacing: StyleConstants.dialogSpacing

            Label {
                text: qsTranslate("SettingsDialog", "Scaling filter:")
            }
            SettingsComboBox {
                options: root.editor.scalingFilters
                value: root.editor.view.scalingFilter
                onPicked: value => root.editor.view.scalingFilter = value
            }
        }
        ColumnLayout {
            Layout.leftMargin: StyleConstants.dialogPadding
            visible: root.editor.casOptionsVisible
            spacing: StyleConstants.dialogSpacing

            SettingsSliderRow {
                label: qsTranslate("SettingsDialog", "Sharpness:")
                range: root.editor.ranges.casSharpening
                value: root.editor.view.casSharpeningPercent
                valueText: root.editor.casValueText(value)
                onMoved: value => root.editor.view.casSharpeningPercent = value
            }
            SettingsSliderRow {
                label: qsTranslate("SettingsDialog", "Contrast:")
                range: root.editor.ranges.casContrast
                value: root.editor.view.casContrastPercent
                valueText: root.editor.casValueText(value)
                onMoved: value => root.editor.view.casContrastPercent = value
            }
        }
    }

    SettingsSection {
        title: qsTranslate("SettingsDialog", "Color Management")

        CheckBox {
            text: qsTranslate("SettingsDialog", "Enable color management")
            checked: root.editor.view.colorManagementEnabled
            onToggled: root.editor.view.colorManagementEnabled = checked
        }
        RowLayout {
            spacing: StyleConstants.dialogSpacing
            enabled: root.editor.view.colorManagementEnabled

            Label {
                text: qsTranslate("SettingsDialog", "Monitor profile:")
            }
            SettingsComboBox {
                options: root.editor.monitorProfiles
                value: root.editor.view.monitorProfileType
                onPicked: value => root.editor.view.monitorProfileType = value
            }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: StyleConstants.dialogSpacing
            visible: root.editor.customProfileVisible

            Label {
                text: qsTranslate("SettingsDialog", "Profile file:")
            }
            TextField {
                Layout.fillWidth: true
                readOnly: true
                text: root.editor.view.monitorProfilePath
            }
            Button {
                text: qsTranslate("SettingsDialog", "Browse...")
                onClicked: profileDialog.open()
            }
        }
    }

    SettingsSection {
        title: qsTranslate("SettingsDialog", "HDR Tone-Mapping")

        CheckBox {
            text: qsTranslate("SettingsDialog", "Enable HDR Tone-Mapping")
            checked: root.editor.view.hdrToneMappingEnabled
            onToggled: root.editor.view.hdrToneMappingEnabled = checked
        }
        GridLayout {
            columns: 2
            columnSpacing: StyleConstants.dialogSpacing
            enabled: root.editor.view.hdrToneMappingEnabled

            Label {
                text: qsTranslate("SettingsDialog", "Tone-mapping operator:")
            }
            SettingsComboBox {
                options: root.editor.hdrOperators
                value: root.editor.view.hdrOperator
                onPicked: value => root.editor.view.hdrOperator = value
            }
            Label {
                text: qsTranslate("SettingsDialog", "Target white level:")
            }
            SettingsComboBox {
                options: root.editor.hdrTargetWhiteLevels
                value: root.editor.view.hdrTargetWhiteLevel
                onPicked: value => root.editor.view.hdrTargetWhiteLevel = value
            }
        }
    }

    FileDialog {
        id: profileDialog

        title: root.editor.colorProfileDialogTitle
        nameFilters: root.editor.colorProfileFilters
        fileMode: FileDialog.OpenFile
        onAccepted: root.editor.setMonitorProfileFile(profileDialog.selectedFile)
    }
}
