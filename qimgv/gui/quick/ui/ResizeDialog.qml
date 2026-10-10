import QtQuick
import QtQuick.Layouts
import qimgv.style

// Image resize settings (ResizeDialog in the widget UI). Left: the size by
// percent or in pixels, the aspect ratio lock, the scaling filter and
// Upscayl with its model; right: common screen sizes, fit / fill the
// desktop and reset to the original size. Every decision is the
// view-model's; editing the percent or a side selects its mode.
DialogWindow {
    id: root

    required property ResizeDialogModel dialog

    readonly property int columnSpacing: 18
    readonly property int fieldWidth: 120
    readonly property int sideColumnWidth: 190
    readonly property int percentDecimals: 2

    title: qsTr("Resize")
    initialFocusItem: percentBox
    visible: root.dialog.open

    onAcceptRequested: root.dialog.accept()
    onDismissRequested: root.dialog.reject()

    RowLayout {
        Layout.fillWidth: true
        spacing: root.columnSpacing

        ColumnLayout {
            Layout.alignment: Qt.AlignTop
            spacing: StyleConstants.dialogSpacing

            RadioButton {
                text: qsTr("By Percent:")
                checked: root.dialog.byPercent
                onClicked: root.dialog.setByPercent(true)
            }

            GridLayout {
                columns: 2
                columnSpacing: StyleConstants.dialogSpacing

                Label {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignRight
                    text: qsTr("Percent:")
                }
                DoubleSpinBox {
                    id: percentBox

                    Layout.preferredWidth: root.fieldWidth
                    from: root.dialog.minimumPercent
                    to: root.dialog.maximumPercent
                    decimals: root.percentDecimals
                    value: root.dialog.percent
                    editable: true
                    onValueModified: root.dialog.setPercent(percentBox.value)
                }
            }

            RadioButton {
                text: qsTr("By Absolute Size:")
                checked: !root.dialog.byPercent
                onClicked: root.dialog.setByPercent(false)
            }

            GridLayout {
                columns: 2
                columnSpacing: StyleConstants.dialogSpacing

                Label {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignRight
                    text: qsTr("Width:")
                }
                SpinBox {
                    id: widthBox

                    Layout.preferredWidth: root.fieldWidth
                    from: root.dialog.minimumSide
                    to: root.dialog.maximumSide
                    value: root.dialog.targetWidth
                    editable: true
                    onValueModified: root.dialog.setTargetWidth(widthBox.value)
                }

                Label {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignRight
                    text: qsTr("Height:")
                }
                SpinBox {
                    id: heightBox

                    Layout.preferredWidth: root.fieldWidth
                    from: root.dialog.minimumSide
                    to: root.dialog.maximumSide
                    value: root.dialog.targetHeight
                    editable: true
                    onValueModified: root.dialog.setTargetHeight(heightBox.value)
                }
            }

            CheckBox {
                text: qsTr("Keep aspect ratio")
                checked: root.dialog.keepAspectRatio
                enabled: !root.dialog.byPercent
                onToggled: root.dialog.setKeepAspectRatio(checked)
            }

            GridLayout {
                columns: 2
                columnSpacing: StyleConstants.dialogSpacing

                Label {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignRight
                    text: qsTr("Filter:")
                }
                ComboBox {
                    Layout.fillWidth: true
                    // In the order of ResizeDialogModel.Filter.
                    model: [qsTr("Nearest"), qsTr("Bilinear"), qsTr("Smart sharpen"),
                        qsTr("Magic Kernel Sharp 2021")]
                    currentIndex: root.dialog.filterIndex
                    onActivated: index => root.dialog.setFilterIndex(index)
                }
            }

            // A disabled check box gets no hover, so the wrapper shows why
            // Upscayl is unavailable.
            Item {
                implicitWidth: upscaylBox.implicitWidth
                implicitHeight: upscaylBox.implicitHeight

                CheckBox {
                    id: upscaylBox

                    text: qsTr("Use Upscayl")
                    checked: root.dialog.useUpscayl
                    enabled: root.dialog.upscaylAvailable && root.dialog.upscaylApplies
                    onToggled: root.dialog.setUseUpscayl(checked)
                }

                HoverHandler {
                    id: upscaylHover
                }

                ToolTip.text: !root.dialog.upscaylAvailable
                              ? qsTr("No AI models found in models/ directory.")
                              : !root.dialog.upscaylApplies
                                ? qsTr("Use Upscayl only applies when the target size is larger than the original; it has no effect at this size and will be skipped.")
                                : ""
                ToolTip.visible: upscaylHover.hovered && ToolTip.text.length > 0
            }

            GridLayout {
                columns: 2
                columnSpacing: StyleConstants.dialogSpacing
                enabled: root.dialog.upscaylApplies && root.dialog.useUpscayl

                Label {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignRight
                    text: qsTr("Model:")
                }
                ComboBox {
                    Layout.fillWidth: true
                    model: root.dialog.upscaylModels
                    currentIndex: root.dialog.upscaylModelIndex
                    onActivated: index => root.dialog.setUpscaylModelIndex(index)
                }
            }
        }

        ColumnLayout {
            Layout.alignment: Qt.AlignTop
            Layout.preferredWidth: root.sideColumnWidth
            spacing: StyleConstants.dialogSpacing

            Label {
                text: qsTr("Common sizes:")
            }

            ComboBox {
                Layout.fillWidth: true
                model: [qsTr("Select:")].concat(root.dialog.commonSizes)
                // Row 0 is "Select:", the common sizes follow.
                currentIndex: root.dialog.commonSizeIndex + 1
                onActivated: index => root.dialog.selectCommonSize(index - 1)
            }

            Button {
                Layout.fillWidth: true
                text: qsTr("Fit to desktop")
                onClicked: root.dialog.fitDesktop()
            }

            Button {
                Layout.fillWidth: true
                text: qsTr("Fill desktop (expanding)")
                onClicked: root.dialog.fillDesktop()
            }

            Button {
                Layout.fillWidth: true
                text: qsTr("Reset:") + " " + root.dialog.originalWidth + " x " + root.dialog.originalHeight
                onClicked: root.dialog.reset()
            }
        }
    }

    DialogButtonBox {
        Layout.fillWidth: true
        Layout.topMargin: StyleConstants.dialogSpacing
        standardButtons: DialogButtonBox.Ok | DialogButtonBox.Cancel
        onAccepted: root.dialog.accept()
        onRejected: root.dialog.reject()
    }
}
