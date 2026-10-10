import QtQuick
import QtQuick.Layouts
import qimgv.bridges
import qimgv.render
import qimgv.style

// One file of the batch converter's queue (BatchItemWidget in the widget
// dialog): the check box selecting it for the batch, its thumbnail, name and
// source info on the left, its state and what happened to it on the right.
Item {
    id: root

    required property int index
    required property string name
    required property string sourceInfo
    required property bool checked
    required property int itemState
    required property string stateText
    required property string details
    required property thumbnailHandle thumbnail

    required property BatchQueueModel queue
    required property int thumbnailSize

    readonly property int margin: 6
    readonly property int spacing: 10
    readonly property int textSpacing: 2
    readonly property int thumbnailRadius: 6
    readonly property int nameFontSize: 12
    readonly property int infoFontSize: 10
    readonly property int stateMinimumWidth: 80

    readonly property color stateColor: {
        switch (root.itemState) {
        case BatchQueueModel.Failed:
        case BatchQueueModel.Stopped:
            return Theme.colors.statusError;
        case BatchQueueModel.Processing:
            return Theme.colors.statusProcessing;
        case BatchQueueModel.Done:
            return Theme.colors.statusSuccess;
        default:
            return Theme.colors.statusPending;
        }
    }

    implicitHeight: row.implicitHeight + 2 * root.margin

    RowLayout {
        id: row

        anchors.fill: parent
        anchors.margins: root.margin
        spacing: root.spacing

        CheckBox {
            checked: root.checked
            onToggled: root.queue.setChecked(root.index, checked)
        }

        Rectangle {
            Layout.preferredWidth: root.thumbnailSize
            Layout.preferredHeight: root.thumbnailSize
            radius: root.thumbnailRadius
            color: Theme.colors.widget
            border.color: Theme.colors.widgetBorder

            ThumbnailItem {
                anchors.fill: parent
                thumbnail: root.thumbnail
                maximumSize: Qt.size(root.thumbnailSize, root.thumbnailSize)
                cornerRadius: root.thumbnailRadius
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: root.textSpacing

            Text {
                Layout.fillWidth: true
                text: root.name
                elide: Text.ElideRight
                color: Theme.colors.textHc
                font.bold: true
                font.pixelSize: root.nameFontSize
            }
            Text {
                Layout.fillWidth: true
                text: root.sourceInfo
                elide: Text.ElideRight
                color: Theme.colors.textLc
                font.pixelSize: root.infoFontSize
            }
        }

        ColumnLayout {
            Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
            Layout.maximumWidth: row.width / 2
            spacing: root.textSpacing

            Text {
                Layout.alignment: Qt.AlignRight
                Layout.minimumWidth: root.stateMinimumWidth
                horizontalAlignment: Text.AlignRight
                text: root.stateText
                color: root.stateColor
                font.bold: true
                font.pixelSize: root.nameFontSize
            }
            TextEdit {
                Layout.alignment: Qt.AlignRight
                Layout.fillWidth: true
                visible: root.details.length > 0
                horizontalAlignment: Text.AlignRight
                text: root.details
                readOnly: true
                selectByMouse: true
                wrapMode: Text.Wrap
                color: Theme.colors.textLc
                font.pixelSize: root.infoFontSize
            }
        }
    }
}
