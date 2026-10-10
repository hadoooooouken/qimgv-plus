pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T
import qimgv.bridges
import qimgv.style

// The format filter of the folder view top bar (FormatFilterComboBox in the
// widget UI): a field showing the filter, accent coloured while it limits
// the formats, that opens a popup with "All formats" and the formats by
// category. FormatFilterModel decides; the check boxes only report clicks
// and show the model's state.
T.AbstractButton {
    id: control

    required property FormatFilterModel filter

    readonly property bool popupOpen: popup.opened
    readonly property bool limited: !filter.allFormats

    // FormatFilterComboBox constants.
    readonly property int textLeftPadding: 9
    readonly property int textIconSpacing: 4
    readonly property int iconRightPadding: 8
    readonly property int popupMargin: 10
    readonly property int popupSpacing: 8
    readonly property int formatColumnWidth: 128
    readonly property int formatColumns: 4
    readonly property int formatIndent: 20
    readonly property int popupTopGap: 10
    readonly property int controlRadius: 4
    readonly property int borderWidth: 2
    // Text and glyph on the accent field (Qt::white in the widget).
    readonly property color limitedContentColor: "white"
    readonly property font headerFont: Qt.font({
        family: Theme.fonts.base.family,
        pointSize: Theme.fonts.base.pointSize + 1,
        bold: true
    })

    readonly property real widestText: {
        let width = 0;
        for (const text of filter.displayTexts)
            width = Math.max(width, metrics.advanceWidth(text));
        return Math.ceil(width);
    }

    implicitWidth: textLeftPadding + widestText + textIconSpacing + Theme.compactIconSize
                   + iconRightPadding + 2 * borderWidth
    focusPolicy: Qt.NoFocus
    hoverEnabled: true
    onClicked: popup.opened ? popup.close() : popup.open()

    Accessible.role: Accessible.ComboBox
    Accessible.name: filter.displayText

    FontMetrics {
        id: metrics

        font: Theme.fonts.base
    }

    background: Rectangle {
        radius: control.controlRadius
        color: control.limited ? Theme.colors.accent
             : control.hovered || control.popupOpen ? Theme.colors.panelButtonHover
             : Theme.colors.panelButton
        border.width: control.borderWidth
        border.color: color
    }

    contentItem: Item {
        Text {
            x: control.textLeftPadding
            width: parent.width - x - glyph.width - control.textIconSpacing - control.iconRightPadding
            height: parent.height
            text: control.filter.displayText
            color: control.limited ? control.limitedContentColor : Theme.colors.textHc
            font: Theme.fonts.base
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }

        IconGlyph {
            id: glyph

            x: parent.width - width - control.iconRightPadding
            anchors.verticalCenter: parent.verticalCenter
            icon: FluentIcons.Checkmark16
            size: Theme.compactIconSize
            color: control.limited ? control.limitedContentColor : Theme.colors.icons
        }
    }

    T.Popup {
        id: popup

        y: control.height + control.popupTopGap
        popupType: T.Popup.Window
        padding: control.popupMargin
        closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutsideParent

        background: PopupBackground {
            radius: Theme.metrics.contextMenuBorderRadius
        }

        contentItem: Column {
            spacing: control.popupSpacing

            ModelCheckBox {
                text: control.filter.allFormatsLabel
                font: control.headerFont
                checked: control.filter.allFormats
                onClicked: control.filter.selectAllFormats()
            }

            Rectangle {
                width: parent.width
                height: 1
                color: Theme.colors.widgetBorder
            }

            Repeater {
                model: control.filter

                delegate: Column {
                    id: category

                    required property int index
                    required property string label
                    required property int checkState
                    required property var formats

                    spacing: control.popupSpacing

                    ModelCheckBox {
                        text: category.label
                        font: control.headerFont
                        tristate: true
                        checkState: category.checkState
                        onClicked: control.filter.setCategoryChecked(category.index,
                                                                     category.checkState !== Qt.Checked)
                    }

                    Grid {
                        leftPadding: control.formatIndent
                        columns: control.formatColumns
                        columnSpacing: control.popupSpacing
                        rowSpacing: control.popupSpacing

                        Repeater {
                            model: category.formats

                            delegate: ModelCheckBox {
                                required property var modelData

                                width: control.formatColumnWidth
                                text: modelData.label
                                checked: modelData.checked
                                onClicked: control.filter.setFormatChecked(modelData.index,
                                                                           !modelData.checked)
                            }
                        }
                    }
                }
            }
        }
    }

    // The model decides the state; a click does not toggle it by itself.
    component ModelCheckBox: CheckBox {
        id: box

        focusPolicy: Qt.NoFocus
        nextCheckState: function() {
            return box.checkState;
        }
    }
}
