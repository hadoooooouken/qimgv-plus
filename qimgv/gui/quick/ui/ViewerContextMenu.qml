pragma ComponentBehavior: Bound

import QtQuick
import QtQml.Models
import QtQuick.Templates as T
import qimgv.bridges
import qimgv.style

// The viewer's context menu (ContextMenu in the widget UI): a row of zoom
// buttons and a row of transform buttons, then the action rows with their
// shortcuts, "More" expanding the rarely used rows in place and the
// "Open with..." submenu of scripts. ContextMenuModel holds the rows and
// decides when the menu opens; the menu is its own window, so it can extend
// past the main window's edges, and opens at the pointer. Named apart from
// the ContextMenu attached type of Qt Quick Controls; its strings keep the
// widget menu's translation context.
Menu {
    id: root

    required property ContextMenuModel menuModel

    // Items declared before the instantiated rows: the button rows and the
    // separator below them.
    readonly property int fixedItemCount: 2
    readonly property int buttonRowSpacing: 7
    readonly property int buttonRowInset: 4
    readonly property int expanderIconOffset: 2

    readonly property real widestRow: {
        let width = buttonRows.implicitWidth;
        for (let i = 0; i < rows.count; ++i) {
            const row = rows.objectAt(i) as Item;
            if (row)
                width = Math.max(width, row.implicitWidth);
        }
        return width;
    }

    popupType: T.Popup.Window
    // All rows laid out at once: the style's ListView only lays out the rows
    // in view and estimates the rest, which leaves the window too short
    // once "More" expands. Hidden rows take no space in a Column.
    contentItem: Column {
        Repeater {
            model: root.contentModel
        }
    }
    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            widestRow + leftPadding + rightPadding)

    // The scripts submenu's own row in this menu.
    delegate: MenuItem {
        id: submenuRow

        readonly property ScriptsMenu scripts: submenuRow.subMenu as ScriptsMenu

        hasGlyph: scripts !== null
        glyph: scripts ? scripts.glyph : FluentIcons.OpenWith20
        enabled: scripts ? scripts.available : true
        visible: scripts ? scripts.shown : true
        height: visible ? implicitHeight : 0
    }

    onClosed: root.menuModel.close()
    // Created by the first opening, after the model opened.
    Component.onCompleted: {
        if (root.menuModel.open)
            root.popup();
    }

    Connections {
        target: root.menuModel

        function onOpenChanged() {
            if (root.menuModel.open)
                root.popup();
            else
                root.dismiss();
        }
    }

    // Zoom and transform buttons; they act on press and close the menu.
    Column {
        id: buttonRows

        width: root.availableWidth
        spacing: root.buttonRowSpacing
        bottomPadding: root.buttonRowSpacing

        Row {
            leftPadding: root.buttonRowInset

            Repeater {
                model: root.menuModel.zoomButtons
                delegate: MenuButton {}
            }
        }

        Row {
            leftPadding: root.buttonRowInset

            Repeater {
                model: root.menuModel.transformButtons
                delegate: MenuButton {}
            }
        }
    }

    MenuSeparator {}

    Instantiator {
        id: rows

        model: root.menuModel.items
        delegate: DelegateChooser {
            role: "entryKind"

            DelegateChoice {
                roleValue: ContextMenuModel.Action
                ActionRow {}
            }
            DelegateChoice {
                roleValue: ContextMenuModel.Separator
                SeparatorRow {}
            }
            DelegateChoice {
                roleValue: ContextMenuModel.Expander
                ExpanderRow {}
            }
            DelegateChoice {
                roleValue: ContextMenuModel.Submenu
                ScriptsMenu {}
            }
        }

        onObjectAdded: (index, object) => {
            if (object instanceof ScriptsMenu)
                root.insertMenu(root.fixedItemCount + index, object as ScriptsMenu);
            else
                root.insertItem(root.fixedItemCount + index, object as Item);
        }
        onObjectRemoved: (index, object) => {
            if (object instanceof ScriptsMenu)
                root.removeMenu(object as ScriptsMenu);
            else
                root.removeItem(object as Item);
        }
    }

    component MenuButton: ToolButton {
        id: button

        required property string entryAction
        required property string entryText
        required property int entryIcon
        required property bool entryEnabled

        enabled: entryEnabled
        focusPolicy: Qt.NoFocus
        hoverEnabled: true
        contentItem: IconGlyph {
            icon: button.entryIcon
            opacity: button.enabled ? 1.0 : StyleConstants.disabledOpacity
        }

        ToolTip.visible: hovered
        ToolTip.text: entryText
        Accessible.name: entryText

        onPressed: root.menuModel.trigger(entryAction)
    }

    component ActionRow: MenuItem {
        id: row

        required property string entryAction
        required property string entryText
        required property int entryIcon
        required property int entryTone
        required property string entryShortcut
        required property bool entryEnabled
        required property bool entryShown

        readonly property color toneColor: entryTone === ContextMenuModel.Trash ? Theme.colors.trash
                                         : entryTone === ContextMenuModel.Danger ? Theme.colors.danger
                                         : "transparent"
        readonly property bool toned: entryTone !== ContextMenuModel.Normal

        text: entryText
        hasGlyph: true
        glyph: entryIcon
        glyphColor: toned ? toneColor : Theme.colors.icons
        labelColor: toned ? toneColor : Theme.colors.textHc
        shortcutText: entryShortcut
        enabled: entryEnabled
        visible: entryShown
        height: visible ? implicitHeight : 0

        onTriggered: root.menuModel.trigger(entryAction)
    }

    component SeparatorRow: MenuSeparator {
        required property bool entryShown

        visible: entryShown
        height: visible ? implicitHeight : 0
    }

    // "More": expands the rows below in place and keeps the menu open, so it
    // is not a MenuItem (a triggered MenuItem closes the menu).
    component ExpanderRow: Item {
        id: expander

        required property string entryText
        required property int entryIcon

        implicitWidth: expanderContent.implicitWidth + 2 * StyleConstants.menuItemHorizontalPadding
        implicitHeight: Theme.metrics.contextMenuItemHeight
        width: root.availableWidth

        Rectangle {
            anchors.fill: parent
            visible: expanderArea.containsMouse
            radius: StyleConstants.itemRadius
            color: Color.transparent(Theme.colors.accent, StyleConstants.accentHighlightOpacity)
        }

        Row {
            id: expanderContent

            anchors.left: parent.left
            anchors.leftMargin: StyleConstants.menuItemHorizontalPadding
            anchors.verticalCenter: parent.verticalCenter
            spacing: StyleConstants.menuItemSpacing

            IconGlyph {
                anchors.verticalCenter: parent.verticalCenter
                anchors.verticalCenterOffset: root.expanderIconOffset
                icon: expander.entryIcon
            }

            Label {
                anchors.verticalCenter: parent.verticalCenter
                text: expander.entryText
                color: Theme.colors.textHc
            }
        }

        MouseArea {
            id: expanderArea

            anchors.fill: parent
            hoverEnabled: true
            onPressed: root.menuModel.toggleMore()
        }

        Accessible.role: Accessible.Button
        Accessible.name: entryText
    }

    // "Open with...": the scripts with a command, then "Configure menu".
    component ScriptsMenu: Menu {
        id: scriptsMenu

        required property string entryText
        required property int entryIcon
        required property bool entryEnabled
        required property bool entryShown

        readonly property int glyph: entryIcon
        readonly property bool available: entryEnabled
        readonly property bool shown: entryShown

        title: entryText

        Instantiator {
            model: root.menuModel.scripts
            delegate: ActionRow {}
            onObjectAdded: (index, object) => scriptsMenu.insertItem(index, object as Item)
            onObjectRemoved: (index, object) => scriptsMenu.removeItem(object as Item)
        }

        MenuSeparator {}

        MenuItem {
            text: qsTranslate("ContextMenu", "Configure menu")
            hasGlyph: true
            glyph: FluentIcons.Settings20
            onTriggered: root.menuModel.configureScripts()
        }
    }
}
