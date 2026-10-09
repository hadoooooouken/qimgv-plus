#pragma once

#include <QLocale>
#include <QSize>
#include <QString>

#include "gui/ports/shellport.h"
#include "settings_types.h"

// Texts about the current file shared by the window title and the fullscreen
// info bar, with the formats of the widget UI (MW::onInfoUpdated).

// "[ index/count ]", or an empty string without files.
[[nodiscard]] QString filePositionText(const ShellFileInfo &info);
// "width x height (a:b)", or an empty string without a size.
[[nodiscard]] QString imageResolutionText(QSize size);
// File size with one decimal (locale formats it), or an empty string.
[[nodiscard]] QString fileSizeText(qint64 bytes, const QLocale &locale);

// The three parts of the fullscreen info bar.
struct FullscreenInfoText {
    // "[ index/count ]".
    QString position;
    // File name, "  *" appended while edited; "No file opened." without a
    // file and in folder view.
    QString name;
    // Resolution, colour profile, format and file size, separated by two
    // spaces.
    QString details;

    friend bool operator==(const FullscreenInfoText &, const FullscreenInfoText &) = default;
};

// Info bar texts for info. Strings use the "MW" translation context, so the
// existing translations apply.
[[nodiscard]] FullscreenInfoText fullscreenInfoFor(const ShellFileInfo &info,
                                                   ViewMode viewMode,
                                                   const QLocale &locale);
