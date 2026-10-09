pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T
import qimgv.bridges

// Combo box: the flat rounded field ProxyStyle draws for plain QComboBoxes,
// with a chevron glyph, and a list popup on the widget surface whose rows
// follow the mouse (highlightOnHover).
T.ComboBox {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding,
                             implicitIndicatorHeight + topPadding + bottomPadding)

    topPadding: StyleConstants.comboVerticalPadding
    bottomPadding: StyleConstants.comboVerticalPadding
    leftPadding: StyleConstants.comboHorizontalPadding
                 + (control.mirrored && indicator ? indicator.width + spacing : 0)
    rightPadding: StyleConstants.comboHorizontalPadding
                  + (!control.mirrored && indicator ? indicator.width + spacing : 0)
    spacing: StyleConstants.indicatorSpacing
    highlightOnHover: true

    delegate: ItemDelegate {
        required property var model
        required property int index

        width: ListView.view.width
        text: model[control.textRole]
        font.weight: control.currentIndex === index ? Font.DemiBold : Font.Normal
        highlighted: control.highlightedIndex === index
        hoverEnabled: control.hoverEnabled
    }

    indicator: IconGlyph {
        x: control.mirrored ? StyleConstants.comboHorizontalPadding
                            : control.width - width - StyleConstants.comboHorizontalPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        icon: FluentIcons.ChevronDown12
        size: StyleConstants.chevronSize
        opacity: control.enabled ? 1.0 : StyleConstants.disabledOpacity
    }

    contentItem: T.TextField {
        text: control.editable ? control.editText : control.displayText

        enabled: control.editable
        autoScroll: control.editable
        readOnly: control.down
        inputMethodHints: control.inputMethodHints
        validator: control.validator
        selectByMouse: control.selectTextByMouse

        font: control.font
        color: Theme.colors.textHc
        opacity: control.enabled ? 1.0 : StyleConstants.disabledOpacity
        selectionColor: Theme.colors.accent
        selectedTextColor: Theme.colors.textHc2
        verticalAlignment: Text.AlignVCenter
    }

    background: Rectangle {
        implicitWidth: StyleConstants.comboImplicitWidth
        implicitHeight: Theme.metrics.buttonHeight
        radius: StyleConstants.controlRadius
        color: control.down || control.popup.visible ? Theme.colors.comboFieldPressed
             : control.hovered && control.enabled ? Theme.colors.comboFieldHover
             : Theme.colors.comboField
        border.width: StyleConstants.comboBorderWidth
        border.color: control.enabled && (control.visualFocus || control.activeFocus)
                      ? Theme.colors.accent
                      : Color.transparent(Theme.colors.comboFieldBorder,
                                          control.enabled ? 1.0 : StyleConstants.disabledOpacity)
    }

    popup: T.Popup {
        y: control.height + StyleConstants.comboPopupGap
        width: control.width
        // The margins keep the list inside the window, shrinking it if needed.
        height: contentItem.implicitHeight + topPadding + bottomPadding
        topMargin: StyleConstants.popupListPadding
        bottomMargin: StyleConstants.popupListPadding
        padding: StyleConstants.popupListPadding
        font: control.font

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.delegateModel
            currentIndex: control.highlightedIndex
            highlightMoveDuration: 0

            T.ScrollBar.vertical: ScrollBar {}
        }

        background: PopupBackground {
            radius: Theme.metrics.contextMenuBorderRadius
        }
    }
}
