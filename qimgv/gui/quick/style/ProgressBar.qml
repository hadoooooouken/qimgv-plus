import QtQuick
import QtQuick.Templates as T
import qimgv.bridges

// Progress bar (the batch converter's QProgressBar rules): a bordered bar in
// the folder view colour, filled with the accent colour, with the percentage
// centred on it.
T.ProgressBar {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    readonly property real percentPerUnit: 100

    padding: StyleConstants.progressBarBorderWidth

    contentItem: Item {
        implicitWidth: StyleConstants.sliderLength
        implicitHeight: percentText.implicitHeight + 2 * StyleConstants.progressBarTextPadding

        Rectangle {
            width: control.indeterminate ? parent.width : control.visualPosition * parent.width
            height: parent.height
            radius: StyleConstants.progressBarRadius
            color: Theme.colors.accent
            visible: control.indeterminate || control.position > 0
        }

        Text {
            id: percentText

            anchors.centerIn: parent
            text: Math.round(control.position * control.percentPerUnit) + "%"
            visible: !control.indeterminate
            color: Theme.colors.textHc2
            font: Theme.fonts.base
        }
    }

    background: Rectangle {
        implicitWidth: StyleConstants.sliderLength
        radius: StyleConstants.progressBarRadius
        color: Theme.colors.folderView
        border.width: StyleConstants.progressBarBorderWidth
        border.color: Theme.colors.widgetBorder
    }
}
