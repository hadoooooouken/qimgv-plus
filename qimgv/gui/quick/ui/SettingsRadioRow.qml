pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import qimgv.style

// Radio buttons over a settings option list ({value, text, description}
// entries of SettingsEditorModel), after a label: the button of `value` is
// checked, picked() reports the value the person chooses. Vertical lists
// show each entry's description next to it.
GridLayout {
    id: root

    property string label
    required property var options
    required property var value
    property bool vertical: false

    signal picked(var value)

    readonly property int labelColumns: root.label.length > 0 ? 1 : 0

    // The label spans the rows of a vertical list.
    columns: (root.vertical ? 1 : root.options.length) + root.labelColumns
    columnSpacing: StyleConstants.dialogSpacing
    rowSpacing: StyleConstants.dialogSpacing

    Label {
        Layout.alignment: Qt.AlignTop
        Layout.rowSpan: root.vertical ? root.options.length : 1
        visible: root.label.length > 0
        text: root.label
    }

    Repeater {
        model: root.options

        delegate: RowLayout {
            id: entry

            required property var modelData

            spacing: StyleConstants.dialogSpacing

            // The buttons sit in separate rows, so they are not exclusive by
            // themselves: `checked` only follows `value`.
            RadioButton {
                text: entry.modelData.text
                checkable: false
                checked: entry.modelData.value === root.value
                onClicked: root.picked(entry.modelData.value)
            }

            SettingsNote {
                Layout.fillWidth: false
                visible: root.vertical && text.length > 0
                text: entry.modelData.description
            }
        }
    }
}
