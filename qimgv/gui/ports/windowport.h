#pragma once

#include <qwindowdefs.h>

// Outbound UI port for the top-level application window. Must only be
// called on the GUI thread.
class IWindowPort {
public:
    virtual ~IWindowPort() = default;

    // Shows the window in its configured state (fullscreen or windowed);
    // does nothing when it is already visible.
    virtual void showWindow() = 0;
    virtual void hideWindow() = 0;
    [[nodiscard]] virtual bool isWindowVisible() const = 0;
    // A concealed window stays mapped but fully transparent, so it can be
    // laid out and painted before it is revealed.
    virtual void setWindowConcealed(bool concealed) = 0;
    virtual void raiseAndActivateWindow() = 0;
    [[nodiscard]] virtual WId nativeWindowHandle() const = 0;
    virtual void saveWindowGeometry() = 0;
    // Suspends painting while several changes are applied; resuming repaints
    // the window immediately.
    virtual void setWindowUpdatesSuspended(bool suspended) = 0;

protected:
    IWindowPort() = default;
    IWindowPort(const IWindowPort &) = default;
    IWindowPort &operator=(const IWindowPort &) = default;
};
