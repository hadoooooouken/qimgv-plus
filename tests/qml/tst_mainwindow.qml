import QtQuick
import QtTest
import qimgv.bridges
import qimgv.tests

TestCase {
    id: testCase

    name: "MainWindow"

    readonly property url droppedFile: "file:///C:/images/dropped.png"

    function createWindow() {
        const component = Qt.createComponent("qimgv.ui", "Main");
        compare(component.status, Component.Ready, component.errorString());
        const window = createTemporaryObject(component, testCase, {
            viewportController: Fixture.viewportController,
            windowShell: Fixture.windowShell,
            overlays: Fixture.overlays
        });
        verify(window !== null, "qimgv.ui/Main could not be instantiated");
        return window;
    }

    function childOfType(window, typeName) {
        const children = window.contentItem.children;
        for (let i = 0; i < children.length; ++i) {
            if (children[i].toString().startsWith(typeName))
                return children[i];
        }
        return null;
    }

    function cleanup() {
        Fixture.setFolderViewActive(false);
        Fixture.setFullscreen(false);
    }

    function test_createsHiddenFromModule() {
        const window = createWindow();
        // Core shows the window through its port.
        verify(!window.visible);
        compare(window.width, window.initialWidth);
        compare(window.height, window.initialHeight);
        verify(Qt.colorEqual(window.color, Theme.colors.background));
    }

    function test_fullscreenUsesTheFullscreenBackground() {
        const window = createWindow();
        Fixture.setFullscreen(true);
        verify(Qt.colorEqual(window.color, Theme.colors.backgroundFullscreen));
        Fixture.setFullscreen(false);
        verify(Qt.colorEqual(window.color, Theme.colors.background));
    }

    function test_folderModeReplacesTheViewport() {
        const window = createWindow();
        const viewport = childOfType(window, "ImageViewport");
        verify(viewport !== null, "no ImageViewport in the main window");
        verify(viewport.visible);

        Fixture.setFolderViewActive(true);
        verify(!viewport.visible);
        Fixture.setFolderViewActive(false);
        verify(viewport.visible);
    }

    function test_externalFileDropsReachTheShell() {
        const window = createWindow();
        window.visible = true;
        verify(waitForRendering(window.contentItem));

        verify(Fixture.dropExternalFile(window, Qt.point(window.width / 2, window.height / 2),
                                        testCase.droppedFile));
        compare(Fixture.lastDroppedUrls.length, 1);
        compare(Fixture.lastDroppedUrls[0], testCase.droppedFile);
    }
}
