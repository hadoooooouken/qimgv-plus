import QtQuick
import QtTest
import qimgv.bridges
import qimgv.tests

// AppSettings, Theme and Actions as published by QmlTestSetup over the
// Fixture singleton (bridgetestfixture.h). The input items are siblings of
// the TestCase: a TestCase is not visible, so its own children would not
// receive pointer events.
Item {
    id: root

    width: 200
    height: 200

    // One row of Actions.actions.
    component ActionRow: Item {
        required property string name
        required property string shortcut
    }

    // A binding consumer, to check that notifications reach bindings.
    QtObject {
        id: consumer

        property bool smoothZoom: AppSettings.viewer.smoothZoom
        property color background: Theme.colors.background
    }

    Item {
        id: keySink

        anchors.fill: parent
        focus: true
        Keys.onPressed: event => event.accepted = Actions.handleKeyEvent(event)

        Repeater {
            id: actionRows

            model: Actions.actions
            delegate: ActionRow {}
        }

        MouseArea {
            id: wheelArea

            anchors.fill: parent
            onWheel: wheel => wheel.accepted = Actions.handleWheelEvent(wheel)
        }
    }

    TestCase {
        id: testCase

        readonly property int wheelNotch: 120
        readonly property string settingsGlyph: String.fromCodePoint(0xF6A9)

        name: "Bridges"
        when: windowShown

        SignalSpy {
            id: viewerSpy
            target: AppSettings
            signalName: "viewerChanged"
        }
        SignalSpy {
            id: panelSpy
            target: AppSettings
            signalName: "panelChanged"
        }
        SignalSpy {
            id: folderViewSpy
            target: AppSettings
            signalName: "folderViewChanged"
        }
        SignalSpy {
            id: overlaysSpy
            target: AppSettings
            signalName: "overlaysChanged"
        }
        SignalSpy {
            id: colorsSpy
            target: Theme
            signalName: "colorsChanged"
        }
        SignalSpy {
            id: darkSpy
            target: Theme
            signalName: "darkChanged"
        }
        SignalSpy {
            id: fontsSpy
            target: Theme
            signalName: "fontsChanged"
        }
        SignalSpy {
            id: iconFontSpy
            target: Theme
            signalName: "iconFontFamilyChanged"
        }
        SignalSpy {
            id: shortcutsSpy
            target: Actions
            signalName: "shortcutsChanged"
        }

        function init() {
            for (const spy of [viewerSpy, panelSpy, folderViewSpy, overlaysSpy,
                               colorsSpy, darkSpy, fontsSpy, iconFontSpy, shortcutsSpy])
                spy.clear();
        }

        function test_settingsChangeNotifiesOnlyItsArea() {
            const before = AppSettings.viewer.smoothZoom;
            Fixture.toggleSmoothZoom();

            compare(viewerSpy.count, 1);
            compare(panelSpy.count, 0);
            compare(folderViewSpy.count, 0);
            compare(overlaysSpy.count, 0);
            compare(AppSettings.viewer.smoothZoom, !before);
            compare(consumer.smoothZoom, AppSettings.viewer.smoothZoom);
        }

        function test_unchangedSettingsDoNotNotify() {
            Fixture.reapplySettings();

            compare(viewerSpy.count, 0);
            compare(panelSpy.count, 0);
            compare(folderViewSpy.count, 0);
            compare(overlaysSpy.count, 0);
        }

        function test_settingsEnums() {
            compare(AppSettings.panel.position, SettingsEnums.PanelPosition.Bottom);
            verify(AppSettings.panel.position !== SettingsEnums.PanelPosition.Top);
        }

        function test_themeSwitchNotifiesColorsAndMode() {
            const wasDark = Theme.dark;
            const background = Theme.colors.background;
            Fixture.switchTheme();

            compare(colorsSpy.count, 1);
            compare(darkSpy.count, 1);
            compare(fontsSpy.count, 0);
            compare(iconFontSpy.count, 0);
            compare(Theme.dark, !wasDark);
            verify(!Qt.colorEqual(Theme.colors.background, background));
            verify(Qt.colorEqual(consumer.background, Theme.colors.background));
        }

        function test_iconGlyphs() {
            compare(Theme.glyph(FluentIcons.Settings20), testCase.settingsGlyph);
            verify(Theme.iconFontFamily.length > 0);
            verify(Theme.compactIconSize > 0);
            verify(Theme.standardIconSize > Theme.compactIconSize);
        }

        function test_invokeAndShortcuts() {
            verify(Actions.invoke("nextImage"));
            compare(Fixture.lastInvoked, "nextImage");
            verify(!Actions.invoke("noSuchAction"));
            compare(Actions.shortcutFor("nextImage"), "Right");
            compare(Actions.shortcutFor("noSuchAction"), "");
        }

        function test_actionsModel() {
            // Sorted by action name: nextImage, prevImage, zoomIn.
            compare(actionRows.count, 3);
            const first = actionRows.itemAt(0) as ActionRow;
            const last = actionRows.itemAt(2) as ActionRow;
            compare(first.name, "nextImage");
            compare(first.shortcut, "Right");
            compare(last.name, "zoomIn");

            Fixture.setShortcut("zoomIn", "Ctrl+=");
            compare(shortcutsSpy.count, 1);
            compare(Actions.shortcutFor("zoomIn"), "Ctrl+=");
            // The model was reassigned, so the delegates were recreated.
            compare((actionRows.itemAt(2) as ActionRow).shortcut, "Ctrl+=");

            // Same shortcut again: nothing changed, nothing is announced.
            Fixture.setShortcut("zoomIn", "Ctrl+=");
            compare(shortcutsSpy.count, 1);
        }

        function test_keyForwarding() {
            keySink.forceActiveFocus();
            keyClick(Qt.Key_Right, Qt.ControlModifier);

            compare(Fixture.lastKey, Qt.Key_Right);
            compare(Fixture.lastModifiers, Qt.ControlModifier);
        }

        function test_wheelForwarding() {
            mouseWheel(wheelArea, wheelArea.width / 2, wheelArea.height / 2, 0, -testCase.wheelNotch);

            compare(Fixture.lastWheelAngleDelta.y, -testCase.wheelNotch);
        }
    }
}
