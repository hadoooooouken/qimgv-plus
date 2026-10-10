import QtQuick
import qimgv.style

// Combo box over a settings option list ({value, text} entries of
// SettingsEditorModel): shows the entry of `value` and reports the value
// the person picks.
ComboBox {
    id: root

    required property var options
    required property var value

    signal picked(var value)

    model: root.options.map(option => option.text)
    currentIndex: root.options.findIndex(option => option.value === root.value)
    onActivated: index => root.picked(root.options[index].value)
}
