pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import qimgv.bridges
import qimgv.style

// Labelled slider rows of an AdjustmentSliderModel (DraggableSliderOverlay's
// form): label, slider and value text. A double click on a slider restores
// its default value.
GridLayout {
    id: grid

    required property AdjustmentSliderModel sliders

    readonly property int valueTextMinimumWidth: 45
    readonly property int formRowSpacing: 10
    readonly property int formFieldSpacing: 6

    columns: 3
    rowSpacing: formRowSpacing
    columnSpacing: formFieldSpacing

    Repeater {
        model: grid.sliders

        delegate: Label {
            required property int index
            required property string label

            text: label
            color: Theme.colors.textHc
            Layout.row: index
            Layout.column: 0
        }
    }

    Repeater {
        model: grid.sliders

        delegate: Slider {
            id: slider

            required property int index
            required property var model

            from: slider.model.minimum
            to: slider.model.maximum
            value: slider.model.value
            stepSize: 1
            snapMode: Slider.SnapAlways
            focusPolicy: Qt.NoFocus
            Layout.row: index
            Layout.column: 1
            Layout.fillWidth: true

            onMoved: grid.sliders.setValue(slider.index, Math.round(slider.value))

            TapHandler {
                onDoubleTapped: grid.sliders.resetValue(slider.index)
            }
        }
    }

    Repeater {
        model: grid.sliders

        delegate: Label {
            required property int index
            required property string valueText

            text: valueText
            color: Theme.colors.textHc
            horizontalAlignment: Text.AlignRight
            Layout.row: index
            Layout.column: 2
            Layout.minimumWidth: grid.valueTextMinimumWidth
        }
    }
}
