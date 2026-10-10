import QtQuick
import QtQuick.Layouts
import qimgv.bridges
import qimgv.style

// One page of the settings window: its title above a scrolling column of
// sections, as the pages of the widget dialog.
ScrollView {
    id: root

    default property alias content: column.data
    required property string title

    readonly property int titleSpacing: 12
    readonly property int sectionSpacing: 18

    contentWidth: availableWidth
    clip: true

    ColumnLayout {
        id: column

        width: root.availableWidth
        spacing: root.sectionSpacing

        Label {
            Layout.fillWidth: true
            Layout.bottomMargin: root.titleSpacing - root.sectionSpacing
            text: root.title
            font: Theme.fonts.large
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: StyleConstants.separatorThickness
            color: Theme.colors.widgetBorder
        }
    }
}
