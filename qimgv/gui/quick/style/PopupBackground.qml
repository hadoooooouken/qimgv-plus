import QtQuick
import QtQuick.Effects
import qimgv.bridges

// Frame of popups, menus and tooltips: the widget surface with its border
// and a GPU drop shadow.
Item {
    property real radius: StyleConstants.popupRadius
    property color color: Theme.colors.widget
    property color borderColor: Theme.colors.widgetBorder
    property int borderWidth: StyleConstants.separatorThickness

    RectangularShadow {
        anchors.fill: parent
        radius: parent.radius
        blur: StyleConstants.shadowBlur
        offset.y: StyleConstants.shadowOffsetY
        color: StyleConstants.shadowColor
    }

    Rectangle {
        anchors.fill: parent
        radius: parent.radius
        color: parent.color
        border.color: parent.borderColor
        border.width: parent.borderWidth
    }
}
