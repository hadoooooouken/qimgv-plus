#pragma once
#include <QColor>
#include <QPixmap>
#include <QString>

// Identifiers for glyphs used from res/fonts/FluentSystemIcons-Custom.ttf.
// Only Regular weight is supported for now.
//
// The enum is not persisted; keep it limited to glyphs used by the UI.
enum class FluentIcon {
    BookmarkAdd20,
    Adjustments20,
    AiUpscale20,
    ArrowAutofitHeight20,
    ArrowAutofitWidth20,
    ArrowExit20,
    ArrowExpand20,
    ArrowLeft,
    ArrowNext20,
    ArrowPrevious20,
    ArrowRotateClockwise20,
    ArrowRotateCounterclockwise20,
    ArrowSort16,
    BatchConvert16,
    BatchConvert20,
    Blur20,
    CheckboxChecked16,
    CheckboxIndeterminate16,
    CheckboxUnchecked16,
    Checkmark16,
    ChevronDown12,
    ChevronLeft48,
    ChevronRight48,
    CopyAdd20,
    Crop20,
    Crop48,
    Delete20,
    Dismiss16,
    Dismiss20,
    DocumentView20,
    ErrorCircle20,
    Edit20,
    FlipHorizontal20,
    FlipVertical20,
    Folder16,
    Folder20,
    FolderAdd,
    Grid20,
    Home20,
    Info20,
    Move20,
    ChevronDown20,
    OpenWith20,
    OpenOnlySelected20,
    PanelLeft20,
    Panorama20,
    Pin20,
    Print20,
    RadioButton16,
    Record16,
    Rename20,
    Resize20,
    Settings20,
    ShowInFolder20,
    BookmarkRemove20,
    Wallpaper20,
    Warning20,
    ZoomIn20,
    ZoomOriginal20,
    ZoomOut20,
    ChevronUp20,
    Settings32,
    Eye32,
    ColorFill32,
    Controls32,
    Scripts32,
    WrenchScrewdriver32,
    BrainSparkle32,
    Info32,
    ChevronUp16,
    CheckmarkCircle20,
    Clock24,
    ClockDismiss24,
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
