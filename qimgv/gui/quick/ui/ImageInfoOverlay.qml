pragma ComponentBehavior: Bound

import QtQuick
import qimgv.bridges
import qimgv.style

// Metadata of the current image at the right edge (ImageInfoOverlay in the
// widget UI): name and value rows, long values below their name; values are
// selectable. The panel can be dragged by its header and returns to its
// place when it opens again.
Item {
    id: root

    required property OverlayState overlay
    required property ImageInfoModel entries

    readonly property int panelWidth: 280
    readonly property int edgeMargin: 20
    readonly property int nameWidth: 120
    readonly property int valueWidth: 142
    readonly property int rowMinimumHeight: 30
    readonly property int rowPadding: 9
    readonly property int valuePadding: 3
    readonly property int listPadding: 2
    readonly property int stubHeight: 48
    readonly property int bottomPadding: 4

    visible: overlay.open

    Connections {
        target: root.overlay
        function onOpenChanged() {
            if (root.overlay.open)
                panel.resetPosition();
        }
    }

    OverlayPanel {
        id: panel

        icon: FluentIcons.Info20
        title: qsTr("Image info")
        draggable: true
        width: root.panelWidth
        bottomPadding: root.bottomPadding
        placementX: root.width - width - root.edgeMargin
        placementY: (root.height - height) / 2
        onCloseClicked: root.overlay.close()

        Item {
            width: parent.width
            height: root.listPadding
        }

        Label {
            width: parent.width
            height: root.stubHeight
            visible: root.entries.empty
            text: qsTr("<no metadata found>")
            textFormat: Text.PlainText
            color: Theme.colors.text
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        Repeater {
            model: root.entries

            delegate: Item {
                id: entry

                required property string name
                required property string value
                required property bool stacked

                width: parent.width
                height: Math.max(root.rowMinimumHeight, entryLayout.height)

                Rectangle {
                    anchors.fill: parent
                    visible: hover.hovered
                    color: Color.transparent(Theme.colors.accent, StyleConstants.accentHighlightOpacity)
                }

                HoverHandler {
                    id: hover
                }

                Flow {
                    id: entryLayout

                    x: root.rowPadding
                    width: parent.width - 2 * root.rowPadding

                    Label {
                        width: entry.stacked ? entryLayout.width : root.nameWidth
                        text: entry.name
                        color: Theme.colors.text
                    }

                    TextEdit {
                        width: entry.stacked ? entryLayout.width : root.valueWidth
                        leftPadding: root.valuePadding
                        text: entry.value
                        textFormat: TextEdit.PlainText
                        wrapMode: TextEdit.Wrap
                        readOnly: true
                        selectByMouse: true
                        activeFocusOnPress: false
                        color: Theme.colors.text
                        selectionColor: Theme.colors.accent
                        font: Theme.fonts.base
                    }
                }
            }
        }
    }
}
