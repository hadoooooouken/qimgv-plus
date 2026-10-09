#include "quickmainwindowcontroller.h"

#include <QCloseEvent>
#include <QDebug>
#include <QGuiApplication>
#include <QLocale>
#include <QMimeData>
#include <QMouseEvent>
#include <QQuickWindow>

#include "components/actionmanager/actionmanager.h"
#include "components/viewmode/viewmodecontroller.h"
#include "gui/ports/uievents.h"
#include "gui/quick/ui/imageviewportcontroller.h"
#include "gui/quick/ui/mainwindowshell.h"
#include "gui/quick/ui/overlays/overlaycoordinator.h"
#include "gui/quick/ui/thumbnails/thumbnailpanelcontroller.h"
#include "settings.h"

namespace {
using namespace Qt::StringLiterals;

constexpr auto exitActionName = "exit"_L1;
constexpr double kPercent = 100.0;

WindowPlacement savedPlacement(Settings &settings) {
    return {
        .geometry = settings.windowGeometry(),
        .maximized = settings.maximizedWindow(),
        .display = settings.lastDisplay(),
    };
}
} // namespace

QuickMainWindowController::QuickMainWindowController(
    const QuickMainWindowContext &context, QObject *parent)
    : QObject(parent),
      window(context.window),
      shell(context.shell),
      viewport(context.viewport),
      overlays(context.overlays),
      thumbnailPanel(context.thumbnailPanel),
      viewMode(context.viewMode),
      events(context.events),
      settings(context.settings),
      actions(context.actions),
      windowState(context.window, savedPlacement(context.settings)) {
    window.installEventFilter(this);

    connect(&windowState, &WindowStateController::placementChanged, this,
            &QuickMainWindowController::persistPlacement);
    connect(&windowState, &WindowStateController::fullscreenChanged, &shell,
            &MainWindowShell::setFullscreen);
    connect(&windowState, &WindowStateController::fullscreenChanged, &overlays,
            &OverlayCoordinator::setFullscreen);
    connect(&windowState, &WindowStateController::fullscreenChanged, &thumbnailPanel,
            &ThumbnailPanelController::setFullscreen);
    connect(&window, &QWindow::widthChanged, this,
            [this]() { thumbnailPanel.setWindowSize(window.size()); });
    connect(&window, &QWindow::heightChanged, this,
            [this]() { thumbnailPanel.setWindowSize(window.size()); });
    connect(&window, &QWindow::activeChanged, this, [this]() {
        if (!window.isActive())
            thumbnailPanel.pointerLeftWindow();
    });
    // Saved also when the application quits while the window rests (the
    // debounced report may still be pending).
    connect(qGuiApp, &QGuiApplication::aboutToQuit, this,
            [this]() { windowState.savePlacement(); });

    // Core::onDropIn reads the URLs synchronously, so the payload can live on
    // the stack for the duration of the emission.
    connect(&shell, &MainWindowShell::urlsDropped, this,
            [this](const QList<QUrl> &urls, QObject *source) {
        QMimeData mimeData;
        mimeData.setUrls(urls);
        emit events.droppedIn(&mimeData, source);
    });

    connect(&viewMode, &ViewModeController::viewModeApplied, this, [this](ViewMode mode) {
        shell.setFolderViewActive(mode == MODE_FOLDERVIEW);
        overlays.setFolderViewActive(mode == MODE_FOLDERVIEW);
        thumbnailPanel.setFolderViewActive(mode == MODE_FOLDERVIEW);
        updateTitle();
    });
    shell.setFolderViewActive(viewMode.currentViewMode() == MODE_FOLDERVIEW);
    overlays.setFolderViewActive(viewMode.currentViewMode() == MODE_FOLDERVIEW);
    overlays.setFullscreen(windowState.isFullscreen());
    thumbnailPanel.setFolderViewActive(viewMode.currentViewMode() == MODE_FOLDERVIEW);
    thumbnailPanel.setFullscreen(windowState.isFullscreen());
    thumbnailPanel.setWindowSize(window.size());

    // The title shows the zoom, the view locks and (by setting) extended
    // details. The lock actions run on the viewport before these
    // connections, which are made later.
    connect(&viewport, &ImageViewportController::scaleChanged, this,
            &QuickMainWindowController::updateTitle);
    connect(&actions, &ActionManager::lockZoom, this, &QuickMainWindowController::updateTitle);
    connect(&actions, &ActionManager::lockView, this, &QuickMainWindowController::updateTitle);
    connect(&settings, &Settings::settingsChanged, this, &QuickMainWindowController::updateTitle);

    connect(&actions, &ActionManager::toggleFullscreen, this,
            [this]() { windowState.toggleFullscreen(); });
    connect(&actions, &ActionManager::closeFullScreenOrExit, this,
            &QuickMainWindowController::closeFullscreenOrExit);

    updateTitle();
}

bool QuickMainWindowController::isFullscreen() const {
    return windowState.isFullscreen();
}

//------------------------------------------------------------------------------
// IWindowPort

void QuickMainWindowController::showWindow() {
    if (window.isVisible())
        return;
    if (settings.fullscreenMode())
        windowState.showFullscreen();
    else
        windowState.showWindowed();
}

void QuickMainWindowController::hideWindow() {
    window.hide();
}

bool QuickMainWindowController::isWindowVisible() const {
    return window.isVisible();
}

void QuickMainWindowController::setWindowConcealed(bool concealed) {
    // See the class comment: the window is never concealed.
    Q_UNUSED(concealed)
}

void QuickMainWindowController::raiseAndActivateWindow() {
    window.raise();
    window.requestActivate();
}

WId QuickMainWindowController::nativeWindowHandle() const {
    return window.winId();
}

void QuickMainWindowController::saveWindowGeometry() {
    windowState.savePlacement();
}

void QuickMainWindowController::setWindowUpdatesSuspended(bool suspended) {
    // The scene graph synchronizes with the GUI thread only between event
    // loop iterations, so changes applied in one go are presented together.
    Q_UNUSED(suspended)
}

//------------------------------------------------------------------------------
// IShellPort

void QuickMainWindowController::setDirectoryPath(const QString &path) {
    Q_UNUSED(path)
    reportUnavailable(u"folder view path"_s);
}

void QuickMainWindowController::setCurrentInfo(const ShellFileInfo &info) {
    currentInfo = info;
    updateTitle();
    overlays.setFileInfo(info);
}

void QuickMainWindowController::setMetadata(const MetadataEntries &entries) {
    overlays.setMetadata(entries);
}

void QuickMainWindowController::notifySortingChanged(SortingMode mode) {
    Q_UNUSED(mode)
    reportUnavailable(u"sorting indicator"_s);
}

void QuickMainWindowController::notifyFolderSortingChanged(SortingMode mode) {
    Q_UNUSED(mode)
    reportUnavailable(u"folder sorting indicator"_s);
}

void QuickMainWindowController::refreshFolderTree(const QString &directoryPath) {
    Q_UNUSED(directoryPath)
    reportUnavailable(u"folder tree"_s);
}

void QuickMainWindowController::setSaveOverlayVisible(bool visible) {
    overlays.setSaveConfirmVisible(visible);
}

bool QuickMainWindowController::isCropPanelActive() const {
    return false;
}

void QuickMainWindowController::toggleCropPanel() {
    reportUnavailable(u"crop panel"_s);
}

void QuickMainWindowController::toggleFullscreenInfoBar() {
    overlays.fullscreenChrome()->toggleInfoBar();
}

void QuickMainWindowController::toggleRenamePrompt(const QString &currentName) {
    overlays.toggleRename(currentName);
}

//------------------------------------------------------------------------------

bool QuickMainWindowController::eventFilter(QObject *watched, QEvent *event) {
    // Every pointer move over the window, whichever item handles it, like
    // the application-wide filter of the widget main window.
    if (watched == &window && event->type() == QEvent::MouseMove) {
        const auto *mouseEvent = static_cast<const QMouseEvent *>(event);
        overlays.pointerMoved(mouseEvent->position(), window.isActive());
        thumbnailPanel.pointerMoved(mouseEvent->position(), mouseEvent->buttons());
    }
    if (watched == &window && event->type() == QEvent::Leave)
        thumbnailPanel.pointerLeftWindow();
    if (watched == &window && event->type() == QEvent::Close) {
        // The window stays; standby hides it, exit tears the UI down.
        event->ignore();
        onCloseRequested();
        return true;
    }
    return QObject::eventFilter(watched, event);
}

// As in the widget UI: closing the window suspends to standby when enabled.
// At the end of the Windows session (shutdown, logoff) the application exits
// through the normal exit path, since the window closing alone would not quit
// it (quitOnLastWindowClosed is off for standby).
void QuickMainWindowController::onCloseRequested() {
    if (qGuiApp->isSavingSession() || !settings.standbyMode()) {
        actions.invokeAction(exitActionName);
        return;
    }
    emit events.suspendRequested();
}

void QuickMainWindowController::closeFullscreenOrExit() {
    if (windowState.isFullscreen())
        windowState.showWindowed();
    else
        actions.invokeAction(exitActionName);
}

void QuickMainWindowController::persistPlacement(const WindowPlacement &placement) {
    settings.setWindowGeometry(placement.geometry);
    settings.setMaximizedWindow(placement.maximized);
    settings.setLastDisplay(placement.display);
}

void QuickMainWindowController::updateTitle() {
    const WindowTitleState state{
        .info = currentInfo,
        .viewMode = viewMode.currentViewMode(),
        .scalePercent = qRound(viewport.currentScale() * kPercent),
        .extendedInfo = settings.windowTitleExtendedInfo(),
        .zoomLocked = viewport.isZoomLocked(),
        .viewLocked = viewport.isViewLocked(),
    };
    const QString title = windowTitleFor(state, QLocale());
    if (window.title() != title)
        window.setTitle(title);
}

void QuickMainWindowController::reportUnavailable(const QString &feature) {
    if (reportedUnavailable.contains(feature))
        return;
    reportedUnavailable.insert(feature);
    qInfo().noquote() << "Qt Quick UI:" << feature << "is not available yet";
}
