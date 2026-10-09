import QtQuick
import qimgv.bridges
import qimgv.style

// Transient notification at the bottom centre of the window (FloatingMessage
// in the widget UI): icon and bold text. It appears at once and fades out
// when its display time is over. It takes no input.
Item {
    id: root

    required property NotificationOverlayModel message

    readonly property int bottomMargin: 35
    readonly property int horizontalPadding: 12
    readonly property int verticalPadding: 11
    readonly property int spacing: 8
    readonly property int fadeOutDuration: 300

    Item {
        id: bubble

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: root.bottomMargin
        width: content.implicitWidth + 2 * root.horizontalPadding
        height: content.implicitHeight + 2 * root.verticalPadding
        visible: opacity > 0

        states: State {
            name: "hidden"
            when: !root.message.visible
            PropertyChanges {
                bubble.opacity: 0
            }
        }
        // Shows at once, fades out.
        transitions: Transition {
            to: "hidden"
            NumberAnimation {
                property: "opacity"
                duration: root.fadeOutDuration
            }
        }

        PopupBackground {
            anchors.fill: parent
        }

        Row {
            id: content

            anchors.centerIn: parent
            spacing: root.spacing

            IconGlyph {
                anchors.verticalCenter: parent.verticalCenter
                icon: root.message.icon
            }

            Label {
                anchors.verticalCenter: parent.verticalCenter
                text: root.message.text
                visible: text.length > 0
                color: Theme.colors.textHc
                font.bold: true
            }
        }
    }
}
