pragma ComponentBehavior: Bound

import QtQuick
import qimgv.style

// The thumbnail strip of the panel (ThumbnailStrip in the widget UI): a
// ListView of ThumbnailWidget cells that reuses its delegates, with the
// scroll bar and its selection marker. The model decides what to load,
// select and where to scroll; the strip reports its viewport and the
// pointer, and runs the scroll animations the model asks for. It never takes
// the keyboard focus.
Item {
    id: strip

    required property ThumbnailPanelController controller

    readonly property ThumbnailListModel thumbnails: controller.model
    readonly property thumbnailStripLayout stripLayout: controller.layout
    readonly property bool horizontal: stripLayout.horizontal
    // Item under the pointer, -1 for none.
    property int hoveredIndex: -1

    // Selection marker in the scroll bar (ThumbnailStrip::
    // updateScrollbarIndicator()).
    readonly property int markerSize: 2
    readonly property int markerInset: 2
    readonly property real markerOpacity: 0.3

    function reportViewport() {
        thumbnails.setViewport(list.viewOffset, list.viewExtent);
    }

    function updateHoveredIndex() {
        hoveredIndex = pointerArea.containsMouse
            ? list.indexAt(pointerArea.mouseX + list.contentX, pointerArea.mouseY + list.contentY)
            : -1;
    }

    function scrollTo(offset: real, durationMs: int) {
        scrollAnimation.stop();
        if (durationMs <= 0) {
            if (horizontal)
                list.contentX = offset;
            else
                list.contentY = offset;
            return;
        }
        scrollAnimation.property = horizontal ? "contentX" : "contentY";
        scrollAnimation.to = offset;
        scrollAnimation.duration = durationMs;
        scrollAnimation.start();
    }

    onVisibleChanged: thumbnails.setActive(visible)
    Component.onCompleted: {
        reportViewport();
        thumbnails.setActive(visible);
    }
    Component.onDestruction: thumbnails.setActive(false)

    Connections {
        target: strip.thumbnails

        function onScrollRequested(offset: real, durationMs: int) {
            strip.scrollTo(offset, durationMs);
        }
    }

    NumberAnimation {
        id: scrollAnimation

        target: list
        easing.type: Easing.OutSine
        onFinished: strip.thumbnails.scrollAnimationFinished()
    }

    ListView {
        id: list

        readonly property real viewOffset: strip.horizontal ? contentX : contentY
        readonly property real viewExtent: strip.horizontal ? width : height

        anchors.fill: parent
        orientation: strip.horizontal ? ListView.Horizontal : ListView.Vertical
        model: strip.thumbnails
        interactive: false
        boundsBehavior: Flickable.StopAtBounds
        reuseItems: true
        clip: true

        ScrollBar.horizontal: strip.horizontal ? scrollBar : null
        ScrollBar.vertical: strip.horizontal ? null : scrollBar

        onViewOffsetChanged: {
            strip.reportViewport();
            strip.updateHoveredIndex();
        }
        onViewExtentChanged: strip.reportViewport()

        delegate: ThumbnailWidget {
            stripLayout: strip.stripLayout
            controller: strip.controller
            hoveredIndex: strip.hoveredIndex
        }
    }

    MouseArea {
        id: pointerArea

        function itemAt(mouse: MouseEvent): int {
            return list.indexAt(mouse.x + list.contentX, mouse.y + list.contentY);
        }

        anchors.fill: list
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.BackButton | Qt.ForwardButton

        onContainsMouseChanged: strip.updateHoveredIndex()
        onPressed: mouse => strip.thumbnails.press(itemAt(mouse), mouse.button, mouse.modifiers,
                                                   Qt.point(mouse.x, mouse.y))
        onPositionChanged: mouse => {
            strip.updateHoveredIndex();
            strip.thumbnails.move(Qt.point(mouse.x, mouse.y), mouse.buttons);
        }
        onReleased: mouse => strip.thumbnails.release(itemAt(mouse), mouse.button,
                                                      Qt.point(mouse.x, mouse.y))
        onDoubleClicked: mouse => strip.thumbnails.doubleClick(itemAt(mouse), mouse.button)
        onWheel: wheel => {
            strip.thumbnails.wheelScrolled(wheel.angleDelta, wheel.pixelDelta);
            wheel.accepted = true;
        }
    }

    ScrollBar {
        id: scrollBar

        readonly property real markerCenter: strip.thumbnails.count > 0
            ? (strip.thumbnails.currentIndex + 0.5) / strip.thumbnails.count : 0

        orientation: strip.horizontal ? Qt.Horizontal : Qt.Vertical
        policy: ScrollBar.AsNeeded
        anchors.left: strip.horizontal ? parent.left : undefined
        anchors.right: parent.right
        anchors.top: strip.horizontal ? undefined : parent.top
        anchors.bottom: parent.bottom

        Rectangle {
            visible: scrollBar.size < 1.0 && strip.thumbnails.currentIndex >= 0
            color: "gray"
            opacity: strip.markerOpacity
            x: strip.horizontal ? scrollBar.width * scrollBar.markerCenter - strip.markerSize
                                : strip.markerInset
            y: strip.horizontal ? strip.markerInset
                                : scrollBar.height * scrollBar.markerCenter - strip.markerSize
            width: strip.horizontal ? strip.markerSize : scrollBar.width - 2 * strip.markerInset
            height: strip.horizontal ? scrollBar.height - 2 * strip.markerInset : strip.markerSize
        }
    }
}
