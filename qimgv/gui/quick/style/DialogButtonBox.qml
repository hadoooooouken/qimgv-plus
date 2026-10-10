import QtQuick
import QtQuick.Templates as T

// Button row of the dialogs (the QDialogButtonBox row of the widget
// dialogs): push buttons at their natural width, right-aligned in the order
// of the platform, on the dialog's own surface. The default button (6.11
// defaultButton / defaultStandardButton) is highlighted and takes the focus.
T.DialogButtonBox {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)
    contentWidth: (contentItem as ListView)?.contentWidth

    spacing: StyleConstants.dialogButtonSpacing
    padding: 0
    alignment: Qt.AlignRight

    delegate: Button {}

    contentItem: ListView {
        implicitWidth: contentWidth
        model: control.contentModel
        spacing: control.spacing
        orientation: ListView.Horizontal
        boundsBehavior: Flickable.StopAtBounds
        snapMode: ListView.SnapToItem
    }
}
