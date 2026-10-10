#pragma once

#include <QColor>
#include <QList>
#include <QMetaType>
#include <QString>
#include <QStringList>

// The values the settings dialog edits, one value object per page of the
// dialog, independent of Settings and of any UI. Values shown by a slider
// keep the slider's units (see SettingsScales), so the dialog stores exactly
// the values the former widget dialog stored. Choices of the legacy settings enums
// (settings_types.h) are kept as their int values: the enums are unscoped
// and cannot be registered with the meta-object system; the option lists of
// SettingsOptions carry the valid values.
//
// QML edits a page through value type write-back:
// `editor.general.panelEnabled = checked` writes the whole page back.

// Page "General".
struct GeneralSettings {
    Q_GADGET
    Q_PROPERTY(QString language MEMBER language FINAL)
    Q_PROPERTY(bool fullscreenMode MEMBER fullscreenMode FINAL)
    Q_PROPERTY(bool startInFolderView MEMBER startInFolderView FINAL)
    Q_PROPERTY(bool standbyMode MEMBER standbyMode FINAL)
    Q_PROPERTY(bool rememberLastFolder MEMBER rememberLastFolder FINAL)
    Q_PROPERTY(bool windowTitleExtendedInfo MEMBER windowTitleExtendedInfo FINAL)
    Q_PROPERTY(bool infoBarFullscreen MEMBER infoBarFullscreen FINAL)
    Q_PROPERTY(bool cursorAutohide MEMBER cursorAutohide FINAL)
    Q_PROPERTY(bool smoothScroll MEMBER smoothScroll FINAL)
    Q_PROPERTY(bool smoothZoom MEMBER smoothZoom FINAL)
    Q_PROPERTY(int zoomIndicatorMode MEMBER zoomIndicatorMode FINAL)
    Q_PROPERTY(bool autoResizeWindow MEMBER autoResizeWindow FINAL)
    Q_PROPERTY(int autoResizeLimitStep MEMBER autoResizeLimitStep FINAL)
    Q_PROPERTY(bool panelEnabled MEMBER panelEnabled FINAL)
    Q_PROPERTY(bool squareThumbnails MEMBER squareThumbnails FINAL)
    Q_PROPERTY(bool panelPinned MEMBER panelPinned FINAL)
    Q_PROPERTY(bool panelFullscreenOnly MEMBER panelFullscreenOnly FINAL)
    Q_PROPERTY(bool panelCenterSelection MEMBER panelCenterSelection FINAL)
    Q_PROPERTY(bool showSubfoldersInPanel MEMBER showSubfoldersInPanel FINAL)
    Q_PROPERTY(int panelHideDelayMs MEMBER panelHideDelayMs FINAL)
    Q_PROPERTY(int thumbPanelStyle MEMBER thumbPanelStyle FINAL)
    Q_PROPERTY(int panelSizeStep MEMBER panelSizeStep FINAL)
    Q_PROPERTY(int panelPosition MEMBER panelPosition FINAL)
    Q_PROPERTY(int folderEndAction MEMBER folderEndAction FINAL)
    Q_PROPERTY(int sortingMode MEMBER sortingMode FINAL)
    Q_PROPERTY(bool sortFolders MEMBER sortFolders FINAL)
    Q_PROPERTY(bool showHiddenFiles MEMBER showHiddenFiles FINAL)
    Q_PROPERTY(int slideshowIntervalMs MEMBER slideshowIntervalMs FINAL)
    Q_PROPERTY(bool loopSlideshow MEMBER loopSlideshow FINAL)

public:
    // Language code ("en_US") or "system".
    QString language;
    bool fullscreenMode = false;
    bool startInFolderView = false;
    bool standbyMode = false;
    bool rememberLastFolder = false;
    bool windowTitleExtendedInfo = false;
    bool infoBarFullscreen = false;
    bool cursorAutohide = false;
    bool smoothScroll = false;
    bool smoothZoom = false;
    int zoomIndicatorMode = 0;   // ZoomIndicatorMode
    bool autoResizeWindow = false;
    int autoResizeLimitStep = 0; // SettingsScales::autoResizeLimit*
    bool panelEnabled = false;
    bool squareThumbnails = false;
    bool panelPinned = false;
    bool panelFullscreenOnly = false;
    bool panelCenterSelection = false;
    bool showSubfoldersInPanel = false;
    int panelHideDelayMs = 0;
    int thumbPanelStyle = 0;     // ThumbPanelStyle
    int panelSizeStep = 0;       // SettingsScales::panelSize*
    int panelPosition = 0;       // PanelPosition
    int folderEndAction = 0;     // FolderEndAction
    int sortingMode = 0;         // SortingMode
    bool sortFolders = false;
    bool showHiddenFiles = false;
    int slideshowIntervalMs = 0;
    bool loopSlideshow = false;

    friend bool operator==(const GeneralSettings &, const GeneralSettings &) = default;
};

// Page "View", with colour management and HDR tone mapping.
struct ViewSettings {
    Q_GADGET
    Q_PROPERTY(int imageFitMode MEMBER imageFitMode FINAL)
    Q_PROPERTY(bool keepFitMode MEMBER keepFitMode FINAL)
    Q_PROPERTY(int focusPoint MEMBER focusPoint FINAL)
    Q_PROPERTY(bool transparencyGrid MEMBER transparencyGrid FINAL)
    Q_PROPERTY(bool expandImage MEMBER expandImage FINAL)
    Q_PROPERTY(int expandLimit MEMBER expandLimit FINAL)
    Q_PROPERTY(bool unlockMinZoom MEMBER unlockMinZoom FINAL)
    Q_PROPERTY(int zoomStepPercent MEMBER zoomStepPercent FINAL)
    Q_PROPERTY(bool useFixedZoomLevels MEMBER useFixedZoomLevels FINAL)
    Q_PROPERTY(QString zoomLevels MEMBER zoomLevels FINAL)
    Q_PROPERTY(int scalingFilter MEMBER scalingFilter FINAL)
    Q_PROPERTY(int casSharpeningPercent MEMBER casSharpeningPercent FINAL)
    Q_PROPERTY(int casContrastPercent MEMBER casContrastPercent FINAL)
    Q_PROPERTY(bool colorManagementEnabled MEMBER colorManagementEnabled FINAL)
    Q_PROPERTY(QString monitorProfileType MEMBER monitorProfileType FINAL)
    Q_PROPERTY(QString monitorProfilePath MEMBER monitorProfilePath FINAL)
    Q_PROPERTY(bool hdrToneMappingEnabled MEMBER hdrToneMappingEnabled FINAL)
    Q_PROPERTY(int hdrOperator MEMBER hdrOperator FINAL)
    Q_PROPERTY(int hdrTargetWhiteLevel MEMBER hdrTargetWhiteLevel FINAL)

public:
    int imageFitMode = 0;        // ImageFitMode
    bool keepFitMode = false;
    int focusPoint = 0;          // ImageFocusPoint
    bool transparencyGrid = false;
    bool expandImage = false;
    int expandLimit = 0;         // factor, 0 for no limit
    bool unlockMinZoom = false;
    int zoomStepPercent = 0;     // SettingsScales::zoomStep*
    bool useFixedZoomLevels = false;
    QString zoomLevels;
    int scalingFilter = 0;       // ScalingFilter
    int casSharpeningPercent = 0;
    int casContrastPercent = 0;
    bool colorManagementEnabled = false;
    // Monitor profile key of SettingsOptions::monitorProfiles() ("System").
    QString monitorProfileType;
    QString monitorProfilePath;
    bool hdrToneMappingEnabled = false;
    int hdrOperator = 0;
    int hdrTargetWhiteLevel = 0; // nits

    friend bool operator==(const ViewSettings &, const ViewSettings &) = default;
};

// Page "Theme". The theme mode, the black background, the thumbnail bar
// opacity and the custom accent are previewed (written to the settings) as
// soon as they change, as in the widget dialog.
struct ThemeSettings {
    Q_GADGET
    Q_PROPERTY(int themeMode MEMBER themeMode FINAL)
    Q_PROPERTY(bool customAccent MEMBER customAccent FINAL)
    Q_PROPERTY(QColor accentColor MEMBER accentColor FINAL)
    Q_PROPERTY(int backgroundOpacityPercent MEMBER backgroundOpacityPercent FINAL)
    Q_PROPERTY(int thumbnailOpacityPercent MEMBER thumbnailOpacityPercent FINAL)
    Q_PROPERTY(bool useBlackBackground MEMBER useBlackBackground FINAL)

public:
    int themeMode = 0;           // ThemeMode
    bool customAccent = false;
    QColor accentColor;
    int backgroundOpacityPercent = 0;
    int thumbnailOpacityPercent = 0;
    bool useBlackBackground = false;

    friend bool operator==(const ThemeSettings &, const ThemeSettings &) = default;
};

// Page "Controls", without the shortcut table (ShortcutTableModel).
struct ControlsSettings {
    Q_GADGET
    Q_PROPERTY(bool clickableEdges MEMBER clickableEdges FINAL)
    Q_PROPERTY(bool clickableEdgesVisible MEMBER clickableEdgesVisible FINAL)
    Q_PROPERTY(int imageScrolling MEMBER imageScrolling FINAL)
    Q_PROPERTY(int mouseScrollingSpeedStep MEMBER mouseScrollingSpeedStep FINAL)
    Q_PROPERTY(bool trackpadDetection MEMBER trackpadDetection FINAL)

public:
    bool clickableEdges = false;
    bool clickableEdgesVisible = false;
    int imageScrolling = 0;      // ImageScrolling
    int mouseScrollingSpeedStep = 0; // SettingsScales::mouseScrollingSpeed*
    bool trackpadDetection = false;

    friend bool operator==(const ControlsSettings &, const ControlsSettings &) = default;
};

// Page "Advanced".
struct AdvancedSettings {
    Q_GADGET
    Q_PROPERTY(bool usePreloader MEMBER usePreloader FINAL)
    Q_PROPERTY(int thumbnailerThreads MEMBER thumbnailerThreads FINAL)
    Q_PROPERTY(bool useThumbnailCache MEMBER useThumbnailCache FINAL)
    Q_PROPERTY(int thumbnailResolution MEMBER thumbnailResolution FINAL)
    Q_PROPERTY(int thumbnailCacheMaxSizeMB MEMBER thumbnailCacheMaxSizeMB FINAL)
    Q_PROPERTY(QString excludedCachePaths MEMBER excludedCachePaths FINAL)
    Q_PROPERTY(bool unloadThumbs MEMBER unloadThumbs FINAL)
    Q_PROPERTY(bool showSaveOverlay MEMBER showSaveOverlay FINAL)
    Q_PROPERTY(int jpegQuality MEMBER jpegQuality FINAL)
    Q_PROPERTY(int modernQuality MEMBER modernQuality FINAL)
    Q_PROPERTY(int pngCompression MEMBER pngCompression FINAL)
    Q_PROPERTY(bool confirmTrash MEMBER confirmTrash FINAL)
    Q_PROPERTY(bool confirmDelete MEMBER confirmDelete FINAL)
    Q_PROPERTY(bool multiInstance MEMBER multiInstance FINAL)
    Q_PROPERTY(int memoryLimitMB MEMBER memoryLimitMB FINAL)

public:
    bool usePreloader = false;
    int thumbnailerThreads = 0;
    bool useThumbnailCache = false;
    int thumbnailResolution = 0; // px
    int thumbnailCacheMaxSizeMB = 0;
    QString excludedCachePaths;
    bool unloadThumbs = false;
    bool showSaveOverlay = false;
    int jpegQuality = 0;
    int modernQuality = 0;
    int pngCompression = 0;
    bool confirmTrash = false;
    bool confirmDelete = false;
    bool multiInstance = false;
    int memoryLimitMB = 0;

    friend bool operator==(const AdvancedSettings &, const AdvancedSettings &) = default;
};

// Page "AI Upscale".
struct UpscaleSettings {
    Q_GADGET
    Q_PROPERTY(bool useUpscayl MEMBER useUpscayl FINAL)
    Q_PROPERTY(QString model MEMBER model FINAL)
    Q_PROPERTY(bool preloadUpscayl MEMBER preloadUpscayl FINAL)
    Q_PROPERTY(bool limitEnabled MEMBER limitEnabled FINAL)
    Q_PROPERTY(int limitPercent MEMBER limitPercent FINAL)

public:
    bool useUpscayl = false;
    QString model;
    bool preloadUpscayl = false;
    bool limitEnabled = false;
    int limitPercent = 0;

    friend bool operator==(const UpscaleSettings &, const UpscaleSettings &) = default;
};

// Every page of the dialog.
struct SettingsValues {
    GeneralSettings general;
    ViewSettings view;
    ThemeSettings theme;
    ControlsSettings controls;
    AdvancedSettings advanced;
    UpscaleSettings upscale;

    friend bool operator==(const SettingsValues &, const SettingsValues &) = default;
};

// What the dialog shows besides the values: the installed Upscayl models and
// the default zoom levels ("Load defaults").
struct SettingsEnvironment {
    QStringList upscaylModels;
    QString defaultUpscaylModel;
    QString defaultZoomLevels;
};

// One row of the shortcut table: the action ("s:<script>" for a script) and
// its shortcut ("Ctrl+R").
struct ShortcutEntry {
    QString action;
    QString shortcut;

    friend bool operator==(const ShortcutEntry &, const ShortcutEntry &) = default;
};

using ShortcutList = QList<ShortcutEntry>;

// One entry of the "Open with" scripts (Script in the application).
struct ScriptDefinition {
    QString command;
    bool blocking = false;

    friend bool operator==(const ScriptDefinition &, const ScriptDefinition &) = default;
};

// Prefix of the shortcut actions that run a script.
inline constexpr QStringView kScriptActionPrefix = u"s:";
