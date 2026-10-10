import QtQuick
import QtQuick.Layouts
import qimgv.style

// Settings page "AI Upscale": Upscayl, its model, preloading and the zoom
// limit. Without installed models every option is off and disabled; the
// link to more models stays available.
SettingsPage {
    id: root

    required property SettingsEditorModel editor

    readonly property int modelBoxWidth: 180
    readonly property string modelsUrl: "https://github.com/upscayl/custom-models/tree/main/models"

    title: qsTranslate("SettingsDialog", "AI Upscale")

    SettingsSection {
        // A disabled check box gets no hover, so the wrapper shows why
        // Upscayl is unavailable.
        Item {
            implicitWidth: useBox.implicitWidth
            implicitHeight: useBox.implicitHeight

            CheckBox {
                id: useBox

                text: qsTranslate("SettingsDialog", "Use Upscayl")
                enabled: root.editor.upscaylAvailable
                checked: root.editor.upscale.useUpscayl
                onToggled: root.editor.upscale.useUpscayl = checked
            }

            HoverHandler {
                id: useHover
            }

            ToolTip.text: qsTranslate("SettingsDialog", "No AI models found in models/ directory.")
            ToolTip.visible: useHover.hovered && !root.editor.upscaylAvailable
        }

        RowLayout {
            spacing: StyleConstants.dialogSpacing

            Label {
                enabled: root.editor.upscaylOptionsEnabled
                text: qsTranslate("SettingsDialog", "Model:")
            }
            ComboBox {
                Layout.preferredWidth: root.modelBoxWidth
                enabled: root.editor.upscaylOptionsEnabled
                model: root.editor.upscaylModels
                currentIndex: root.editor.upscaylModels.indexOf(root.editor.upscale.model)
                onActivated: index => root.editor.upscale.model = root.editor.upscaylModels[index]
            }
            Label {
                text: "<a href=\"" + root.modelsUrl + "\">" + qsTranslate("SettingsDialog", "Get more models") + "</a>"
                textFormat: Text.StyledText
                onLinkActivated: link => Qt.openUrlExternally(link)

                HoverHandler {
                    cursorShape: Qt.PointingHandCursor
                }
            }
        }

        CheckBox {
            text: qsTranslate("SettingsDialog", "Load engine at startup and keep ready in video memory")
            enabled: root.editor.upscaylOptionsEnabled
            checked: root.editor.upscale.preloadUpscayl
            onToggled: root.editor.upscale.preloadUpscayl = checked
        }
        CheckBox {
            text: qsTranslate("SettingsDialog", "Enable upscaling only when zoom exceeds:")
            enabled: root.editor.upscaylOptionsEnabled
            checked: root.editor.upscale.limitEnabled
            onToggled: root.editor.upscale.limitEnabled = checked
        }
        SettingsSliderRow {
            Layout.leftMargin: StyleConstants.dialogPadding
            enabled: root.editor.upscaylLimitSliderEnabled
            range: root.editor.ranges.upscaylLimit
            value: root.editor.upscale.limitPercent
            valueText: root.editor.percentText(value)
            onMoved: value => root.editor.upscale.limitPercent = value
        }
    }
}
