pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T
import qimgv.bridges

// Search field (the folder view name filter): the TextField look with a
// clear button, and a suggestion popup like the ComboBox list. The widget
// filter has no search icon, so there is no search indicator.
T.SearchField {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding,
                             clearIndicator.implicitIndicatorHeight + topPadding + bottomPadding)

    readonly property bool __clearIndicatorVisible: control.clearIndicator.indicator
                                                    && control.clearIndicator.indicator.visible

    topPadding: StyleConstants.fieldVerticalPadding + StyleConstants.fieldBorderWidth
    bottomPadding: StyleConstants.fieldVerticalPadding + StyleConstants.fieldBorderWidth
    leftPadding: StyleConstants.fieldHorizontalPadding + StyleConstants.fieldBorderWidth
                 + (control.mirrored && __clearIndicatorVisible
                    ? control.clearIndicator.indicator.width + spacing : 0)
    rightPadding: StyleConstants.fieldHorizontalPadding + StyleConstants.fieldBorderWidth
                  + (!control.mirrored && __clearIndicatorVisible
                     ? control.clearIndicator.indicator.width + spacing : 0)
    spacing: StyleConstants.indicatorSpacing
    opacity: enabled ? 1.0 : StyleConstants.disabledOpacity

    delegate: ItemDelegate {
        required property var model
        required property int index

        width: ListView.view.width
        text: model[control.textRole]
        font.weight: control.currentIndex === index ? Font.DemiBold : Font.Normal
        highlighted: control.highlightedIndex === index
        hoverEnabled: control.hoverEnabled
    }

    searchIndicator.indicator: null

    clearIndicator.indicator: SpinStepIndicator {
        x: control.mirrored ? StyleConstants.fieldBorderWidth
                            : control.width - width - StyleConstants.fieldBorderWidth
        y: StyleConstants.fieldBorderWidth
        height: control.height - 2 * StyleConstants.fieldBorderWidth
        visible: control.text.length > 0
        icon: FluentIcons.Dismiss16
        pressed: control.clearIndicator.pressed
        hovered: control.clearIndicator.hovered
    }

    contentItem: T.TextField {
        text: control.text
        placeholderText: control.placeholderText
        placeholderTextColor: Color.transparent(Theme.colors.text, StyleConstants.secondaryTextOpacity)
        selectByMouse: control.selectTextByMouse

        font: control.font
        color: Theme.colors.textHc
        selectionColor: Theme.colors.accent
        selectedTextColor: Theme.colors.textHc2
        verticalAlignment: TextInput.AlignVCenter

        // The template draws no placeholder.
        Text {
            width: parent.width
            height: parent.height
            text: parent.placeholderText
            font: parent.font
            color: parent.placeholderTextColor
            verticalAlignment: parent.verticalAlignment
            visible: !parent.length && !parent.preeditText
                     && (!parent.activeFocus || parent.horizontalAlignment !== Qt.AlignHCenter)
            elide: Text.ElideRight
            renderType: parent.renderType
        }
    }

    background: Rectangle {
        implicitWidth: Theme.metrics.contextMenuWidth
        radius: StyleConstants.controlRadius
        color: control.enabled && (control.hovered || control.activeFocus || control.contentItem.activeFocus)
               ? Theme.colors.buttonHover : Theme.colors.button
        border.width: StyleConstants.fieldBorderWidth
        border.color: color
    }

    popup: T.Popup {
        y: control.height + StyleConstants.comboPopupGap
        width: control.width
        // The margins keep the list inside the window, shrinking it if needed.
        height: contentItem.implicitHeight + topPadding + bottomPadding
        topMargin: StyleConstants.popupListPadding
        bottomMargin: StyleConstants.popupListPadding
        padding: StyleConstants.popupListPadding

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
