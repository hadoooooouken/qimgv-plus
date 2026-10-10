#pragma once

#include <QObject>
#include <QRect>
#include <QSize>
#include <QTimer>

class QScreen;
class QWindow;

// Persisted placement of the main window: the normal (restored) geometry
// without the frame, whether it was maximized, and the index of the display
// it was on (QGuiApplication::screens()).
struct WindowPlacement {
    QRect geometry;
    bool maximized = false;
    int display = 0;

    bool operator==(const WindowPlacement &) const = default;
};

// Input of windowGeometryFittingContent().
struct ContentFitRequest {
    // Size of the shown document; an empty size takes the whole limit.
    QSize contentSize;
    // Available area of the window's display.
    QRect availableGeometry;
    // Largest window, in percent of availableGeometry.
    int limitPercent = 100;
    // Height of the title bar and the borders around the window's content.
    int frameHeight = 0;
};

// Content geometry of a window that fits the shown document (the
// "autoResizeWindow" setting): the document size, scaled down with its aspect
// ratio kept to limitPercent of the available area less the frame, centred
// on that area together with the frame.
[[nodiscard]] QRect windowGeometryFittingContent(const ContentFitRequest &request);

// Window states of a top-level QWindow, independent of the UI toolkit: it
// restores the saved placement, remembers the maximized state and the
// display, and switches between the windowed state and pseudo-fullscreen (a
// frameless window covering the remembered display, as the widget UI does).
// Placement changes are reported through placementChanged() after the window
// has rested (moves and resizes are coalesced); the owner persists them.
//
// The window must outlive this controller. GUI thread only.
class WindowStateController final : public QObject {
    Q_OBJECT
public:
    // Moves and resizes are reported once the window has rested this long.
    static constexpr int kPlacementSettleDelayMs = 30;

    // Applies saved's geometry and maximized state to the (hidden) window.
    WindowStateController(QWindow &window, const WindowPlacement &saved,
                          QObject *parent = nullptr);

    [[nodiscard]] bool isFullscreen() const;
    [[nodiscard]] WindowPlacement placement() const;

    // Shows the window in the given state.
    void showFullscreen();
    void showWindowed();
    void toggleFullscreen();

    // Resizes a plain window (neither fullscreen, maximized nor minimized)
    // to fit a document of contentSize on its display, within limitPercent
    // of the display (windowGeometryFittingContent()). A hidden window keeps
    // the geometry for when it is shown, and it is reported at once.
    void fitToContent(QSize contentSize, int limitPercent);

    // Reports the current placement through placementChanged() now (for
    // example before the window is hidden or the application exits).
    void savePlacement();

signals:
    void placementChanged(const WindowPlacement &placement);
    void fullscreenChanged(bool fullscreen);

private:
    void setFrameless(bool frameless);
    void restorePlacement();
    void onGeometryChanged();
    void onWindowStateChanged(Qt::WindowStates states);
    void onPlacementSettled();
    void updateDisplay();
    [[nodiscard]] QScreen *targetScreen() const;

    QWindow &window;
    WindowPlacement current;
    bool fullscreen = false;
    QTimer placementSettleTimer;
};
