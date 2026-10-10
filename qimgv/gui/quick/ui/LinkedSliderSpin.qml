import QtQuick
import QtQuick.Layouts
import qimgv.style

// One colour adjustment of the batch converter (LinkedSliderSpin in the
// widget dialog): its label, a slider in `step` units and a spin box with
// the value and its suffix, both editing the same value. Double-clicking
// the slider restores the default value.
RowLayout {
    id: root

    // A BatchConverterDialogModel.colorSliders entry.
    required property var spec
    required property real value

    readonly property int labelWidth: 80
    readonly property int spinBoxWidth: 80
    // Step of the spin box arrows for values with decimals.
    readonly property real fineStep: 0.1
    readonly property real coarseStep: 1

    signal edited(real value)

    spacing: StyleConstants.dialogSpacing

    Label {
        Layout.preferredWidth: root.labelWidth
        text: root.spec.label
    }

    Slider {
        id: slider

        Layout.fillWidth: true
        from: root.spec.minimum
        to: root.spec.maximum
        stepSize: root.spec.step
        snapMode: Slider.SnapAlways
        value: root.value
        onMoved: root.edited(slider.value)

        TapHandler {
            onDoubleTapped: root.edited(root.spec.defaultValue)
        }
    }

    DoubleSpinBox {
        id: spinBox

        Layout.preferredWidth: root.spinBoxWidth
        from: root.spec.minimum
        to: root.spec.maximum
        decimals: root.spec.decimals
        stepSize: root.spec.decimals > 0 ? root.fineStep : root.coarseStep
        value: root.value
        editable: true
        onValueModified: root.edited(spinBox.value)
    }

    Label {
        text: root.spec.suffix
        visible: root.spec.suffix.length > 0
    }
}
