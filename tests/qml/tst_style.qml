pragma ComponentBehavior: Bound

import QtQuick
import QtTest
import qimgv.bridges
import qimgv.style
import qimgv.tests

// The qimgv.style controls over the application's dark and light themes
// (Fixture builds them from ThemeStore). Each test checks the colours of the
// flat parts of a control in its states against the Theme colours they come
// from, sampled at points away from text and rounded corners, so the checks
// do not depend on font rendering. test_saveSnapshots writes the whole
// gallery of both schemes next to the test executable for visual review.
//
// The gallery is a sibling of the TestCase: a TestCase is not visible, so
// its own children would not be rendered or receive pointer events.
Item {
    id: root

    width: 720
    height: 640

    Rectangle {
        id: gallery

        anchors.fill: parent
        color: Theme.colors.widget

        Flow {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 12

            Button {
                id: button
                text: "Button"
                hoverEnabled: true
            }
            Button {
                id: disabledButton
                text: "Disabled"
                enabled: false
            }
            ToolButton {
                id: toolButton
                text: "Tool"
                hoverEnabled: true
            }
            ComboBox {
                id: comboBox
                model: ["First", "Second", "Third"]
                hoverEnabled: true
            }
            Slider {
                id: slider
                width: 200
                from: 0
                to: 1
                value: 0.5
                hoverEnabled: true
            }
            SpinBox {
                id: spinBox
                from: 0
                to: 10
                value: 5
                hoverEnabled: true
            }
            DoubleSpinBox {
                id: doubleSpinBox
                from: 0
                to: 1
                value: 0.5
                stepSize: 0.1
                hoverEnabled: true
            }
            CheckBox {
                id: checkBox
                text: "Check box"
            }
            RadioButton {
                id: radioButton
                text: "Radio button"
            }
            TextField {
                id: textField
                placeholderText: "Text field"
                hoverEnabled: true
            }
            SearchField {
                id: searchField
                placeholderText: "Search field"
                hoverEnabled: true
            }
            Label {
                id: label
                text: "Label"
            }
            Item {
                width: scrollBar.width
                height: 200

                ScrollBar {
                    id: scrollBar
                    height: parent.height
                    orientation: Qt.Vertical
                    policy: ScrollBar.AlwaysOn
                    size: 0.3
                    position: 0
                    hoverEnabled: true
                }
            }
        }

        Menu {
            id: menu

            x: 300
            y: 320
            popupType: Popup.Item

            Action {
                text: "Copy"
                shortcut: "Ctrl+C"
            }
            MenuSeparator {}
            MenuItem {
                text: "Disabled"
                enabled: false
            }
        }

        ToolTip {
            id: toolTip
            parent: button
            text: "Tooltip"
        }

        Popup {
            id: popup

            x: 40
            y: 400
            width: 160
            height: 80
        }
    }

    TestCase {
        id: testCase

        // One 8-bit level per channel, plus one for blending.
        readonly property real tolerance: 2 / 255

        name: "Style"
        when: windowShown

        // Global data: every test function without its own _data runs once
        // per scheme and starts with useScheme(data.dark).
        function init_data() {
            return [
                {tag: "dark", dark: true},
                {tag: "light", dark: false},
            ];
        }

        function useScheme(dark) {
            if (Theme.dark !== dark)
                Fixture.switchTheme();
            compare(Theme.dark, dark);
        }

        function cleanup() {
            menu.close();
            toolTip.close();
            popup.close();
            comboBox.popup.close();
            mouseMove(gallery, gallery.width - 1, gallery.height - 1);
            gallery.forceActiveFocus();
            useScheme(true);
        }

        // Colour at (x, y) of item, read from a grab of the whole gallery
        // (at the scene origin): grabImage(item) crops the window at the item's
        // position in its parent, not in the scene.
        function pixel(item, x, y) {
            const point = item.mapToItem(gallery, x, y);
            return grabImage(gallery).pixel(Math.floor(point.x), Math.floor(point.y));
        }

        function colorClose(actual, expected) {
            return Math.abs(actual.r - expected.r) <= tolerance
                && Math.abs(actual.g - expected.g) <= tolerance
                && Math.abs(actual.b - expected.b) <= tolerance;
        }

        function verifyColor(actual, expected, what) {
            verify(colorClose(actual, expected), `${what}: ${actual} instead of ${expected}`);
        }

        // Colour of a translucent fill over the gallery surface.
        function over(fill, opacity) {
            return Color.blend(Theme.colors.widget, Color.transparent(fill, 1.0), opacity);
        }

        function test_button(data) {
            useScheme(data.dark);
            const y = button.height / 2;
            verifyColor(pixel(button, 3, y), Theme.colors.button, "normal");
            mouseMove(button, 3, y);
            verify(button.hovered);
            verifyColor(pixel(button, 3, y), Theme.colors.buttonHover, "hovered");
            mousePress(button, 3, y);
            verifyColor(pixel(button, 3, y), Theme.colors.buttonPressed, "pressed");
            mouseRelease(button, 3, y);
            compare(disabledButton.contentItem.opacity, StyleConstants.disabledOpacity);
            verify(button.height >= Theme.metrics.buttonHeight);
        }

        function test_toolButton(data) {
            useScheme(data.dark);
            const x = StyleConstants.toolButtonHorizontalInset + 2;
            const y = toolButton.height / 2;
            verifyColor(pixel(toolButton, x, y), Theme.colors.widget, "normal (transparent)");
            mouseMove(toolButton, x, y);
            verifyColor(pixel(toolButton, x, y), Theme.colors.panelButtonHover, "hovered");
            mouseMove(gallery, gallery.width - 1, gallery.height - 1);
            toolButton.checkable = true;
            toolButton.checked = true;
            verifyColor(pixel(toolButton, x, y), Theme.colors.panelButton, "checked");
            toolButton.checked = false;
            toolButton.checkable = false;
            // The inset keeps the panel colour off the bar edge.
            verifyColor(pixel(toolButton, x, 1), Theme.colors.widget, "inset");
        }

        function test_comboBox(data) {
            useScheme(data.dark);
            const y = comboBox.height / 2;
            verify(comboBox.highlightOnHover);
            verifyColor(pixel(comboBox, 3, y), Theme.colors.comboField, "field");
            verifyColor(pixel(comboBox, 0, y), Theme.colors.comboFieldBorder, "border");
            mouseMove(comboBox, 3, y);
            verifyColor(pixel(comboBox, 3, y), Theme.colors.comboFieldHover, "hovered");
            comboBox.forceActiveFocus();
            verifyColor(pixel(comboBox, 0, y), Theme.colors.accent, "focus border");
            compare(comboBox.indicator.text, Theme.glyph(FluentIcons.ChevronDown12));

            comboBox.popup.open();
            tryCompare(comboBox.popup, "opened", true);
            const background = comboBox.popup.background;
            verifyColor(pixel(background, background.width / 2, 2), Theme.colors.widget, "popup");
            verifyColor(pixel(background, background.width / 2, 0), Theme.colors.widgetBorder,
                        "popup border");
            compare(background.radius, Theme.metrics.contextMenuBorderRadius);
        }

        function test_slider(data) {
            useScheme(data.dark);
            const y = slider.height / 2;
            verifyColor(pixel(slider, 20, y), Theme.colors.accent, "filled groove");
            verifyColor(pixel(slider, slider.width - 20, y), Theme.surfaces.tintedHc2, "groove");
            const handle = slider.handle;
            const centerX = handle.x + handle.width / 2;
            verifyColor(pixel(slider, centerX, y), Theme.colors.accent, "handle");
            verifyColor(pixel(slider, handle.x + 1, y), Theme.colors.widget, "handle ring");
            mouseMove(slider, centerX, y);
            verifyColor(pixel(slider, centerX, y),
                        Color.lighter(Theme.colors.accent, StyleConstants.accentLightFactor),
                        "hovered handle");
            mouseMove(gallery, gallery.width - 1, gallery.height - 1);
            slider.enabled = false;
            verifyColor(pixel(slider, 20, y), Theme.colors.widgetBorder, "disabled fill");
            verifyColor(pixel(slider, centerX, y), Theme.colors.text, "disabled handle");
            slider.enabled = true;
        }

        function test_spinBoxes_data() {
            return [
                {tag: "int, dark", dark: true, spin: spinBox},
                {tag: "int, light", dark: false, spin: spinBox},
                {tag: "double, dark", dark: true, spin: doubleSpinBox},
                {tag: "double, light", dark: false, spin: doubleSpinBox},
            ];
        }

        function test_spinBoxes(data) {
            useScheme(data.dark);
            const spin = data.spin;
            const field = spin.width / 2;
            verifyColor(pixel(spin, field, 1), Theme.colors.button, "field");
            mouseMove(spin, field, spin.height / 2);
            verifyColor(pixel(spin, field, 1), Theme.colors.buttonHover, "hovered");
            compare(spin.up.indicator.icon, FluentIcons.ChevronUp20);
            compare(spin.down.indicator.icon, FluentIcons.ChevronDown20);

            const value = spin.value;
            mouseClick(spin.up.indicator);
            verify(spin.value > value);
            mouseClick(spin.down.indicator);
            compare(spin.value, value);

            spin.value = spin.to;
            verify(!spin.up.indicator.enabled);
            verify(spin.down.indicator.enabled);
            spin.value = value;
        }

        function test_selectionIndicators(data) {
            useScheme(data.dark);
            compare(checkBox.indicator.text, Theme.glyph(FluentIcons.CheckboxUnchecked16));
            mouseClick(checkBox);
            compare(checkBox.indicator.text, Theme.glyph(FluentIcons.CheckboxChecked16));
            checkBox.tristate = true;
            checkBox.checkState = Qt.PartiallyChecked;
            compare(checkBox.indicator.text, Theme.glyph(FluentIcons.CheckboxIndeterminate16));
            checkBox.checkState = Qt.Unchecked;
            checkBox.tristate = false;

            compare(radioButton.indicator.text, Theme.glyph(FluentIcons.RadioButton16));
            mouseClick(radioButton);
            compare(radioButton.indicator.text, Theme.glyph(FluentIcons.Record16));
            radioButton.checked = false;

            compare(checkBox.indicator.font.family, Theme.iconFontFamily);
            verifyColor(checkBox.indicator.color, Theme.colors.icons, "indicator");
            checkBox.enabled = false;
            compare(checkBox.indicator.opacity, StyleConstants.disabledOpacity);
            checkBox.enabled = true;
        }

        function test_textFields(data) {
            useScheme(data.dark);
            const y = textField.height / 2;
            verifyColor(pixel(textField, 1, y), Theme.colors.button, "field");
            textField.forceActiveFocus();
            verifyColor(pixel(textField, 1, y), Theme.colors.buttonHover, "focused");

            verifyColor(pixel(searchField, 1, searchField.height / 2), Theme.colors.button,
                        "search field");
            verify(searchField.searchIndicator.indicator === null);
            verify(searchField.contentItem.children[0].visible);
            compare(searchField.contentItem.children[0].text, searchField.placeholderText);
            verify(!searchField.clearIndicator.indicator.visible);
            searchField.text = "filter";
            verify(searchField.clearIndicator.indicator.visible);
            compare(searchField.clearIndicator.indicator.icon, FluentIcons.Dismiss16);
            searchField.text = "";
        }

        function test_scrollBar(data) {
            useScheme(data.dark);
            const x = scrollBar.width / 2;
            compare(scrollBar.width, StyleConstants.scrollBarThickness);
            verifyColor(pixel(scrollBar, x, 10), Theme.colors.scrollbar, "handle");
            mouseMove(scrollBar, x, 10);
            verifyColor(pixel(scrollBar, x, 10), Theme.colors.scrollbarHover, "hovered handle");
            // The handle never gets shorter than the minimum length.
            scrollBar.size = 0.05;
            verifyColor(pixel(scrollBar, x, StyleConstants.scrollBarMinimumHandleLength - 2),
                        Theme.colors.scrollbarHover, "minimum handle");
            scrollBar.size = 0.3;
        }

        function test_menu(data) {
            useScheme(data.dark);
            verify(menu.separatorsCollapsible);
            menu.open();
            tryCompare(menu, "opened", true);
            verify(menu.width >= Theme.metrics.contextMenuWidth);
            const background = menu.background;
            verifyColor(pixel(background, background.width / 2, 2), Theme.colors.widget, "menu");
            verifyColor(pixel(background, background.width / 2, 0), Theme.colors.widgetBorder,
                        "menu border");

            const item = menu.itemAt(0);
            compare(item.height, Theme.metrics.contextMenuItemHeight);
            const shortcut = item.contentItem.shortcutText;
            compare(shortcut, ShortcutText.of("Ctrl+C"));
            verify(shortcut.length > 0);
            menu.currentIndex = 0;
            verifyColor(pixel(item, 3, item.height / 2),
                        over(Theme.colors.accent, StyleConstants.accentHighlightOpacity),
                        "highlighted item");

            const disabledItem = menu.itemAt(2);
            verify(!disabledItem.enabled);
            verifyColor(disabledItem.contentItem.secondaryColor,
                        Color.transparent(Theme.colors.text, StyleConstants.secondaryTextOpacity),
                        "secondary text");
        }

        function test_toolTipAndPopup(data) {
            useScheme(data.dark);
            toolTip.open();
            tryCompare(toolTip, "opened", true);
            const tipBackground = toolTip.background;
            verifyColor(pixel(tipBackground, 3, tipBackground.height / 2), Theme.colors.widget,
                        "tooltip");
            compare(tipBackground.radius, Theme.metrics.tooltipBorderRadius);
            verifyColor(toolTip.contentItem.color, Theme.colors.textHc, "tooltip text");

            popup.open();
            tryCompare(popup, "opened", true);
            verifyColor(pixel(popup.background, popup.width / 2, popup.height / 2),
                        Theme.colors.widget, "popup");
            verifyColor(pixel(popup.background, popup.width / 2, 0), Theme.colors.widgetBorder,
                        "popup border");

            verifyColor(label.color, Theme.colors.textHc, "label");
        }

        // A theme switch restyles live controls through their bindings, with
        // no reload.
        function test_runtimeThemeSwitch_data() {
            return [{tag: "dark to light", dark: true}];
        }

        function test_runtimeThemeSwitch(data) {
            useScheme(data.dark);
            const y = button.height / 2;
            const darkButton = pixel(button, 3, y);
            const darkField = pixel(comboBox, 3, comboBox.height / 2);
            verifyColor(darkButton, Theme.colors.button, "dark button");

            Fixture.switchTheme();
            verify(!Theme.dark);
            const lightButton = pixel(button, 3, y);
            verifyColor(lightButton, Theme.colors.button, "light button");
            verify(!colorClose(lightButton, darkButton));
            verifyColor(pixel(comboBox, 3, comboBox.height / 2), Theme.colors.comboField,
                        "light combo box");
            verify(!colorClose(pixel(comboBox, 3, comboBox.height / 2), darkField));
            verifyColor(pixel(gallery, 1, 1), Theme.colors.widget, "light surface");
        }

        // Writes style-dark.png and style-light.png (the gallery with a menu,
        // a tooltip and a popup open) for visual review.
        function test_saveSnapshots(data) {
            useScheme(data.dark);
            menu.open();
            toolTip.open();
            popup.open();
            tryCompare(menu, "opened", true);
            tryCompare(toolTip, "opened", true);
            tryCompare(popup, "opened", true);
            menu.currentIndex = 0;
            grabImage(gallery).save(Fixture.artifactPath(`style-${data.tag}.png`));
        }
    }
}
