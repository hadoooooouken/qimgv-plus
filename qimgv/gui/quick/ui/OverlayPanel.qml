import QtQuick
import QtQuick.Layouts
import qimgv.bridges
import qimgv.style

// Frame of the floating overlay panels (FloatingWidget with an overlay
// header in the widget UI): the widget surface with a header of icon, title
// and close button above the content. Pointer input over the panel does not
// reach the viewer behind it. A draggable panel moves by its header, inside
// its parent; resetPosition() returns it to its placement bindings.
Item {
    id: panel

    // A FluentIcons value.
    required property int icon
    required property string title
    property bool draggable: false
    property bool boldTitle: false
    // Space below the content.
    property int bottomPadding: 0
    // Placement while not dragged.
    property real placementX: 0
    property real placementY: 0
    default property alias content: body.data

    signal closeClicked()

    function resetPosition() {
        x = Qt.binding(() => panel.placementX);
        y = Qt.binding(() => panel.placementY);
    }

    x: placementX
    y: placementY
    implicitHeight: header.height + body.implicitHeight + bottomPadding

    PopupBackground {
        anchors.fill: parent
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        hoverEnabled: true
        onWheel: wheel => wheel.accepted = true
    }

    Item {
        id: header

        width: parent.width
        height: Theme.metrics.overlayHeaderSize

        RowLayout {
            anchors.fill: parent
            spacing: 0

            IconGlyph {
                icon: panel.icon
                Layout.preferredWidth: Theme.metrics.overlayHeaderSize
                Layout.fillHeight: true
            }

            Label {
                text: panel.title
                color: Theme.colors.textHc
                font.bold: panel.boldTitle
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            OverlayHeaderButton {
                glyph: FluentIcons.Dismiss16
                onClicked: panel.closeClicked()
            }
        }

        DragHandler {
            enabled: panel.draggable
            target: panel
            cursorShape: active ? Qt.ClosedHandCursor : Qt.OpenHandCursor
            xAxis.minimum: 0
            xAxis.maximum: panel.parent ? Math.max(0, panel.parent.width - panel.width) : 0
            yAxis.minimum: 0
            yAxis.maximum: panel.parent ? Math.max(0, panel.parent.height - panel.height) : 0
        }
    }

    Column {
        id: body

        anchors.top: header.bottom
        width: parent.width
    }
}
