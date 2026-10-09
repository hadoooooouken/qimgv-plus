import QtQuick
import QtQuick.Templates as T
import qimgv.bridges

// Tooltip (QToolTip in the widget stylesheet): high-contrast text on the
// widget surface with a thin border, rounded corners and a drop shadow.
// The 6.12 ToolTip.policy keeps its default (shown on hover).
T.ToolTip {
    id: control

    x: parent ? (parent.width - implicitWidth) / 2 : 0
    y: -implicitHeight - StyleConstants.toolTipOffset

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    margins: StyleConstants.toolTipMargins
    verticalPadding: StyleConstants.toolTipVerticalPadding
    horizontalPadding: StyleConstants.toolTipHorizontalPadding

    closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutsideParent | T.Popup.CloseOnReleaseOutsideParent

    contentItem: Text {
        text: control.text
        font: control.font
        wrapMode: Text.Wrap
        color: Theme.colors.textHc
    }

    background: PopupBackground {
        radius: Theme.metrics.tooltipBorderRadius
        borderWidth: Theme.metrics.tooltipBorderWidth
    }
}
