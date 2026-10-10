import QtQuick
import QtQuick.Layouts
import qimgv.bridges
import qimgv.style

// Explanation under an option of the settings window, in the dimmed text
// colour.
Label {
    Layout.fillWidth: true
    color: Theme.colors.textLc
    font: Theme.fonts.compact
    textFormat: Text.PlainText
    wrapMode: Text.WordWrap
}
