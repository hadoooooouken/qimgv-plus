#pragma once

#include <QLocale>
#include <QString>

#include "gui/ports/shellport.h"
#include "settings_types.h"

// Everything the main window title is built from.
struct WindowTitleState {
    ShellFileInfo info;
    ViewMode viewMode = MODE_DOCUMENT;
    // Current zoom of the viewer, percent.
    int scalePercent = 0;
    // Settings::windowTitleExtendedInfo().
    bool extendedInfo = false;
    bool zoomLocked = false;
    bool viewLocked = false;
};

// Title of the main window for state, with the rules of the widget UI
// (MW::onInfoUpdated): "Folder view" in folder mode, the application name
// without a file, otherwise the file name, zoom, optional extended details
// (position, resolution and aspect ratio, colour profile, format, file size)
// and the active states. Strings use the "MW" translation context, so the
// existing translations apply. locale formats the file size.
[[nodiscard]] QString windowTitleFor(const WindowTitleState &state,
                                     const QLocale &locale);
