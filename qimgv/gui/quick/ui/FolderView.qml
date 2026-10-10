pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.impl
import QtQuick.Layouts
import QtQuick.Templates as T
import qimgv.bridges
import qimgv.style

// The folder view (FolderView in the widget UI): the top bar with the path,
// the selection count and batch conversion, the grid size slider, the
// folder and file sorting, the format and name filters and the window
// buttons; below it the places panel and the thumbnail grid behind a
// splitter. FolderViewController holds the state and the rules; this file
// lays the parts out. Its strings keep the widget view's translation
// context.
FocusScope {
    id: root

    required property FolderViewController controller

    readonly property FolderGridController gridController: controller.grid

    // FolderView::setupUi() sizes.
    readonly property int placesButtonWidth: 40
    readonly property int upButtonWidth: 34
    readonly property int windowButtonWidth: 38
    readonly property int captionSpacing: 7
    readonly property int topBarRightMargin: 5
    readonly property int controlMarginV: 4
    readonly property int controlMarginH: 2
    readonly property int placesButtonLeftMargin: 4
    readonly property int pathTextRightPadding: 9
    readonly property int pathBarMinimumWidth: 80
    readonly property int pathBarSpacing: 12
    readonly property int selectionCountMargin: 8
    readonly property int batchButtonMargin: 8
    readonly property int batchIconGap: 14
    readonly property int batchTextPadding: 8
    readonly property int batchIconPadding: 38
    readonly property int gridSizeLabelMargin: 6
    readonly property int sliderWidth: 110
    readonly property int sliderSpacing: 3
    readonly property int comboSpacing: 4
    readonly property int nameFilterWidth: 163
    readonly property int nameFilterSpacing: 8
    readonly property int controlRadius: 4
    readonly property int fieldBorderWidth: 2
    readonly property int buttonBorderWidth: 1
    readonly property int splitterHandleWidth: 6
    readonly property int contentsBorderWidth: 1
    // The top bar's border is a 14 % black line unless the top bar has the
    // folder view colour.
    readonly property real topBarBorderOpacity: 0.14

    readonly property list<string> sortingNames: [
        qsTr("A - Z"), qsTr("Z - A"), qsTr("Size"), qsTr("Size (desc)"), qsTr("Oldest"),
        qsTr("Newest")
    ]

    onWidthChanged: controller.setViewWidth(width)
    Component.onCompleted: controller.setViewWidth(width)

    Rectangle {
        anchors.fill: parent
        color: Theme.colors.folderView
    }

    Rectangle {
        id: topBar

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: Theme.metrics.topPanelHeight
        color: Theme.colors.folderViewTopBar

        RowLayout {
            anchors.fill: parent
            anchors.rightMargin: root.topBarRightMargin
            spacing: 0

            TopBarButton {
                Layout.preferredWidth: root.placesButtonWidth
                Layout.leftMargin: root.placesButtonLeftMargin
                glyph: FluentIcons.PanelLeft20
                active: root.controller.placesPanelEnabled
                toolTip: qsTr("Toggle side panel")
                onPressed: root.controller.setPlacesPanelEnabled(!root.controller.placesPanelEnabled)
            }

            // Path bar: the up button and the directory.
            Rectangle {
                Layout.fillHeight: true
                Layout.topMargin: root.controlMarginV
                Layout.bottomMargin: root.controlMarginV
                Layout.leftMargin: root.controlMarginH
                Layout.rightMargin: root.controlMarginH
                Layout.minimumWidth: root.pathBarMinimumWidth
                Layout.preferredWidth: root.upButtonWidth + pathText.implicitWidth
                radius: root.controlRadius
                color: Theme.colors.panelButton

                Row {
                    anchors.fill: parent

                    T.AbstractButton {
                        id: upButton

                        width: root.upButtonWidth
                        height: parent.height
                        focusPolicy: Qt.NoFocus
                        hoverEnabled: true
                        onClicked: Actions.invoke("goUp")

                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Go up")

                        background: Rectangle {
                            radius: root.controlRadius
                            color: upButton.down ? Theme.colors.panelButtonPressed
                                 : upButton.hovered ? Theme.colors.panelButtonHover
                                 : "transparent"
                        }
                        contentItem: IconGlyph {
                            icon: FluentIcons.ChevronUp16
                            size: Theme.compactIconSize
                        }
                    }

                    Text {
                        id: pathText

                        width: Math.max(0, parent.width - upButton.width)
                        height: parent.height
                        rightPadding: root.pathTextRightPadding
                        text: root.controller.directoryPath
                        color: Theme.colors.text
                        font: Theme.fonts.base
                        elide: Text.ElideMiddle
                        verticalAlignment: Text.AlignVCenter

                        HoverHandler {
                            cursorShape: Qt.IBeamCursor
                        }
                    }
                }
            }

            Item {
                Layout.fillWidth: true
                Layout.minimumWidth: root.controller.compactTopBar ? 0 : root.pathBarSpacing
            }

            Label {
                Layout.rightMargin: root.selectionCountMargin
                visible: root.gridController.selectedImageCount > 0
                text: root.gridController.selectedImageCount === 1
                      ? qsTr("1 image selected")
                      : qsTr("%1 images selected").arg(root.gridController.selectedImageCount)
                color: Theme.colors.textHc
            }

            T.AbstractButton {
                id: batchButton

                Layout.fillHeight: true
                Layout.topMargin: root.controlMarginV
                Layout.bottomMargin: root.controlMarginV
                Layout.leftMargin: root.controlMarginH
                Layout.rightMargin: root.batchButtonMargin
                implicitWidth: batchLabel.implicitWidth + root.batchTextPadding + root.batchIconPadding
                visible: root.gridController.selectedImageCount > 0
                focusPolicy: Qt.NoFocus
                hoverEnabled: true
                onClicked: root.gridController.requestBatchConversion()

                background: Rectangle {
                    radius: root.controlRadius
                    color: batchButton.down ? Theme.colors.panelButtonPressed
                         : batchButton.hovered ? Theme.colors.panelButtonHover
                         : Theme.colors.panelButton
                }
                contentItem: Item {
                    Label {
                        id: batchLabel

                        x: root.batchTextPadding
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("Batch convert")
                        color: Theme.colors.textHc
                    }
                    IconGlyph {
                        x: batchButton.width - width - root.batchIconGap
                        anchors.verticalCenter: parent.verticalCenter
                        icon: FluentIcons.BatchConvert16
                        size: Theme.compactIconSize
                    }
                }
            }

            Label {
                Layout.rightMargin: root.gridSizeLabelMargin
                visible: !root.controller.compactTopBar
                text: qsTr("Grid size")
                color: Theme.colors.textHc
            }

            Slider {
                Layout.preferredWidth: root.sliderWidth
                Layout.rightMargin: root.sliderSpacing
                visible: !root.controller.compactTopBar
                focusPolicy: Qt.NoFocus
                from: root.gridController.zoomSliderMinimum
                to: root.gridController.zoomSliderMaximum
                stepSize: 1
                value: root.gridController.zoomSliderValue
                onMoved: root.gridController.setZoomSliderValue(value)
                onPressedChanged: root.gridController.setZoomSliderPressed(pressed)
            }

            PanelComboBox {
                glyph: FluentIcons.Folder16
                model: root.sortingNames
                currentIndex: root.controller.folderSortingMode
                toolTip: qsTr("Folder icon sorting")
                onActivated: index => root.controller.selectFolderSorting(index)
            }

            PanelComboBox {
                Layout.leftMargin: root.comboSpacing
                glyph: FluentIcons.ArrowSort16
                model: root.sortingNames
                currentIndex: root.controller.sortingMode
                toolTip: qsTr("Sort folders and images")
                onActivated: index => root.controller.selectSorting(index)
            }

            FormatFilterComboBox {
                Layout.fillHeight: true
                Layout.topMargin: root.controlMarginV
                Layout.bottomMargin: root.controlMarginV
                Layout.leftMargin: root.comboSpacing + root.controlMarginH
                Layout.rightMargin: root.controlMarginH
                filter: root.controller.formatFilter

                ToolTip.visible: hovered && !popupOpen
                ToolTip.text: qsTr("Filter by file format")
            }

            SearchField {
                id: nameFilter

                Layout.preferredWidth: root.nameFilterWidth
                Layout.fillHeight: true
                Layout.topMargin: root.controlMarginV
                Layout.bottomMargin: root.controlMarginV
                Layout.leftMargin: root.comboSpacing + root.controlMarginH
                Layout.rightMargin: root.controlMarginH
                placeholderText: qsTr("Name filter")
                text: root.controller.nameFilter
                onTextEdited: root.controller.editNameFilter(text)

                ToolTip.visible: hovered && !activeFocus
                ToolTip.text: qsTr("Filter images by name")
            }

            TopBarButton {
                Layout.preferredWidth: root.windowButtonWidth
                Layout.leftMargin: root.nameFilterSpacing
                glyph: FluentIcons.DocumentView20
                toolTip: qsTr("Viewer")
                onPressed: Actions.invoke("documentView")
            }

            TopBarButton {
                Layout.preferredWidth: root.windowButtonWidth
                Layout.leftMargin: root.captionSpacing
                glyph: FluentIcons.Settings20
                toolTip: qsTr("Settings")
                onPressed: Actions.invoke("openSettings")
            }

            TopBarButton {
                Layout.preferredWidth: root.windowButtonWidth
                Layout.leftMargin: root.controller.fullscreen ? 0 : root.captionSpacing
                glyph: FluentIcons.ArrowExit20
                toolTip: qsTr("Quit qimgv-plus")
                onPressed: Actions.invoke("exit")
            }
        }
    }

    // FolderViewContents' top border.
    Rectangle {
        id: contentsBorder

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: topBar.bottom
        height: root.contentsBorderWidth
        color: Qt.colorEqual(Theme.colors.folderView, Theme.colors.folderViewTopBar)
               ? Theme.colors.folderView
               : Qt.rgba(0, 0, 0, root.topBarBorderOpacity)
    }

    SplitView {
        id: splitView

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: contentsBorder.bottom
        anchors.bottom: parent.bottom
        orientation: Qt.Horizontal

        onResizingChanged: {
            if (!resizing && placesPanel.visible)
                root.controller.resizePlacesPanel(placesPanel.width);
        }

        handle: Rectangle {
            implicitWidth: root.splitterHandleWidth
            color: Theme.colors.folderView
        }

        PlacesPanel {
            id: placesPanel

            SplitView.preferredWidth: root.controller.placesPanelWidth
            SplitView.minimumWidth: root.controller.placesPanelMinimumWidth
            visible: root.controller.placesPanelShown
            controller: root.controller
        }

        FolderGridView {
            SplitView.fillWidth: true
            controller: root.gridController
            focus: true
        }
    }

    // CheckableButtonLE / PanelButton of the widget stylesheet.
    component TopBarButton: T.AbstractButton {
        id: button

        required property int glyph
        property string toolTip
        // Checked look, not toggled by a press.
        property bool active: false

        Layout.fillHeight: true
        Layout.topMargin: root.controlMarginV
        Layout.bottomMargin: root.controlMarginV
        Layout.rightMargin: root.controlMarginH
        focusPolicy: Qt.NoFocus
        hoverEnabled: true

        ToolTip.visible: hovered && toolTip.length > 0
        ToolTip.text: toolTip
        Accessible.role: Accessible.Button
        Accessible.name: toolTip

        background: Rectangle {
            readonly property color surface: button.active || button.down ? Theme.colors.panelButton
                                           : button.hovered ? Theme.colors.panelButtonHover
                                           : "transparent"

            radius: root.controlRadius
            color: surface
            border.width: root.buttonBorderWidth
            border.color: surface
        }
        contentItem: IconGlyph {
            icon: button.glyph
        }
    }

    // PanelComboBox of the widget stylesheet: a field with an icon instead of
    // the chevron, as wide as its widest entry (QComboBox::sizeHint()).
    component PanelComboBox: ComboBox {
        id: combo

        required property int glyph
        property string toolTip
        readonly property real widestText: {
            let width = 0;
            for (let i = 0; i < count; ++i)
                width = Math.max(width, comboMetrics.advanceWidth(textAt(i)));
            return Math.ceil(width);
        }

        implicitWidth: leftPadding + widestText + rightPadding
        Layout.fillHeight: true
        Layout.topMargin: root.controlMarginV
        Layout.bottomMargin: root.controlMarginV
        Layout.leftMargin: root.controlMarginH
        Layout.rightMargin: root.controlMarginH
        focusPolicy: Qt.NoFocus

        ToolTip.visible: hovered && !popup.visible && toolTip.length > 0
        ToolTip.text: toolTip

        FontMetrics {
            id: comboMetrics

            font: combo.font
        }

        indicator: IconGlyph {
            x: combo.width - width - StyleConstants.comboHorizontalPadding
            y: combo.topPadding + (combo.availableHeight - height) / 2
            icon: combo.glyph
            size: Theme.compactIconSize
        }
        background: Rectangle {
            radius: root.controlRadius
            color: combo.hovered || combo.popup.visible ? Theme.colors.panelButtonHover
                                                        : Theme.colors.panelButton
            border.width: root.fieldBorderWidth
            border.color: color
        }
    }
}
