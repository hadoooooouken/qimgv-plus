pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import qimgv.bridges
import qimgv.style

// The settings window (SettingsDialog in the widget UI): an application-
// modal window over the main window with the page list on the left, the
// page on the right and OK / Apply / Cancel below. Each page is created the
// first time it is shown while the window is open, and the pages are
// dropped when it closes, so every opening shows the reloaded values. The
// shortcut creator and the script editor open over it. Escape and the close
// button cancel.
Window {
    id: root

    required property SettingsDialogController controller
    readonly property SettingsEditorModel editor: root.controller.editor

    readonly property int initialWidth: 900
    readonly property int initialHeight: 680
    readonly property int minimumContentWidth: 700
    readonly property int minimumContentHeight: 620
    readonly property int sidebarWidth: 170
    readonly property int sidebarIconSize: 24
    readonly property int sidebarRowHeight: 40
    readonly property int pageSpacing: 12

    // The pages in SettingsEditorModel.Page order, with the sidebar texts
    // of the widget dialog.
    readonly property list<var> pages: [
        {title: qsTranslate("SSideBar", "General"), icon: FluentIcons.Settings32},
        {title: qsTranslate("SSideBar", "View"), icon: FluentIcons.Eye32},
        {title: qsTranslate("SSideBar", "Theme"), icon: FluentIcons.ColorFill32},
        {title: qsTranslate("SSideBar", "Controls"), icon: FluentIcons.Controls32},
        {title: qsTranslate("SSideBar", "Scripts"), icon: FluentIcons.Scripts32},
        {title: qsTranslate("SSideBar", "Advanced"), icon: FluentIcons.WrenchScrewdriver32},
        {title: qsTranslate("SSideBar", "AI Upscale"), icon: FluentIcons.BrainSparkle32},
        {title: qsTranslate("SSideBar", "About"), icon: FluentIcons.Info32}
    ]

    title: root.editor.windowTitle
    flags: Qt.Dialog | Qt.CustomizeWindowHint | Qt.WindowTitleHint | Qt.WindowCloseButtonHint
    modality: Qt.ApplicationModal
    color: Theme.colors.widget
    width: root.initialWidth
    height: root.initialHeight
    minimumWidth: root.minimumContentWidth
    minimumHeight: root.minimumContentHeight
    visible: root.controller.open

    onVisibleChanged: {
        if (!root.visible)
            return;
        const owner = root.transientParent;
        if (owner) {
            root.x = owner.x + (owner.width - root.width) / 2;
            root.y = owner.y + (owner.height - root.height) / 2;
        }
        keyScope.forceActiveFocus();
        root.requestActivate();
    }

    onClosing: close => {
        close.accepted = false;
        root.controller.dismiss();
    }

    FocusScope {
        id: keyScope

        anchors.fill: parent
        focus: true

        Keys.onEscapePressed: event => {
            event.accepted = true;
            root.controller.dismiss();
        }

        RowLayout {
            anchors.fill: parent
            anchors.margins: StyleConstants.dialogPadding
            spacing: root.pageSpacing

            ColumnLayout {
                Layout.preferredWidth: root.sidebarWidth
                Layout.fillHeight: true
                spacing: StyleConstants.dialogSpacing

                ListView {
                    id: sidebar

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    model: root.pages
                    currentIndex: root.controller.page
                    interactive: false

                    delegate: ItemDelegate {
                        id: entry

                        required property int index
                        required property var modelData

                        width: ListView.view.width
                        height: root.sidebarRowHeight
                        highlighted: ListView.isCurrentItem
                        onClicked: root.controller.page = entry.index

                        contentItem: RowLayout {
                            spacing: StyleConstants.menuItemSpacing

                            IconGlyph {
                                icon: entry.modelData.icon
                                size: root.sidebarIconSize
                            }
                            Label {
                                Layout.fillWidth: true
                                text: entry.modelData.title
                                elide: Text.ElideRight
                            }
                        }
                    }
                }

                RowLayout {
                    spacing: StyleConstants.dialogSpacing

                    SettingsNote {
                        Layout.fillWidth: false
                        text: Qt.application.name + " " + root.editor.applicationVersion
                    }
                    SettingsNote {
                        Layout.fillWidth: false
                        text: "Qt " + root.editor.qtVersion
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: StyleConstants.dialogSpacing

                Item {
                    id: pageArea

                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    Repeater {
                        model: root.pages.length

                        delegate: Loader {
                            id: pageLoader

                            required property int index
                            // Created on first show, dropped on close.
                            property bool shown: false

                            anchors.fill: parent
                            active: pageLoader.shown && root.controller.open
                            visible: root.controller.page === pageLoader.index
                            sourceComponent: root.pageComponents[pageLoader.index]

                            function update() {
                                if (!root.controller.open)
                                    pageLoader.shown = false;
                                else if (root.controller.page === pageLoader.index)
                                    pageLoader.shown = true;
                            }

                            Component.onCompleted: pageLoader.update()

                            Connections {
                                target: root.controller

                                function onPageChanged() {
                                    pageLoader.update();
                                }

                                function onOpenChanged() {
                                    pageLoader.update();
                                }
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: StyleConstants.dialogButtonSpacing

                    Item {
                        Layout.fillWidth: true
                    }
                    Button {
                        text: qsTranslate("SettingsDialog", "OK")
                        onClicked: root.controller.accept()
                    }
                    Button {
                        text: qsTranslate("SettingsDialog", "Apply")
                        onClicked: root.controller.applyChanges()
                    }
                    Button {
                        text: qsTranslate("SettingsDialog", "Cancel")
                        onClicked: root.controller.dismiss()
                    }
                }
            }
        }
    }

    // In SettingsEditorModel.Page order.
    readonly property list<Component> pageComponents: [
        Component {
            SettingsGeneralPage {
                editor: root.editor
            }
        },
        Component {
            SettingsViewPage {
                editor: root.editor
            }
        },
        Component {
            SettingsThemePage {
                editor: root.editor
            }
        },
        Component {
            SettingsControlsPage {
                editor: root.editor
            }
        },
        Component {
            SettingsScriptsPage {
                editor: root.editor
            }
        },
        Component {
            SettingsAdvancedPage {
                editor: root.editor
            }
        },
        Component {
            SettingsUpscalePage {
                editor: root.editor
            }
        },
        Component {
            SettingsAboutPage {
                editor: root.editor
            }
        }
    ]

    Loader {
        active: root.editor.shortcutEditor.created
        sourceComponent: ShortcutCreatorDialog {
            dialog: root.editor.shortcutEditor
            transientParent: root
        }
    }

    Loader {
        active: root.editor.scriptEditor.created
        sourceComponent: ScriptEditorDialog {
            dialog: root.editor.scriptEditor
            transientParent: root
        }
    }
}
