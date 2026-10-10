import QtQuick
import QtQuick.Layouts
import qimgv.bridges
import qimgv.style

// A group of options on a settings page under its title; no title for an
// untitled group.
ColumnLayout {
    id: root

    default property alias content: body.data
    property string title

    Layout.fillWidth: true
    spacing: StyleConstants.dialogSpacing

    Label {
        Layout.fillWidth: true
        visible: root.title.length > 0
        text: root.title
        font: Theme.fonts.section
    }

    ColumnLayout {
        id: body

        Layout.fillWidth: true
        spacing: StyleConstants.dialogSpacing
    }
}
