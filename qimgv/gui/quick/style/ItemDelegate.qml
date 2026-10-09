import QtQuick
import QtQuick.Controls.impl
import QtQuick.Templates as T
import qimgv.bridges

// Row of a combo box list or another popup list: a rounded accent highlight
// on the widget surface (QComboBox QAbstractItemView::item).
T.ItemDelegate {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding,
                             implicitIndicatorHeight + topPadding + bottomPadding)

    topPadding: StyleConstants.itemVerticalPadding
    bottomPadding: StyleConstants.itemVerticalPadding
    leftPadding: StyleConstants.itemHorizontalPadding
    rightPadding: StyleConstants.itemHorizontalPadding
    spacing: StyleConstants.indicatorSpacing

    icon.width: Theme.compactIconSize
    icon.height: Theme.compactIconSize
    icon.color: Theme.colors.icons

    contentItem: IconLabel {
        spacing: control.spacing
        mirrored: control.mirrored
        display: control.display
        alignment: control.display === IconLabel.IconOnly || control.display === IconLabel.TextUnderIcon
                   ? Qt.AlignCenter : Qt.AlignLeft
        opacity: control.enabled ? 1.0 : StyleConstants.disabledOpacity

        icon: control.icon
        text: control.text
        font: control.font
        color: Theme.colors.textHc
    }

    background: Rectangle {
        implicitHeight: StyleConstants.itemMinimumHeight
        radius: StyleConstants.itemRadius
        visible: control.down || control.highlighted || control.visualFocus
        color: Color.transparent(Theme.colors.accent, StyleConstants.accentHighlightOpacity)
    }
}
