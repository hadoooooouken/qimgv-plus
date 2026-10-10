pragma ComponentBehavior: Bound

import QtQuick
import qimgv.bridges
import qimgv.render
import qimgv.style

// One cell of the thumbnail strip or the folder grid, drawn like the widget
// ThumbnailWidget: hover and selection surfaces, the thumbnail
// (ThumbnailItem, with rounded corners and the hover highlight), the clock
// glyph while it loads or when it is unavailable, the name and info labels
// of the labelled styles, and the drop target frame. Geometry comes from the
// view's cell layout and the colours from the view's surface (the panel or
// the folder view); the view tells the cell whether the pointer is over it.
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
    // Set by the view.
    required property thumbnailStripLayout cellLayout
    required property int hoveredIndex
    // The view's surface and its hovered shade, and the label colours on
    // them while unselected and the label colour of a selected cell.
    required property color surfaceColor
    required property color hoverColor
    required property color labelOnSurface
    required property color labelOnHover
    required property color selectedLabelColor

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
    readonly property color labelColor: selected ? selectedLabelColor
                                                 : hovered ? labelOnHover : labelOnSurface

    width: cellLayout.cellWidth
    height: cellLayout.cellHeight

    onHoveredChanged: {
        hoverAnimation.stop();
        hoverAnimation.to = hovered ? 1 : 0;
        hoverAnimation.duration = hovered ? hoverInDuration : hoverOutDuration;
        hoverAnimation.start();
    }
    ListView.onReused: resetHover()
    GridView.onReused: resetHover()

    function resetHover() {
        hoverAnimation.stop();
        hoverAmount = hovered ? 1 : 0;
    }

    NumberAnimation {
        id: hoverAnimation

        target: cell
        property: "hoverAmount"
        easing.type: Easing.OutQuad
    }

    // ThumbnailWidget fills its whole cell with the (translucent) surface
    // colour over the view, which has the same background.
    Rectangle {
        anchors.fill: parent
        color: cell.surfaceColor
    }

    // The cell minus its margins carries the surfaces.
    Item {
        id: surface

        x: cell.cellLayout.marginX
        y: cell.cellLayout.marginY
        width: cell.width - 2 * cell.cellLayout.marginX
        height: cell.height - 2 * cell.cellLayout.marginY

        Rectangle {
            anchors.fill: parent
            radius: cell.surfaceRadius
            color: cell.hoverColor
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
        y: cell.cellLayout.imageCenterOffset
        width: cell.width
        height: cell.height
        visible: cell.hasImage
        thumbnail: cell.thumbnail
        maximumSize: cell.cellLayout.imageArea
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
        x: cell.cellLayout.labelLeft
        y: cell.cellLayout.labelTop
        width: cell.cellLayout.labelWidth
        height: cell.cellLayout.textHeight
        visible: cell.cellLayout.labels && cell.loaded
        text: cell.name
        color: cell.labelColor
        font: Theme.fonts.base
        elide: Text.ElideRight
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    Text {
        x: cell.cellLayout.labelLeft
        y: cell.cellLayout.infoTop
        width: cell.cellLayout.labelWidth
        height: cell.cellLayout.textHeight
        visible: cell.cellLayout.labels && cell.loaded
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
