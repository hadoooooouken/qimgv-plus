pragma ComponentBehavior: Bound

import QtQuick
import qimgv.bridges
import qimgv.render
import qimgv.style

// One cell of the thumbnail strip, drawn like the widget ThumbnailWidget:
// hover and selection surfaces, the thumbnail (ThumbnailItem, with rounded
// corners and the hover highlight), the clock glyph while it loads or when
// it is unavailable, the name and info labels of the extended style, and the
// drop target frame. Geometry comes from the strip layout; the strip tells
// the cell whether the pointer is over it.
Item {
    id: cell

    // Model roles.
    required property int index
    required property string name
    required property string info
    required property thumbnailHandle thumbnail
    required property bool loaded
    required property bool unavailable
    required property bool selected
    required property bool dragHover
    // Set by the strip.
    required property thumbnailStripLayout stripLayout
    required property ThumbnailPanelController controller
    property int hoveredIndex: -1

    readonly property bool hovered: index === hoveredIndex
    // 0 - 1, follows hovered with the widget's fade in and out.
    property real hoverAmount: 0

    // ThumbnailWidget constants.
    readonly property real surfaceRadius: 8
    readonly property real thumbnailRadius: 4
    readonly property int frameWidth: 2
    readonly property real selectionFillOpacity: 0.40
    readonly property real selectionFrameOpacity: 0.70
    readonly property real hoverHighlight: 0.2
    readonly property real infoOpacity: 0.62
    readonly property int hoverInDuration: 100
    readonly property int hoverOutDuration: 60
    readonly property int statusIconSize: 72
    readonly property color dropHoverColor: Qt.rgba(190 / 255, 60 / 255, 25 / 255, 1)
    readonly property real dropHoverFillOpacity: 0.1

    readonly property bool hasImage: loaded && thumbnail.valid
    readonly property color labelColor: selected
        ? Theme.colors.thumbPanelText
        : controller.labelTextColor(hovered ? Theme.colors.thumbPanelHc : Theme.colors.thumbPanel)

    width: stripLayout.cellWidth
    height: stripLayout.cellHeight

    onHoveredChanged: {
        hoverAnimation.stop();
        hoverAnimation.to = hovered ? 1 : 0;
        hoverAnimation.duration = hovered ? hoverInDuration : hoverOutDuration;
        hoverAnimation.start();
    }
    ListView.onReused: {
        hoverAnimation.stop();
        hoverAmount = hovered ? 1 : 0;
    }

    NumberAnimation {
        id: hoverAnimation

        target: cell
        property: "hoverAmount"
        easing.type: Easing.OutQuad
    }

    // ThumbnailWidget fills its whole cell with the (translucent) panel
    // colour over the strip, which has the same background.
    Rectangle {
        anchors.fill: parent
        color: Theme.colors.thumbPanel
    }

    // The cell minus its margins carries the surfaces.
    Item {
        id: surface

        x: cell.stripLayout.marginX
        y: cell.stripLayout.marginY
        width: cell.width - 2 * cell.stripLayout.marginX
        height: cell.height - 2 * cell.stripLayout.marginY

        Rectangle {
            anchors.fill: parent
            radius: cell.surfaceRadius
            color: Theme.colors.thumbPanelHc
            opacity: cell.hoverAmount
            visible: !cell.selected && cell.hoverAmount > 0
        }

        Rectangle {
            anchors.fill: parent
            radius: cell.surfaceRadius
            color: Theme.colors.accent
            opacity: cell.selectionFillOpacity
            visible: cell.selected
        }

        Rectangle {
            anchors.fill: parent
            radius: cell.surfaceRadius
            color: "transparent"
            border.width: cell.frameWidth
            border.color: Theme.colors.accent
            opacity: cell.selectionFrameOpacity
            visible: cell.selected
        }
    }

    ThumbnailItem {
        y: cell.stripLayout.imageCenterOffset
        width: cell.width
        height: cell.height
        visible: cell.hasImage
        thumbnail: cell.thumbnail
        maximumSize: cell.stripLayout.imageArea
        cornerRadius: cell.thumbnailRadius
        highlight: cell.hoverHighlight * cell.hoverAmount
    }

    IconGlyph {
        anchors.centerIn: parent
        visible: !cell.hasImage
        icon: cell.unavailable || cell.loaded ? FluentIcons.ClockDismiss24 : FluentIcons.Clock24
        size: cell.statusIconSize
        color: cell.selected ? Theme.colors.accent : Theme.colors.thumbPanelHc2
    }

    Text {
        x: cell.stripLayout.labelLeft
        y: cell.stripLayout.labelTop
        width: cell.stripLayout.labelWidth
        height: cell.stripLayout.textHeight
        visible: cell.stripLayout.labels && cell.loaded
        text: cell.name
        color: cell.labelColor
        font: Theme.fonts.base
        elide: Text.ElideRight
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    Text {
        x: cell.stripLayout.labelLeft
        y: cell.stripLayout.infoTop
        width: cell.stripLayout.labelWidth
        height: cell.stripLayout.textHeight
        visible: cell.stripLayout.labels && cell.loaded
        text: cell.info
        color: cell.labelColor
        opacity: cell.infoOpacity
        font: Theme.fonts.base
        elide: Text.ElideRight
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    // Drop target.
    Item {
        anchors.fill: surface
        visible: cell.dragHover

        Rectangle {
            anchors.fill: parent
            radius: cell.surfaceRadius
            color: cell.dropHoverColor
            opacity: cell.dropHoverFillOpacity
        }

        Rectangle {
            anchors.fill: parent
            radius: cell.surfaceRadius
            color: "transparent"
            border.width: cell.frameWidth
            border.color: cell.dropHoverColor
        }
    }
}
