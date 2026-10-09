import QtQuick
import qimgv.bridges

// One glyph of the application icon font (FluentIcons), centred in a square
// of size pixels: IconGlyph { icon: FluentIcons.Settings20 }.
Item {
    // Glyph to draw (a FluentIcons value).
    required property int icon
    // Edge of the glyph square, in pixels.
    property int size: Theme.standardIconSize
    property alias color: glyph.color
    readonly property alias text: glyph.text
    readonly property alias font: glyph.font

    implicitWidth: size
    implicitHeight: size

    Text {
        id: glyph

        anchors.fill: parent
        text: Theme.glyph(parent.icon)
        color: Theme.colors.icons
        font.family: Theme.iconFontFamily
        font.pixelSize: parent.size
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        renderType: Text.QtRendering
    }
}
