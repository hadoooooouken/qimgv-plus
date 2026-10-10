import QtQuick
import QtQuick.Layouts
import qimgv.style

// Settings page "About": the application description with its links.
SettingsPage {
    id: root

    required property SettingsEditorModel editor

    title: qsTranslate("SettingsDialog", "About qimgv-plus")

    Label {
        Layout.fillWidth: true
        text: root.editor.aboutText
        textFormat: Text.MarkdownText
        wrapMode: Text.WordWrap
        onLinkActivated: link => Qt.openUrlExternally(link)

        HoverHandler {
            cursorShape: parent.hoveredLink.length > 0 ? Qt.PointingHandCursor : Qt.ArrowCursor
        }
    }
}
