#pragma once

#include <QObject>

#include <optional>

// Catalogue of the Fluent System Icons glyphs used by the UI. It is
// toolkit-independent: the widget UI renders the glyphs through
// IconFontManager, the Qt Quick UI through the Theme bridge
// (gui/quick/bridges/themebridge.h). The enum lives in a Q_NAMESPACE so that
// QML can name the glyphs; the using-declaration below keeps the unqualified
// FluentIcon spelling for C++ users.
namespace FluentIcons {
Q_NAMESPACE

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
    FolderAdd20,
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
Q_ENUM_NS(FluentIcon)

// Regular-weight codepoint of icon in res/fonts/FluentSystemIcons-Custom.ttf,
// or std::nullopt when no glyph is registered for it.
[[nodiscard]] std::optional<char32_t> codepoint(FluentIcon icon);

} // namespace FluentIcons

using FluentIcons::FluentIcon;
