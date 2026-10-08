import QtQuick
import QtTest

TestCase {
    id: testCase

    name: "MainWindow"

    function test_createsFromModule() {
        const component = Qt.createComponent("qimgv.ui", "Main");
        compare(component.status, Component.Ready, component.errorString());

        const window = createTemporaryObject(component, testCase, {
            visible: false
        });
        verify(window !== null, "qimgv.ui/Main could not be instantiated");
        compare(window.width, window.initialWidth);
        compare(window.height, window.initialHeight);
    }
}
