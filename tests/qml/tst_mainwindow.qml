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
            overlays: Fixture.overlays,
            thumbnailPanel: Fixture.thumbnailPanel,
            contextMenu: Fixture.contextMenu,
            crop: Fixture.crop,
            folderView: Fixture.folderView
        });
        verify(window !== null, "qimgv.ui/Main could not be instantiated");
        return window;
    }

    function findType(item, typeName) {
        if (item.toString().startsWith(typeName))
            return item;
        for (let i = 0; i < item.children.length; ++i) {
            const found = findType(item.children[i], typeName);
            if (found)
                return found;
        }
        return null;
    }

    function childOfType(window, typeName) {
        return findType(window.contentItem, typeName);
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

        // The folder view is created on its first activation.
        verify(childOfType(window, "FolderView") === null);
        Fixture.setFolderViewActive(true);
        verify(!viewport.visible);
        tryVerify(() => childOfType(window, "FolderView") !== null);
        Fixture.setFolderViewActive(false);
        verify(viewport.visible);
    }

    function test_pinnedPanelTakesItsSpaceFromTheViewport() {
        Fixture.configureThumbnailPanel(true, SettingsEnums.PanelPosition.Bottom, false);
        const window = createWindow();
        Fixture.setPanelWindowSize(window.width, window.height);
        const viewport = childOfType(window, "ImageViewport");
        compare(viewport.height, window.height - Fixture.thumbnailPanel.layout.panelExtent);
        // A floating panel covers the viewport instead.
        Fixture.configureThumbnailPanel(false, SettingsEnums.PanelPosition.Bottom, false);
        compare(viewport.height, window.height);
    }

    function test_cropModeDocksTheSidePanelAndCoversTheViewport() {
        Fixture.configureThumbnailPanel(false, SettingsEnums.PanelPosition.Bottom, false);
        const window = createWindow();
        const viewport = childOfType(window, "ImageViewport");
        const overlay = childOfType(window, "CropOverlay");
        const sidePanel = childOfType(window, "SidePanel");
        verify(overlay !== null && sidePanel !== null, "no crop overlay or side panel");
        verify(!overlay.visible);
        verify(!sidePanel.visible);
        Fixture.showTestImage(64, 64);
        Fixture.toggleCrop(Qt.size(1920, 1080));
        verify(overlay.visible);
        verify(sidePanel.visible);
        compare(viewport.width, window.width - sidePanel.width);
        compare(overlay.width, viewport.width);
        compare(sidePanel.x, window.width - sidePanel.width);
        Fixture.crop.cancel();
        compare(viewport.width, window.width);
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
