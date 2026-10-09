import QtQuick
import qimgv.bridges
import qimgv.style

// Colour adjustment sliders with a live preview (ColorAdjustmentsOverlay in
// the widget UI). It opens at the pointer and can be dragged by its header.
// Compare shows the unadjusted image while held; Apply applies the
// adjustments to the image; Reset restores the defaults.
Item {
    id: root

    required property OverlayCoordinator coordinator
    readonly property ColorAdjustmentsEditor editor: coordinator.colorAdjustmentsEditor

    readonly property int panelWidth: 420
    readonly property int padding: 12
    readonly property int contentSpacing: 8
    readonly property int buttonSpacing: 6

    visible: coordinator.colorAdjustments.open

    Connections {
        target: root.coordinator.colorAdjustments
        function onOpenChanged() {
            if (root.coordinator.colorAdjustments.open)
                panel.resetPosition();
        }
    }

    OverlayPanel {
        id: panel

        icon: FluentIcons.Adjustments20
        title: qsTr("Color adjustments")
        boldTitle: true
        draggable: true
        width: root.panelWidth
        bottomPadding: root.padding
        placementX: Math.max(0, Math.min(root.coordinator.colorAdjustments.anchor.x, root.width - width))
        placementY: Math.max(0, Math.min(root.coordinator.colorAdjustments.anchor.y, root.height - height))
        onCloseClicked: root.coordinator.colorAdjustments.close()

        Column {
            x: root.padding
            topPadding: root.contentSpacing
            width: parent.width - 2 * root.padding
            spacing: root.contentSpacing

            AdjustmentSliders {
                width: parent.width
                sliders: root.editor
            }

            Row {
                spacing: root.buttonSpacing

                Button {
                    text: qsTr("Compare")
                    focusPolicy: Qt.NoFocus
                    onPressed: root.editor.setComparing(true)
                    onReleased: root.editor.setComparing(false)
                    onCanceled: root.editor.setComparing(false)
                }
                Button {
                    text: qsTr("Apply")
                    focusPolicy: Qt.NoFocus
                    onClicked: root.editor.apply()
                }
                Button {
                    text: qsTr("Reset")
                    focusPolicy: Qt.NoFocus
                    onClicked: root.editor.resetAll()
                }
            }
        }
    }
}
