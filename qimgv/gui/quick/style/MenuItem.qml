import QtQuick
import QtQuick.Controls.impl
import QtQuick.Templates as T
import qimgv.bridges

// Context menu row of the widget UI: label on the left, the action's
// shortcut on the right in the secondary text colour, a rounded accent
// highlight under the pointer.
T.MenuItem {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding,
                             implicitIndicatorHeight + topPadding + bottomPadding)

    verticalPadding: 0
    horizontalPadding: StyleConstants.menuItemHorizontalPadding
    spacing: StyleConstants.menuItemSpacing

    icon.width: Theme.compactIconSize
    icon.height: Theme.compactIconSize
    icon.color: Theme.colors.icons

    contentItem: Item {
        readonly property real indicatorPadding: control.checkable && control.indicator
                                                 ? control.indicator.width + control.spacing : 0
        readonly property real arrowPadding: control.subMenu && control.arrow
                                             ? control.arrow.width + control.spacing : 0
        readonly property string shortcutText: control.action ? ShortcutText.of(control.action.shortcut) : ""
        readonly property color secondaryColor: Color.transparent(Theme.colors.text,
                                                                  StyleConstants.secondaryTextOpacity)

        implicitWidth: indicatorPadding + label.implicitWidth
                       + (shortcutText ? StyleConstants.menuShortcutGap + shortcut.implicitWidth : 0)
                       + arrowPadding
        implicitHeight: Math.max(label.implicitHeight, shortcut.implicitHeight)
        LayoutMirroring.enabled: control.mirrored
        LayoutMirroring.childrenInherit: true

        IconLabel {
            id: label

            anchors.left: parent.left
            anchors.leftMargin: parent.indicatorPadding
            anchors.right: shortcut.visible ? shortcut.left : parent.right
            anchors.rightMargin: shortcut.visible ? StyleConstants.menuShortcutGap : parent.arrowPadding
            anchors.verticalCenter: parent.verticalCenter

            spacing: control.spacing
            mirrored: control.mirrored
            display: control.display
            alignment: Qt.AlignLeft
            icon: control.icon
            text: control.text
            font: control.font
            color: control.enabled ? Theme.colors.textHc : parent.secondaryColor
        }

        Text {
            id: shortcut

            anchors.right: parent.right
            anchors.rightMargin: parent.arrowPadding
            anchors.verticalCenter: parent.verticalCenter
            visible: parent.shortcutText.length > 0

            text: parent.shortcutText
            font: control.font
            color: parent.secondaryColor
        }
    }

    indicator: IconGlyph {
        x: control.mirrored ? control.width - width - control.rightPadding : control.leftPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        visible: control.checked
        icon: FluentIcons.Checkmark16
        size: StyleConstants.indicatorSize
        opacity: control.enabled ? 1.0 : StyleConstants.disabledOpacity
    }

    arrow: IconGlyph {
        x: control.mirrored ? control.leftPadding : control.width - width - control.rightPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        visible: control.subMenu
        // ChevronDown turned to point into the submenu.
        rotation: control.mirrored ? StyleConstants.quarterTurn : -StyleConstants.quarterTurn
        icon: FluentIcons.ChevronDown20
        size: StyleConstants.chevronSize
    }

    background: Rectangle {
        implicitHeight: Theme.metrics.contextMenuItemHeight
        radius: StyleConstants.itemRadius
        color: control.highlighted || control.down
               ? Color.transparent(Theme.colors.accent, StyleConstants.accentHighlightOpacity)
               : "transparent"
    }
}
