import QtQuick
import QtQuick.Layouts
import qimgv.style

// Settings page "Advanced": preloading, the thumbnailer and its cache,
// saving, confirmations, multiple instances and the memory limit.
SettingsPage {
    id: root

    required property SettingsEditorModel editor

    readonly property int sliderLabelWidth: 260
    readonly property int spinBoxWidth: 110

    title: qsTranslate("SettingsDialog", "Advanced")

    SettingsSection {
        CheckBox {
            text: qsTranslate("SettingsDialog", "Use preloader (recommended)")
            checked: root.editor.advanced.usePreloader
            onToggled: root.editor.advanced.usePreloader = checked
            ToolTip.text: qsTranslate("SettingsDialog", "Preload the next/previous image.\nResults in a much faster image switching (at the expense of wasting more RAM).")
            ToolTip.visible: hovered
        }
        SettingsNote {
            text: qsTranslate("SettingsDialog", "Load adjacent images in background")
        }
        SettingsSliderRow {
            label: qsTranslate("SettingsDialog", "Thumbnailer thread count:")
            labelWidth: root.sliderLabelWidth
            range: root.editor.ranges.thumbnailerThreads
            value: root.editor.advanced.thumbnailerThreads
            valueText: value.toString()
            onMoved: value => root.editor.advanced.thumbnailerThreads = value
        }
        CheckBox {
            text: qsTranslate("SettingsDialog", "Use thumbnail cache (recommended)")
            checked: root.editor.advanced.useThumbnailCache
            onToggled: root.editor.advanced.useThumbnailCache = checked
        }
        SettingsSliderRow {
            label: qsTranslate("SettingsDialog", "Thumbnail cache resolution:")
            labelWidth: root.sliderLabelWidth
            range: root.editor.ranges.thumbnailResolution
            value: root.editor.advanced.thumbnailResolution
            valueText: root.editor.thumbnailResolutionText(value)
            onMoved: value => root.editor.advanced.thumbnailResolution = value
        }
        RowLayout {
            spacing: StyleConstants.dialogSpacing

            Label {
                Layout.preferredWidth: root.sliderLabelWidth
                text: qsTranslate("SettingsDialog", "Thumbnail cache size limit:")
            }
            SpinBox {
                id: cacheSizeBox

                Layout.preferredWidth: root.spinBoxWidth
                from: root.editor.ranges.thumbnailCacheSize.from
                to: root.editor.ranges.thumbnailCacheSize.to
                stepSize: root.editor.ranges.thumbnailCacheSize.step
                value: root.editor.advanced.thumbnailCacheMaxSizeMB
                editable: true
                textFromValue: value => value === cacheSizeBox.from
                               ? qsTranslate("SettingsDialog", "Unlimited") : value + " MB"
                valueFromText: text => text === qsTranslate("SettingsDialog", "Unlimited")
                               ? cacheSizeBox.from : parseInt(text, 10)
                onValueModified: root.editor.advanced.thumbnailCacheMaxSizeMB = cacheSizeBox.value
            }
        }
        RowLayout {
            spacing: StyleConstants.dialogSpacing

            Label {
                Layout.preferredWidth: root.sliderLabelWidth
                text: qsTranslate("SettingsDialog", "Current cache size:")
            }
            Label {
                text: root.editor.thumbnailCacheSizeText
            }
            Button {
                text: qsTranslate("SettingsDialog", "Clear")
                onClicked: root.editor.clearThumbnailCache()
            }
        }
        Label {
            text: qsTranslate("SettingsDialog", "Exclude paths from caching (separated by semicolon ';'):")
        }
        TextField {
            Layout.fillWidth: true
            text: root.editor.advanced.excludedCachePaths
            onTextEdited: root.editor.advanced.excludedCachePaths = text
            ToolTip.text: qsTranslate("SettingsDialog", "Paths to folders that should not be cached, separated by ';'.\nExample: D:\\Downloads; E:\\Pictures")
            ToolTip.visible: hovered
        }
        CheckBox {
            text: qsTranslate("SettingsDialog", "Unload off-screen thumbnails")
            checked: root.editor.advanced.unloadThumbs
            onToggled: root.editor.advanced.unloadThumbs = checked
        }
        SettingsNote {
            text: qsTranslate("SettingsDialog", "Dynamically unload items to save memory")
        }
    }

    SettingsSection {
        CheckBox {
            text: qsTranslate("SettingsDialog", "Show save overlay when editing images")
            checked: root.editor.advanced.showSaveOverlay
            onToggled: root.editor.advanced.showSaveOverlay = checked
        }
        SettingsSliderRow {
            label: qsTranslate("SettingsDialog", "JPEG save quality:")
            labelWidth: root.sliderLabelWidth
            range: root.editor.ranges.quality
            value: root.editor.advanced.jpegQuality
            valueText: root.editor.percentText(value)
            onMoved: value => root.editor.advanced.jpegQuality = value
        }
        SettingsSliderRow {
            label: qsTranslate("SettingsDialog", "Modern formats quality (WebP, JXL, AVIF):")
            labelWidth: root.sliderLabelWidth
            range: root.editor.ranges.quality
            value: root.editor.advanced.modernQuality
            valueText: root.editor.percentText(value)
            onMoved: value => root.editor.advanced.modernQuality = value
        }
        SettingsSliderRow {
            label: qsTranslate("SettingsDialog", "PNG compression level:")
            labelWidth: root.sliderLabelWidth
            range: root.editor.ranges.pngCompression
            value: root.editor.advanced.pngCompression
            valueText: root.editor.pngCompressionText(value)
            onMoved: value => root.editor.advanced.pngCompression = value
        }
    }

    SettingsSection {
        CheckBox {
            text: qsTranslate("SettingsDialog", "Confirm moving to trash")
            checked: root.editor.advanced.confirmTrash
            onToggled: root.editor.advanced.confirmTrash = checked
        }
        CheckBox {
            text: qsTranslate("SettingsDialog", "Confirm file delete (!)")
            checked: root.editor.advanced.confirmDelete
            onToggled: root.editor.advanced.confirmDelete = checked
        }
        CheckBox {
            text: qsTranslate("SettingsDialog", "Allow multiple instances")
            checked: root.editor.advanced.multiInstance
            onToggled: root.editor.advanced.multiInstance = checked
        }
        RowLayout {
            spacing: StyleConstants.dialogSpacing

            Label {
                text: qsTranslate("SettingsDialog", "Memory allocation limit per image, MB:")
            }
            SpinBox {
                id: memoryBox

                Layout.preferredWidth: root.spinBoxWidth
                from: root.editor.ranges.memoryLimit.from
                to: root.editor.ranges.memoryLimit.to
                stepSize: root.editor.ranges.memoryLimit.step
                value: root.editor.advanced.memoryLimitMB
                editable: true
                onValueModified: root.editor.advanced.memoryLimitMB = memoryBox.value
            }
        }
    }
}
