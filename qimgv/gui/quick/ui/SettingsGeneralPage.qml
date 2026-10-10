import QtQuick
import QtQuick.Layouts
import qimgv.bridges
import qimgv.style

// Settings page "General": language, startup, user interface, thumbnail
// panel, folder navigation and slideshow.
SettingsPage {
    id: root

    required property SettingsEditorModel editor

    readonly property int sliderLabelWidth: 200

    title: qsTranslate("SettingsDialog", "General")

    SettingsSection {
        RowLayout {
            spacing: StyleConstants.dialogSpacing

            Label {
                text: qsTranslate("SettingsDialog", "Language:")
            }
            SettingsComboBox {
                options: root.editor.languages
                value: root.editor.general.language
                onPicked: value => root.editor.general.language = value
            }
            SettingsNote {
                text: qsTranslate("SettingsDialog", "Requires application restart")
            }
        }

        CheckBox {
            text: qsTranslate("SettingsDialog", "Open in fullscreen")
            checked: root.editor.general.fullscreenMode
            onToggled: root.editor.general.fullscreenMode = checked
        }
        CheckBox {
            text: qsTranslate("SettingsDialog", "Start in folder view by default")
            checked: root.editor.general.startInFolderView
            onToggled: root.editor.general.startInFolderView = checked
        }
        CheckBox {
            text: qsTranslate("SettingsDialog", "Enable standby mode on close")
            checked: root.editor.general.standbyMode
            onToggled: root.editor.general.standbyMode = checked
            ToolTip.text: qsTranslate("SettingsDialog", "Keeps the application running in the background when closed. Subsequent launches will be instant.")
            ToolTip.visible: hovered
        }
        CheckBox {
            text: qsTranslate("SettingsDialog", "Remember last opened folder")
            checked: root.editor.general.rememberLastFolder
            onToggled: root.editor.general.rememberLastFolder = checked
        }
    }

    SettingsSection {
        title: qsTranslate("SettingsDialog", "User interface")

        CheckBox {
            text: qsTranslate("SettingsDialog", "Image info in window title")
            checked: root.editor.general.windowTitleExtendedInfo
            onToggled: root.editor.general.windowTitleExtendedInfo = checked
        }
        CheckBox {
            text: qsTranslate("SettingsDialog", "Fullscreen info bar")
            checked: root.editor.general.infoBarFullscreen
            onToggled: root.editor.general.infoBarFullscreen = checked
        }
        CheckBox {
            text: qsTranslate("SettingsDialog", "Auto-hide cursor")
            checked: root.editor.general.cursorAutohide
            onToggled: root.editor.general.cursorAutohide = checked
        }
        CheckBox {
            text: qsTranslate("SettingsDialog", "Smooth thumbnail scrolling")
            checked: root.editor.general.smoothScroll
            onToggled: root.editor.general.smoothScroll = checked
            ToolTip.text: qsTranslate("SettingsDialog", "Turn this off if you are using a touchpad with libinput driver.")
            ToolTip.visible: hovered
        }
        CheckBox {
            text: qsTranslate("SettingsDialog", "Smooth zooming")
            checked: root.editor.general.smoothZoom
            onToggled: root.editor.general.smoothZoom = checked
        }
        SettingsRadioRow {
            label: qsTranslate("SettingsDialog", "Zoom indicator:")
            options: root.editor.zoomIndicatorModes
            value: root.editor.general.zoomIndicatorMode
            onPicked: value => root.editor.general.zoomIndicatorMode = value
        }
        RowLayout {
            spacing: StyleConstants.dialogSpacing

            CheckBox {
                text: qsTranslate("SettingsDialog", "Automatic window resize")
                checked: root.editor.general.autoResizeWindow
                onToggled: root.editor.general.autoResizeWindow = checked
            }
            SettingsNote {
                text: qsTranslate("SettingsDialog", "Match displayed content")
            }
        }
        SettingsSliderRow {
            label: qsTranslate("SettingsDialog", "Screen area limit for auto resize:")
            range: root.editor.ranges.autoResizeLimit
            value: root.editor.general.autoResizeLimitStep
            valueText: root.editor.autoResizeLimitText(value)
            onMoved: value => root.editor.general.autoResizeLimitStep = value
        }
    }

    SettingsSection {
        CheckBox {
            text: qsTranslate("SettingsDialog", "Thumbnail panel")
            font: Theme.fonts.section
            checked: root.editor.general.panelEnabled
            onToggled: root.editor.general.panelEnabled = checked
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: StyleConstants.dialogSpacing
            enabled: root.editor.general.panelEnabled

            GridLayout {
                columns: 2
                columnSpacing: StyleConstants.dialogSpacing

                CheckBox {
                    text: qsTranslate("SettingsDialog", "Crop previews")
                    checked: root.editor.general.squareThumbnails
                    onToggled: root.editor.general.squareThumbnails = checked
                }
                CheckBox {
                    text: qsTranslate("SettingsDialog", "Pinned")
                    checked: root.editor.general.panelPinned
                    onToggled: root.editor.general.panelPinned = checked
                }
                CheckBox {
                    text: qsTranslate("SettingsDialog", "Disable in windowed mode")
                    checked: root.editor.general.panelFullscreenOnly
                    onToggled: root.editor.general.panelFullscreenOnly = checked
                }
                CheckBox {
                    text: qsTranslate("SettingsDialog", "Center selected image")
                    checked: root.editor.general.panelCenterSelection
                    onToggled: root.editor.general.panelCenterSelection = checked
                }
                CheckBox {
                    text: qsTranslate("SettingsDialog", "Show subfolders")
                    checked: root.editor.general.showSubfoldersInPanel
                    onToggled: root.editor.general.showSubfoldersInPanel = checked
                }
            }
            SettingsSliderRow {
                label: qsTranslate("SettingsDialog", "Hide delay:")
                labelWidth: root.sliderLabelWidth
                range: root.editor.ranges.panelHideDelay
                value: root.editor.general.panelHideDelayMs
                valueText: root.editor.panelHideDelayText(value)
                onMoved: value => root.editor.general.panelHideDelayMs = value
            }
            SettingsRadioRow {
                label: qsTranslate("SettingsDialog", "Display style:")
                vertical: true
                options: root.editor.thumbPanelStyles
                value: root.editor.general.thumbPanelStyle
                onPicked: value => root.editor.general.thumbPanelStyle = value
            }
            SettingsSliderRow {
                label: qsTranslate("SettingsDialog", "Preview size:")
                labelWidth: root.sliderLabelWidth
                range: root.editor.ranges.panelSize
                value: root.editor.general.panelSizeStep
                onMoved: value => root.editor.general.panelSizeStep = value
            }
            RowLayout {
                spacing: StyleConstants.dialogSpacing

                Label {
                    Layout.preferredWidth: root.sliderLabelWidth
                    text: qsTranslate("SettingsDialog", "Position:")
                }
                SettingsComboBox {
                    options: root.editor.panelPositions
                    value: root.editor.general.panelPosition
                    onPicked: value => root.editor.general.panelPosition = value
                }
            }
        }
    }

    SettingsSection {
        title: qsTranslate("SettingsDialog", "Folder navigation")

        SettingsRadioRow {
            label: qsTranslate("SettingsDialog", "After reaching the end:")
            vertical: true
            options: root.editor.folderEndActions
            value: root.editor.general.folderEndAction
            onPicked: value => root.editor.general.folderEndAction = value
        }
        RowLayout {
            spacing: StyleConstants.dialogSpacing

            Label {
                text: qsTranslate("SettingsDialog", "Default sorting mode:")
            }
            SettingsComboBox {
                options: root.editor.sortingModes
                value: root.editor.general.sortingMode
                onPicked: value => root.editor.general.sortingMode = value
            }
        }
        CheckBox {
            text: qsTranslate("SettingsDialog", "Apply sorting to folders")
            checked: root.editor.general.sortFolders
            onToggled: root.editor.general.sortFolders = checked
        }
        CheckBox {
            text: qsTranslate("SettingsDialog", "Show hidden files")
            checked: root.editor.general.showHiddenFiles
            onToggled: root.editor.general.showHiddenFiles = checked
        }
    }

    SettingsSection {
        title: qsTranslate("SettingsDialog", "Slideshow")

        RowLayout {
            spacing: StyleConstants.dialogSpacing

            Label {
                text: qsTranslate("SettingsDialog", "Switch interval:")
            }
            SpinBox {
                id: intervalBox

                from: root.editor.ranges.slideshowInterval.from
                to: root.editor.ranges.slideshowInterval.to
                value: root.editor.general.slideshowIntervalMs
                editable: true
                textFromValue: value => value + qsTranslate("SettingsDialog", "ms")
                valueFromText: text => parseInt(text, 10)
                onValueModified: root.editor.general.slideshowIntervalMs = intervalBox.value
            }
            CheckBox {
                text: qsTranslate("SettingsDialog", "Loop slideshow")
                checked: root.editor.general.loopSlideshow
                onToggled: root.editor.general.loopSlideshow = checked
            }
        }
    }
}
