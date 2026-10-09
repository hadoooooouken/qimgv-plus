#pragma once

#include <QFont>
#include <QString>

#include "gui/quick/bridges/themedata.h"

class ColorScheme;

// Builds the snapshot published by ThemeBridge from a colour scheme, the
// application font and the registered icon font family: every scheme colour,
// the text styles (UiMetrics::Typography), the control metrics
// (UiMetrics::ControlMetrics) and the dialog surfaces (DialogSurfaces).
//
// Pure: reads no global state, so the QML tests build the same snapshots
// from ThemeStore's schemes as the application does from Settings.
[[nodiscard]] ThemeSnapshot buildThemeSnapshot(const ColorScheme &scheme,
                                               const QFont &baseFont,
                                               const QString &iconFontFamily);
