import QtQuick
import QtQuick.Layouts
import qimgv.style

// One slider option of the settings window: its label, the slider over a
// SettingsRange and the value text next to it. The slider snaps to the
// range's step. moved() reports every value the person picks; released()
// the value when a drag ends.
RowLayout {
    id: root

    property string label
    required property settingsRange range
    required property int value
    property string valueText
    // Width of the label column, so that the sliders of a group line up.
    property int labelWidth: 0
    readonly property alias pressed: slider.pressed

    readonly property int sliderWidth: 190
    readonly property int valueTextWidth: 60

    signal moved(int value)
    signal released(int value)

    spacing: StyleConstants.dialogSpacing

    Label {
        Layout.preferredWidth: root.labelWidth > 0 ? root.labelWidth : implicitWidth
        visible: root.label.length > 0
        text: root.label
    }

    Slider {
        id: slider

        Layout.preferredWidth: root.sliderWidth
        from: root.range.from
        to: root.range.to
        stepSize: root.range.step
        snapMode: Slider.SnapAlways
        value: root.value
        onMoved: root.moved(Math.round(slider.value))
        onPressedChanged: {
            if (!slider.pressed)
                root.released(Math.round(slider.value));
        }
    }

    Label {
        Layout.preferredWidth: root.valueTextWidth
        text: root.valueText
    }

    Item {
        Layout.fillWidth: true
    }
}
