import QtQuick
import qimgv.bridges
import qimgv.style

// Sharpening and contrast of the CAS filter (CasSettingsOverlay in the
// widget UI), applied to the viewer while the sliders move. It opens at the
// pointer and can be dragged by its header; Reset restores the defaults.
Item {
    id: root

    required property OverlayCoordinator coordinator
    readonly property CasSettingsEditor editor: coordinator.casSettingsEditor

    readonly property int panelWidth: 380
    readonly property int padding: 12
    readonly property int contentSpacing: 8

    visible: coordinator.casSettings.open

    Connections {
        target: root.coordinator.casSettings
        function onOpenChanged() {
            if (root.coordinator.casSettings.open)
                panel.resetPosition();
        }
    }

    OverlayPanel {
        id: panel

        icon: FluentIcons.Blur20
        title: qsTr("CAS Settings")
        boldTitle: true
        draggable: true
        width: root.panelWidth
        bottomPadding: root.padding
        placementX: Math.max(0, Math.min(root.coordinator.casSettings.anchor.x, root.width - width))
        placementY: Math.max(0, Math.min(root.coordinator.casSettings.anchor.y, root.height - height))
        onCloseClicked: root.coordinator.casSettings.close()

        Column {
            x: root.padding
            topPadding: root.contentSpacing
            width: parent.width - 2 * root.padding
            spacing: root.contentSpacing

            AdjustmentSliders {
                width: parent.width
                sliders: root.editor
            }

            Button {
                anchors.right: parent.right
                text: qsTr("Reset")
                focusPolicy: Qt.NoFocus
                onClicked: root.editor.resetAll()
            }
        }
    }
}
