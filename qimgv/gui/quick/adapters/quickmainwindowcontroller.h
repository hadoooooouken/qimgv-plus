#pragma once

#include <QObject>
#include <QSet>
#include <QString>

#include "components/shellinfo/windowtitle.h"
#include "components/windowstate/windowstatecontroller.h"
#include "gui/ports/shellport.h"
#include "gui/ports/windowport.h"

class ActionManager;
class ImageViewportController;
class MainWindowShell;
class QQuickWindow;
class Settings;
class UiEvents;
class ViewModeController;

// Everything the Quick main window controller works with. All referenced
// objects must outlive the controller.
struct QuickMainWindowContext {
    QQuickWindow &window;
    MainWindowShell &shell;
    ImageViewportController &viewport;
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
// The shell parts without a Quick UI yet (metadata, folder tree, sorting,
// overlays, crop panel, rename prompt) are logged once and otherwise ignored
// until their stages (S2.3 - S3.1) implement them. GUI thread only.
class QuickMainWindowController final : public QObject,
                                        public IWindowPort,
                                        public IShellPort {
    Q_OBJECT
public:
    explicit QuickMainWindowController(const QuickMainWindowContext &context,
                                       QObject *parent = nullptr);

    [[nodiscard]] bool isFullscreen() const;

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
    // Logs, once per feature, that the Quick UI cannot show it yet.
    void reportUnavailable(const QString &feature);

    QQuickWindow &window;
    MainWindowShell &shell;
    ImageViewportController &viewport;
    ViewModeController &viewMode;
    UiEvents &events;
    Settings &settings;
    ActionManager &actions;
    WindowStateController windowState;
    ShellFileInfo currentInfo;
    QSet<QString> reportedUnavailable;
};
