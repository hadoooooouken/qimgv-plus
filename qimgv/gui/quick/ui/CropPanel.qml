pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import qimgv.bridges
import qimgv.style

// The crop side panel (CropPanel in the widget UI): the selection in
// pixels, the aspect ratio presets and inputs, and the crop buttons.
// CropController decides everything; the inputs show its state and report
// edits. A right click on Crop or Crop & Save makes it the default action
// (Enter), which the marker in its corner shows. Keys the inputs do not use
// go on to the action shortcuts. Shown in the SidePanel.
FocusScope {
    id: root

    required property CropController controller

    readonly property int horizontalMargin: 18
    readonly property int topMargin: 12
    readonly property int contentSpacing: 8
    readonly property int headerIconSize: 48
    readonly property int headerBottomMargin: 4
    readonly property int labelSpacing: 10
    readonly property int aspectSpacing: 6
    readonly property int buttonsTopGap: 5
    readonly property int groupRadius: 4
    readonly property int groupTopPadding: 6
    readonly property int groupSidePadding: 10
    readonly property int groupBottomPadding: 8
    readonly property int maxPositionValue: 65535
    readonly property int aspectDecimals: 4
    readonly property real minimumAspect: 0.0001
    // QDoubleSpinBox's default maximum.
    readonly property real maximumAspect: 99.99
    // Default action marker (PushButtonFocusInd): a corner triangle.
    readonly property int markerInset: 4
    readonly property int markerSize: 9
    readonly property real markerRotation: 45

    // Shows the selection and the ratio of the controller in the inputs
    // (edited values were moved into the image).
    function showSelection() {
        const selection = root.controller.selection;
        widthBox.value = selection.width;
        heightBox.value = selection.height;
        xBox.value = selection.x;
        yBox.value = selection.y;
    }

    function showAspect() {
        presetBox.currentIndex = root.controller.aspectPreset;
        aspectWidthBox.value = root.controller.aspectWidth;
        aspectHeightBox.value = root.controller.aspectHeight;
    }

    function commitSelection() {
        root.controller.setSelectionValues(xBox.value, yBox.value, widthBox.value, heightBox.value);
    }

    // Gives the width input the focus (when the panel opens).
    function focusInputs() {
        widthBox.forceActiveFocus();
    }

    Keys.onPressed: event => event.accepted = Actions.handleKeyEvent(event)

    Component.onCompleted: {
        root.showSelection();
        root.showAspect();
    }

    Connections {
        target: root.controller

        function onSelectionChanged() {
            root.showSelection();
        }
        function onAspectChanged() {
            root.showAspect();
        }
    }

    ColumnLayout {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: root.horizontalMargin
        anchors.rightMargin: root.horizontalMargin
        anchors.topMargin: root.topMargin
        spacing: root.contentSpacing

        IconGlyph {
            Layout.bottomMargin: root.headerBottomMargin
            icon: FluentIcons.Crop48
            size: root.headerIconSize
        }

        PanelGroup {
            title: qsTr("Selection")

            // Labels in one column, so the inputs line up.
            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: root.labelSpacing
                rowSpacing: root.contentSpacing

                Label {
                    Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                    text: qsTr("Width")
                    color: Theme.colors.text
                }
                SpinBox {
                    id: widthBox
                    objectName: "widthInput"

                    Layout.fillWidth: true
                    from: 0
                    to: root.controller.imageSize.width
                    editable: true
                    up.indicator: null
                    down.indicator: null
                    onValueModified: root.commitSelection()
                }

                Label {
                    Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                    text: qsTr("Height")
                    color: Theme.colors.text
                }
                SpinBox {
                    id: heightBox
                    objectName: "heightInput"

                    Layout.fillWidth: true
                    from: 0
                    to: root.controller.imageSize.height
                    editable: true
                    up.indicator: null
                    down.indicator: null
                    onValueModified: root.commitSelection()
                }

                Rectangle {
                    Layout.columnSpan: 2
                    Layout.fillWidth: true
                    implicitHeight: StyleConstants.separatorThickness
                    color: Theme.colors.widgetBorder
                }

                Label {
                    Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                    text: qsTr("Pos_X")
                    color: Theme.colors.text
                }
                SpinBox {
                    id: xBox
                    objectName: "xInput"

                    Layout.fillWidth: true
                    from: 0
                    to: root.maxPositionValue
                    editable: true
                    up.indicator: null
                    down.indicator: null
                    onValueModified: root.commitSelection()
                }

                Label {
                    Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                    text: qsTr("Pos_Y")
                    color: Theme.colors.text
                }
                SpinBox {
                    id: yBox
                    objectName: "yInput"

                    Layout.fillWidth: true
                    from: 0
                    to: root.maxPositionValue
                    editable: true
                    up.indicator: null
                    down.indicator: null
                    onValueModified: root.commitSelection()
                }
            }
        }

        PanelGroup {
            title: qsTr("Aspect Ratio")

            ComboBox {
                id: presetBox
                objectName: "aspectPresetInput"

                Layout.fillWidth: true
                focusPolicy: Qt.StrongFocus
                // In the order of CropController.AspectPreset.
                model: [qsTr("Free"), qsTr("Custom"), qsTr("Current Image"), qsTr("This Screen"),
                        qsTr("1:1"), qsTr("4:3"), qsTr("16:9"), qsTr("16:10")]
                onActivated: index => root.controller.selectAspectPreset(index)
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: root.aspectSpacing

                DoubleSpinBox {
                    id: aspectWidthBox
                    objectName: "aspectWidthInput"

                    Layout.fillWidth: true
                    from: root.minimumAspect
                    to: root.maximumAspect
                    decimals: root.aspectDecimals
                    editable: true
                    up.indicator: null
                    down.indicator: null
                    onValueModified: root.controller.setCustomAspect(aspectWidthBox.value, aspectHeightBox.value)
                }

                Button {
                    flat: true
                    focusPolicy: Qt.NoFocus
                    text: qsTr("⇄")
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Swap aspect ratio")
                    onClicked: root.controller.swapAspect()
                }

                DoubleSpinBox {
                    id: aspectHeightBox
                    objectName: "aspectHeightInput"

                    Layout.fillWidth: true
                    from: root.minimumAspect
                    to: root.maximumAspect
                    decimals: root.aspectDecimals
                    editable: true
                    up.indicator: null
                    down.indicator: null
                    onValueModified: root.controller.setCustomAspect(aspectWidthBox.value, aspectHeightBox.value)
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: root.buttonsTopGap

            ActionButton {
                objectName: "cropButton"
                Layout.fillWidth: true
                text: qsTr("Crop")
                cropAction: SettingsEnums.CropAction.Crop
                onClicked: root.controller.crop()
            }

            ActionButton {
                objectName: "cropAndSaveButton"
                Layout.fillWidth: true
                text: qsTr("Crop && Save")
                cropAction: SettingsEnums.CropAction.CropAndSave
                onClicked: root.controller.cropAndSave()
            }
        }

        Button {
            Layout.fillWidth: true
            focusPolicy: Qt.NoFocus
            text: qsTr("Reset")
            onClicked: root.controller.reset()
        }

        Button {
            Layout.fillWidth: true
            focusPolicy: Qt.NoFocus
            text: qsTr("Cancel")
            onClicked: root.controller.cancel()
        }
    }

    // A titled group (QGroupBox of the side panel stylesheet).
    component PanelGroup: Rectangle {
        id: group

        property alias title: groupTitle.text
        default property alias content: groupContent.data

        Layout.fillWidth: true
        implicitHeight: groupContent.implicitHeight + root.groupTopPadding + root.groupBottomPadding
        color: Theme.colors.widget
        border.width: StyleConstants.separatorThickness
        border.color: Theme.colors.button
        radius: root.groupRadius

        ColumnLayout {
            id: groupContent

            anchors.fill: parent
            anchors.topMargin: root.groupTopPadding
            anchors.bottomMargin: root.groupBottomPadding
            anchors.leftMargin: root.groupSidePadding
            anchors.rightMargin: root.groupSidePadding
            spacing: root.contentSpacing

            Label {
                id: groupTitle

                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                color: Theme.colors.text
            }
        }
    }

    // Crop or Crop & Save: a right click makes it the default action.
    component ActionButton: Button {
        id: actionButton

        required property int cropAction
        readonly property bool isDefault: root.controller.defaultAction === cropAction

        focusPolicy: Qt.NoFocus

        Item {
            objectName: "defaultMarker"
            x: root.markerInset
            y: root.markerInset
            width: root.markerSize
            height: root.markerSize
            visible: actionButton.isDefault
            clip: true

            // A square turned by 45 degrees around the corner leaves the
            // triangle of PushButtonFocusInd inside the clip.
            Rectangle {
                width: root.markerSize * Math.SQRT2
                height: width
                x: -width / 2
                y: -height / 2
                rotation: root.markerRotation
                color: Theme.colors.widget
            }
        }

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.RightButton
            onPressed: root.controller.chooseDefaultAction(actionButton.cropAction)
        }
    }
}
