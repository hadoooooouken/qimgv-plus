#pragma once
#include <QColor>
#include <QPixmap>
#include <QString>

// Identifiers for glyphs used from res/fonts/FluentSystemIcons-Regular.ttf.
// Only Regular weight is supported for now.
//
// New values must be appended at the end - the enum itself is not persisted
// anywhere, but IconFontManager's internal codepoint table is indexed by
// name (not by ordinal), so ordering has no functional effect. Values are
// kept append-only regardless, to match the project's convention for
// settings-adjacent enums and to avoid gratuitous diff noise when the table
// is edited later.
enum class FluentIcon {
    Add12,
    Apps16,
    ArrowAutofitHeight,
    ArrowAutofitWidth,
    ArrowExit20,
    ArrowLeft,
    ArrowMaximize,
    ArrowMove,
    ArrowNext20,
    ArrowPrevious20,
    ArrowRotateClockwise,
    ArrowRotateCounterclockwise,
    ArrowSort16,
    ArrowUp16,
    Blur,
    CheckmarkCircle16,
    ChevronDown12,
    ChevronLeft48,
    ChevronRight48,
    Color,
    Color24,
    Copy,
    Crop16,
    Crop48,
    Delete16,
    Dismiss16,
    Dismiss20,
    Eye24,
    FlipHorizontal,
    FlipVertical,
    Folder,
    FolderAdd,
    FolderArrowRight,
    FolderOpen16,
    FolderOpen20,
    Grid16,
    Grid20,
    Home12,
    Image,
    Image16,
    ImageMultiple16,
    Info16,
    Info24,
    Keyboard24,
    PanelLeft20,
    Pin20,
    Print,
    Rename,
    Resize16,
    ResizeLarge24,
    Settings16,
    Settings20,
    Settings24,
    Subtract12,
    WindowDevTools24,
    Wrench24,
    ZoomIn,
    ZoomOriginal,
    ZoomOut,
};

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
    // after the QApplication instance is constructed, before any glyph is
    // rendered. Returns false (and logs a warning) if the font could not be
    // loaded - callers should treat this as non-fatal, since IconWidget/
    // StyledComboBox fall back to drawing nothing rather than crashing.
    static bool init();

    // Renders (or returns a cached render of) the given glyph as a square
    // pixmap. sizePx is the logical (non-DPI-scaled) side length of the
    // square the glyph is drawn into; dpr is the target devicePixelRatio.
    // The returned QPixmap already has its device pixel ratio set, matching
    // the convention used by the current @2x pixmap loading code.
    static QPixmap pixmap(FluentIcon icon, int sizePx, QColor color, qreal dpr = 1.0);

private:
    static QString fontFamily;
    static bool initialized;
};
