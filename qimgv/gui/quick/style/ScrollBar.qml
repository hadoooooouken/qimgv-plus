import QtQuick
import QtQuick.Templates as T
import qimgv.bridges

// Scroll bar of the thumbnail strip and the folder view: a square handle in
// the scrollbar colour over a transparent track, shown while the content
// does not fit, with no arrow buttons.
T.ScrollBar {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    padding: 0
    visible: control.policy !== T.ScrollBar.AlwaysOff
    minimumSize: {
        const length = control.horizontal ? control.width : control.height
        return length > 0 ? Math.min(StyleConstants.scrollBarMinimumHandleLength / length, 1.0) : 0
    }

    contentItem: Rectangle {
        implicitWidth: StyleConstants.scrollBarThickness
        implicitHeight: StyleConstants.scrollBarThickness
        visible: control.policy === T.ScrollBar.AlwaysOn || control.size < 1.0
        color: control.hovered || control.pressed ? Theme.colors.scrollbarHover
                                                  : Theme.colors.scrollbar
    }
}
