import QtQuick
import qimgv.bridges
import qimgv.style

// Info bar in the top left corner in fullscreen (FullscreenInfoOverlay in
// the widget UI): position, file name and details on the translucent overlay
// colour. FullscreenChromeController decides when it shows; it fades out.
// It takes no input.
Item {
    id: root

    required property FullscreenChromeController chrome

    readonly property int maximumWidth: 600
    readonly property int horizontalPadding: 9
    readonly property int verticalPadding: 5
    readonly property int spacing: 12
    readonly property int cornerRadius: 8
    readonly property real backgroundOpacity: 0.9
    readonly property int fadeOutDuration: 230

    Rectangle {
        id: infoBar

        width: Math.min(row.implicitWidth + 2 * root.horizontalPadding, root.maximumWidth)
        height: row.implicitHeight + 2 * root.verticalPadding
        bottomRightRadius: root.cornerRadius
        color: Color.transparent(Theme.colors.overlay, root.backgroundOpacity)
        clip: row.implicitWidth + 2 * root.horizontalPadding > root.maximumWidth
        visible: opacity > 0

        states: State {
            name: "hidden"
            when: !root.chrome.infoBarShown
            PropertyChanges {
                infoBar.opacity: 0
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

        Row {
            id: row

            x: root.horizontalPadding
            anchors.verticalCenter: parent.verticalCenter
            spacing: root.spacing

            Label {
                text: root.chrome.positionText
                visible: text.length > 0
                color: Theme.colors.overlayText
            }
            Label {
                text: root.chrome.nameText
                color: Theme.colors.overlayText
            }
            Label {
                text: root.chrome.detailsText
                visible: text.length > 0
                color: Theme.colors.overlayText
            }
        }
    }
}
