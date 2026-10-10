import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import qimgv.bridges
import qimgv.style

// Modal dialog window of the Qt Quick UI, the counterpart of an exec()'d
// QDialog: a separate application-modal window over the main window (the
// main window takes no input and cannot be closed meanwhile), centred on it
// when shown, as large as its content. The content is laid out in a column
// with the dialog margins. Enter and Return press the focused button (like
// the default button of a QDialog) and otherwise ask to accept; Escape and
// the window's close button ask to dismiss. The dialog decides through its
// view-model, which closes the window by its `open` state.
Window {
    id: root

    default property alias content: body.data
    // Narrowest content width (the minimum width of the widget dialog).
    property int minimumContentWidth: 0
    // The control that takes the keyboard focus when the dialog is shown;
    // none keeps it on the dialog itself (Enter accepts, Escape dismisses).
    property Item initialFocusItem: null

    signal acceptRequested
    signal dismissRequested

    flags: Qt.Dialog | Qt.CustomizeWindowHint | Qt.WindowTitleHint | Qt.WindowCloseButtonHint
    modality: Qt.ApplicationModal
    color: Theme.colors.widget

    // The window keeps the size of its content.
    readonly property int contentWidth: Math.max(root.minimumContentWidth, body.implicitWidth)
                                        + 2 * StyleConstants.dialogPadding
    readonly property int contentHeight: body.implicitHeight + 2 * StyleConstants.dialogPadding

    width: root.contentWidth
    height: root.contentHeight
    minimumWidth: root.contentWidth
    maximumWidth: root.contentWidth
    minimumHeight: root.contentHeight
    maximumHeight: root.contentHeight

    // Moves the keyboard focus out of the fields first: an edited spin box
    // commits its text only when it loses the focus or on the key release,
    // after the key press that accepts.
    function requestAccept() {
        const focusedButton = root.activeFocusItem as T.Button;
        if (focusedButton) {
            focusedButton.click();
            return;
        }
        body.forceActiveFocus();
        root.acceptRequested();
    }

    function focusInitialItem() {
        (root.initialFocusItem ?? keyScope).forceActiveFocus();
    }

    onVisibleChanged: {
        if (!root.visible)
            return;
        const owner = root.transientParent;
        if (owner) {
            root.x = owner.x + (owner.width - root.width) / 2;
            root.y = owner.y + (owner.height - root.height) / 2;
        }
        root.focusInitialItem();
        root.requestActivate();
    }

    // A window shown for the first time is activated after it was shown, and
    // the focus given before is lost then.
    onActiveChanged: {
        if (root.active && root.activeFocusItem === null)
            root.focusInitialItem();
    }

    onClosing: close => {
        close.accepted = false;
        root.dismissRequested();
    }

    // The keys reach the scope from the focused control when it leaves them
    // to its parents, as in a QDialog. Window shortcuts would not do: every
    // dialog window counts as active while its main window has a focused
    // dialog, so the keys of the other dialogs would match too.
    FocusScope {
        id: keyScope

        anchors.fill: parent
        focus: true

        Keys.onPressed: event => {
            if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                event.accepted = true;
                root.requestAccept();
            } else if (event.key === Qt.Key_Escape) {
                event.accepted = true;
                root.dismissRequested();
            }
        }

        ColumnLayout {
            id: body

            x: StyleConstants.dialogPadding
            y: StyleConstants.dialogPadding
            width: root.width - 2 * StyleConstants.dialogPadding
            spacing: StyleConstants.dialogSpacing
            focus: true
        }
    }
}
