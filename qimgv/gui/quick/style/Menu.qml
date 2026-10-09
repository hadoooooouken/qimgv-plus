import QtQuick
import QtQuick.Templates as T
import qimgv.bridges

// Menu with the context menu frame of the widget UI: the widget surface,
// a thin border, rounded corners and a drop shadow. Runs of separators
// collapse (Menu.separatorsCollapsible) like QMenu's.
T.Menu {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    margins: 0
    overlap: 1
    padding: StyleConstants.popupListPadding
    separatorsCollapsible: true

    delegate: MenuItem {}

    contentItem: ListView {
        implicitHeight: contentHeight
        model: control.contentModel
        interactive: Window.window
                     ? contentHeight + control.topPadding + control.bottomPadding > control.height
                     : false
        clip: true
        currentIndex: control.currentIndex

        T.ScrollBar.vertical: ScrollBar {}
    }

    background: PopupBackground {
        implicitWidth: Theme.metrics.contextMenuWidth
        radius: Theme.metrics.contextMenuBorderRadius
    }

    T.Overlay.modal: Rectangle {
        color: StyleConstants.modalDimColor
    }

    T.Overlay.modeless: Rectangle {
        color: StyleConstants.modelessDimColor
    }
}
