pragma ComponentBehavior: Bound

import QtQuick
import qimgv.bridges
import qimgv.style

// The panel at the right edge of the window (SidePanel in the widget UI),
// which shows the crop panel while the crop mode is active. It takes the
// keyboard focus (the width input) when it opens and the wheel, so neither
// reaches the viewer. The crop panel is created the first time the crop
// mode opens and kept.
FocusScope {
    id: root

    required property CropController crop

    readonly property int panelWidth: 250
    // The crop panel exists from the first opening on.
    property bool cropPanelCreated: false

    implicitWidth: panelWidth
    visible: crop.active

    onVisibleChanged: {
        if (!root.visible)
            return;
        root.cropPanelCreated = true;
        if (cropPanel.item)
            (cropPanel.item as CropPanel).focusInputs();
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.colors.widget

        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: StyleConstants.separatorThickness
            color: Theme.colors.widgetBorder
        }
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.NoButton
        onWheel: wheel => wheel.accepted = true
    }

    Loader {
        id: cropPanel

        anchors.fill: parent
        active: root.cropPanelCreated
        focus: true
        sourceComponent: CropPanel {
            controller: root.crop
        }

        onLoaded: (cropPanel.item as CropPanel).focusInputs()
    }
}
