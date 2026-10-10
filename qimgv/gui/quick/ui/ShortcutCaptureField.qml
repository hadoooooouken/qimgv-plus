import QtQuick
import qimgv.bridges
import qimgv.style

// Captures a shortcut (KeySequenceEdit in the widget UI): while focused,
// every key press, mouse button press or release and wheel turn over it is
// turned into shortcut text by the actions bridge and reported by
// captured(); input that forms no shortcut is ignored. Keys do not reach the
// dialog meanwhile, so Enter, Escape and Tab can be bound too.
FocusScope {
    id: root

    required property string text

    signal captured(string shortcut)

    implicitWidth: label.implicitWidth + 2 * StyleConstants.buttonHorizontalPadding
    implicitHeight: Theme.metrics.buttonHeight
    activeFocusOnTab: true

    function report(shortcut: string) {
        if (shortcut.length > 0)
            root.captured(shortcut);
    }

    Keys.onPressed: event => {
        event.accepted = true;
        root.report(Actions.shortcutText(event));
    }

    Rectangle {
        anchors.fill: parent
        radius: StyleConstants.controlRadius
        color: mouseArea.containsMouse ? Theme.colors.buttonHover : Theme.colors.button
        border.width: root.activeFocus ? StyleConstants.separatorThickness : 0
        border.color: Theme.colors.accent
    }

    Label {
        id: label

        anchors.centerIn: parent
        text: root.text
    }

    MouseArea {
        id: mouseArea

        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        hoverEnabled: true
        onPressed: mouse => {
            root.forceActiveFocus();
            root.report(Actions.mousePressShortcutText(mouse));
        }
        onReleased: mouse => root.report(Actions.mouseReleaseShortcutText(mouse))
        onWheel: wheel => root.report(Actions.wheelShortcutText(wheel))
    }
}
