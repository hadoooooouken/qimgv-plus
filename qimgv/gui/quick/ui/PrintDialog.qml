import QtQuick
import QtQuick.Dialogs as Dialogs
import QtQuick.Layouts
import qimgv.bridges
import qimgv.render
import qimgv.style

// Printing the current image (PrintDialog in the widget UI): the page
// preview on the left; the printer, page orientation, colour mode and fit to
// page on the right; Export PDF, Print and Cancel below. Without printers
// only the PDF export is offered. Export PDF asks for the file first and
// keeps the dialog open when that is cancelled. Every decision is the
// view-model's.
DialogWindow {
    id: root

    required property PrintDialogModel dialog

    readonly property int columnSpacing: 9
    readonly property int groupWidth: 130
    // Light window backgrounds get a frame around the white page.
    readonly property real pageFrameWindowValue: 0.45
    readonly property real pageFrameOpacity: 0.25
    readonly property int pageFrameWidth: 1
    readonly property color pageFrameColor: "black"

    title: qsTr("Print image")
    initialFocusItem: root.dialog.exportFirst ? exportButton : printButton
    visible: root.dialog.open

    onAcceptRequested: {
        if (root.dialog.printersAvailable)
            root.dialog.print();
        else
            exportButton.click();
    }
    onDismissRequested: root.dialog.reject()

    RowLayout {
        Layout.fillWidth: true
        spacing: root.columnSpacing

        ColumnLayout {
            Layout.alignment: Qt.AlignTop
            spacing: StyleConstants.dialogSpacing

            Item {
                Layout.preferredWidth: root.dialog.previewExtent
                Layout.preferredHeight: root.dialog.previewExtent

                ThumbnailItem {
                    id: previewItem

                    objectName: "printPreview"
                    anchors.fill: parent
                    thumbnail: root.dialog.preview
                    maximumSize: Qt.size(root.dialog.previewExtent, root.dialog.previewExtent)
                }

                Rectangle {
                    x: previewItem.paintedRect.x
                    y: previewItem.paintedRect.y
                    width: previewItem.paintedRect.width
                    height: previewItem.paintedRect.height
                    visible: Theme.colors.widget.hsvValue > root.pageFrameWindowValue
                    color: "transparent"
                    border.width: root.pageFrameWidth
                    border.color: root.pageFrameColor
                    opacity: root.pageFrameOpacity
                }
            }

            RowLayout {
                Layout.preferredWidth: root.dialog.previewExtent
                spacing: StyleConstants.dialogSpacing

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: StyleConstants.separatorThickness
                    color: Theme.colors.widgetBorder
                }
                Label {
                    text: qsTr("Preview")
                }
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: StyleConstants.separatorThickness
                    color: Theme.colors.widgetBorder
                }
            }
        }

        ColumnLayout {
            Layout.alignment: Qt.AlignTop
            spacing: StyleConstants.dialogSpacing

            RowLayout {
                spacing: StyleConstants.dialogSpacing

                Label {
                    text: qsTr("Printer:")
                }
                Label {
                    visible: !root.dialog.printersAvailable
                    text: qsTr("<No printers found>")
                    textFormat: Text.PlainText
                }
                ComboBox {
                    Layout.fillWidth: true
                    visible: root.dialog.printersAvailable
                    model: root.dialog.printers
                    currentIndex: root.dialog.printerIndex
                    onActivated: index => root.dialog.selectPrinter(index)
                }
            }

            RowLayout {
                spacing: root.columnSpacing

                ColumnLayout {
                    Layout.alignment: Qt.AlignTop
                    Layout.minimumWidth: root.groupWidth

                    Label {
                        text: qsTr("Page orientation:")
                    }
                    RadioButton {
                        text: qsTr("Portrait")
                        checkable: false
                        checked: !root.dialog.landscape
                        onClicked: root.dialog.setLandscape(false)
                    }
                    RadioButton {
                        text: qsTr("Landscape")
                        checkable: false
                        checked: root.dialog.landscape
                        onClicked: root.dialog.setLandscape(true)
                    }
                }

                Rectangle {
                    Layout.fillHeight: true
                    implicitWidth: StyleConstants.separatorThickness
                    color: Theme.colors.widgetBorder
                }

                ColumnLayout {
                    Layout.alignment: Qt.AlignTop
                    Layout.minimumWidth: root.groupWidth

                    Label {
                        text: qsTr("Color mode:")
                    }
                    RadioButton {
                        text: qsTr("Grayscale")
                        checkable: false
                        checked: !root.dialog.color
                        onClicked: root.dialog.setColor(false)
                    }
                    RadioButton {
                        text: qsTr("Color")
                        checkable: false
                        checked: root.dialog.color
                        onClicked: root.dialog.setColor(true)
                    }
                }
            }

            CheckBox {
                text: qsTr("Fit to page")
                checked: root.dialog.fitToPage
                onToggled: root.dialog.setFitToPage(checked)
            }

            Label {
                objectName: "printError"
                Layout.fillWidth: true
                visible: root.dialog.errorText.length > 0
                text: root.dialog.errorText
                color: Theme.colors.statusError
                wrapMode: Text.Wrap
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: StyleConstants.dialogButtonSpacing

                Button {
                    id: exportButton

                    objectName: "exportPdfButton"
                    text: qsTr("Export PDF")
                    onClicked: pdfDialog.open()
                }
                Item {
                    Layout.fillWidth: true
                }
                Button {
                    id: printButton

                    objectName: "printButton"
                    text: qsTr("Print")
                    enabled: root.dialog.printersAvailable
                    onClicked: root.dialog.print()
                }
                Button {
                    text: qsTr("Cancel")
                    onClicked: root.dialog.reject()
                }
            }
        }
    }

    Dialogs.FileDialog {
        id: pdfDialog

        title: qsTr("Choose pdf location")
        parentWindow: root
        fileMode: Dialogs.FileDialog.SaveFile
        nameFilters: ["*.pdf"]
        currentFile: root.dialog.pdfFileUrl
        onAccepted: root.dialog.exportPdf(pdfDialog.selectedFile)
    }
}
