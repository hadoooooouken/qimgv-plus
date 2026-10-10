#pragma once

#include <QObject>
#include <QSize>
#include <QString>

#include "components/shellinfo/windowtitle.h"
#include "components/windowstate/windowstatecontroller.h"
#include "gui/ports/shellport.h"
#include "gui/ports/windowport.h"

class ActionManager;
class ContextMenuModel;
class CropController;
class FolderViewController;
class ImageViewportController;
class MainWindowShell;
class OverlayCoordinator;
class QQuickWindow;
class QuickFolderViewActions;
class Settings;
class ThumbnailPanelController;
class UiEvents;
class ViewModeController;

// Everything the Quick main window controller works with. All referenced
// objects must outlive the controller.
struct QuickMainWindowContext {
    QQuickWindow &window;
    MainWindowShell &shell;
    ImageViewportController &viewport;
    OverlayCoordinator &overlays;
    ThumbnailPanelController &thumbnailPanel;
    ContextMenuModel &contextMenu;
    CropController &crop;
    FolderViewController &folderView;
    QuickFolderViewActions &folderViewActions;
    ViewModeController &viewMode;
    UiEvents &events;
    Settings &settings;
    ActionManager &actions;
};

// Window and shell ports of the Qt Quick UI. Owns the window states
// (WindowStateController: saved placement, pseudo-fullscreen, display
// memory) and persists them in Settings; keeps the window title and the
// MainWindowShell state current; turns window close requests into standby or
// exit, and dropped files into UiEvents::droppedIn. Runs the window actions
// of ActionManager (fullscreen, close fullscreen or exit).
//
// Concealing (IWindowPort::setWindowConcealed) does nothing: the Quick window
// is never hidden behind an opacity trick. It is shown with the theme
// background as its clear colour, so its first frame is the themed window and
// the image appears with the first frame that renders it.
//
// The overlay parts of the shell port (metadata, save confirmation,
// fullscreen info bar, rename prompt, current file) and the window's pointer
// moves, fullscreen state and view mode go to the OverlayCoordinator. The
// thumbnail panel follows the window's pointer moves and exits, size,
// activation, fullscreen state and view mode. The context menu and the crop
// mode follow the view mode; the crop panel requests open and close the
// crop mode (CropController) with the size of the window's screen. The
// folder view (FolderViewController) follows the view mode and the
// fullscreen state and gets the folder view parts of the shell port: the
// directory path, the sorting indicators and the folder tree refresh.
// GUI thread only.
class QuickMainWindowController final : public QObject,
                                        public IWindowPort,
                                        public IShellPort {
    Q_OBJECT
public:
    explicit QuickMainWindowController(const QuickMainWindowContext &context,
                                       QObject *parent = nullptr);

    [[nodiscard]] bool isFullscreen() const;

    // With the "autoResizeWindow" setting, fits a plain window to a document
    // of the given size (QuickViewerPort::documentShown()).
    void fitWindowToDocument(QSize size);

    // IWindowPort
    void showWindow() override;
    void hideWindow() override;
    [[nodiscard]] bool isWindowVisible() const override;
    void setWindowConcealed(bool concealed) override;
    void raiseAndActivateWindow() override;
    [[nodiscard]] WId nativeWindowHandle() const override;
    void saveWindowGeometry() override;
    void setWindowUpdatesSuspended(bool suspended) override;

    // IShellPort
    void setDirectoryPath(const QString &path) override;
    void setCurrentInfo(const ShellFileInfo &info) override;
    void setMetadata(const MetadataEntries &entries) override;
    void notifySortingChanged(SortingMode mode) override;
    void notifyFolderSortingChanged(SortingMode mode) override;
    void refreshFolderTree(const QString &directoryPath) override;
    void setSaveOverlayVisible(bool visible) override;
    [[nodiscard]] bool isCropPanelActive() const override;
    void toggleCropPanel() override;
    void toggleFullscreenInfoBar() override;
    void toggleRenamePrompt(const QString &currentName) override;

private:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void onCloseRequested();
    void closeFullscreenOrExit();
    void persistPlacement(const WindowPlacement &placement);
    void updateTitle();

    QQuickWindow &window;
    MainWindowShell &shell;
    ImageViewportController &viewport;
    OverlayCoordinator &overlays;
    ThumbnailPanelController &thumbnailPanel;
    ContextMenuModel &contextMenu;
    CropController &crop;
    FolderViewController &folderView;
    QuickFolderViewActions &folderViewActions;
    ViewModeController &viewMode;
    UiEvents &events;
    Settings &settings;
    ActionManager &actions;
    WindowStateController windowState;
    ShellFileInfo currentInfo;
};
