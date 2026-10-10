#pragma once
#include <QColor>
#include <QPixmap>
#include <QString>

#include "utils/fluenticon.h"

// Renders glyphs from the bundled Fluent System Icons font into QPixmap,
// as a replacement for the PNG-based icon set.
//
// Thread affinity: identical to the pixmap-loading code it replaces
// (IconWidget::loadIcon(), StyledComboBox's icon handling, etc.) - the font
// is a QGuiApplication-wide resource and QPainter/QPixmap rendering is only
// valid on the GUI thread. All IconFontManager calls must happen on the GUI
// thread.
class IconFontManager {
public:
    // Registers the bundled font from the Qt resource. Must be called once,
    // after the QGuiApplication instance is constructed, before any glyph is
    // rendered. Returns false (and logs a warning) if the font could not be
    // loaded - callers should treat this as non-fatal, since IconWidget/
    // StyledComboBox fall back to drawing nothing rather than crashing.
    static bool init();

    // Renders (or returns a cached render of) the given glyph as a square
    // pixmap. sizePx is the logical (non-DPI-scaled) side length of the
    // square the glyph is drawn into; dpr is the target devicePixelRatio.
    // The returned QPixmap already has its device pixel ratio set, matching
    // the convention used by the current @2x pixmap loading code.
    // Family name of the registered icon font, or an empty string before a
    // successful init().
    static const QString &family();

    static QPixmap pixmap(FluentIcon icon, int sizePx, QColor color, qreal dpr = 1.0);

private:
    static QString fontFamily;
    static bool initialized;
};
