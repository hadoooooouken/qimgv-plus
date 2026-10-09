#include "windowstatecontroller.h"

#include <QDebug>
#include <QGuiApplication>
#include <QScreen>
#include <QWindow>

namespace {
// A saved geometry that no longer meets any display (a monitor was removed)
// is centred on the primary display instead of opening off-screen.
QRect visibleGeometry(const QRect &saved) {
    const QList<QScreen *> screens = QGuiApplication::screens();
    for (const QScreen *screen : screens) {
        if (screen->availableGeometry().intersects(saved))
            return saved;
    }
    const QScreen *primary = QGuiApplication::primaryScreen();
    if (!primary)
        return saved;
    QRect moved = saved;
    moved.moveCenter(primary->availableGeometry().center());
    return moved;
}
} // namespace

WindowStateController::WindowStateController(QWindow &window,
                                             const WindowPlacement &saved,
                                             QObject *parent)
    : QObject(parent),
      window(window),
      current(saved) {
    placementSettleTimer.setSingleShot(true);
    placementSettleTimer.setInterval(kPlacementSettleDelayMs);
    connect(&placementSettleTimer, &QTimer::timeout, this,
            &WindowStateController::onPlacementSettled);

    connect(&window, &QWindow::xChanged, this, &WindowStateController::onGeometryChanged);
    connect(&window, &QWindow::yChanged, this, &WindowStateController::onGeometryChanged);
    connect(&window, &QWindow::widthChanged, this, &WindowStateController::onGeometryChanged);
    connect(&window, &QWindow::heightChanged, this, &WindowStateController::onGeometryChanged);
    connect(&window, &QWindow::windowStateChanged, this,
            [this]() { onWindowStateChanged(this->window.windowStates()); });
    connect(&window, &QWindow::screenChanged, this, &WindowStateController::updateDisplay);

    restorePlacement();
}

bool WindowStateController::isFullscreen() const {
    return fullscreen;
}

WindowPlacement WindowStateController::placement() const {
    return current;
}

void WindowStateController::showFullscreen() {
    QScreen *screen = targetScreen();
    if (!screen) {
        qWarning() << "WindowStateController: no display for fullscreen; showing"
                      " the window windowed";
        showWindowed();
        return;
    }
    // Keep the windowed placement for the way back; not saved on the very
    // first show (the window has no placement of its own yet).
    if (window.isVisible() && !fullscreen)
        savePlacement();

    const bool entering = !fullscreen;
    fullscreen = true;
    placementSettleTimer.stop();
    // A maximized window would keep its maximized geometry.
    window.setWindowStates(Qt::WindowNoState);
    setFrameless(true);
    window.setGeometry(screen->geometry());
    window.setVisible(true);
    if (entering)
        emit fullscreenChanged(true);
}

void WindowStateController::showWindowed() {
    const bool leaving = fullscreen;
    fullscreen = false;
    setFrameless(false);
    if (leaving)
        restorePlacement();
    window.setVisible(true);
    if (leaving) {
        emit fullscreenChanged(false);
    } else {
        // The display layout may have changed while the window was hidden
        // (standby); re-applying the placement is not needed for that.
        updateDisplay();
    }
}

void WindowStateController::toggleFullscreen() {
    if (fullscreen)
        showWindowed();
    else
        showFullscreen();
}

void WindowStateController::savePlacement() {
    placementSettleTimer.stop();
    if (window.isVisible() && !fullscreen && window.windowStates() == Qt::WindowNoState)
        current.geometry = window.geometry();
    updateDisplay();
    emit placementChanged(current);
}

// Changing the flags of a native window restyles it; only real changes are
// applied.
void WindowStateController::setFrameless(bool frameless) {
    if (window.flags().testFlag(Qt::FramelessWindowHint) == frameless)
        return;
    Qt::WindowFlags flags = window.flags();
    flags.setFlag(Qt::FramelessWindowHint, frameless);
    window.setFlags(flags);
}

void WindowStateController::restorePlacement() {
    window.setGeometry(visibleGeometry(current.geometry));
    window.setWindowStates(current.maximized ? Qt::WindowMaximized : Qt::WindowNoState);
}

void WindowStateController::onGeometryChanged() {
    if (fullscreen || !window.isVisible())
        return;
    placementSettleTimer.start();
}

void WindowStateController::onWindowStateChanged(Qt::WindowStates states) {
    // Only states the user chose while the window is shown windowed count;
    // minimizing keeps the state the window returns to.
    if (!window.isVisible() || fullscreen || states.testFlag(Qt::WindowMinimized))
        return;
    current.maximized = states.testFlag(Qt::WindowMaximized);
    placementSettleTimer.start();
}

void WindowStateController::onPlacementSettled() {
    if (fullscreen || !window.isVisible())
        return;
    const WindowPlacement previous = current;
    if (window.windowStates() == Qt::WindowNoState)
        current.geometry = window.geometry();
    updateDisplay();
    if (current != previous)
        emit placementChanged(current);
}

void WindowStateController::updateDisplay() {
    const qsizetype index = QGuiApplication::screens().indexOf(window.screen());
    if (index >= 0)
        current.display = static_cast<int>(index);
}

QScreen *WindowStateController::targetScreen() const {
    const QList<QScreen *> screens = QGuiApplication::screens();
    if (current.display >= 0 && current.display < screens.size())
        return screens.at(current.display);
    if (window.screen())
        return window.screen();
    return QGuiApplication::primaryScreen();
}
