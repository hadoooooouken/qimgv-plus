#include "appsettingsstore.h"

#include "components/actionmanager/actionmanager.h"
#include "components/cache/thumbnailcache.h"
#include "components/scriptmanager/scriptmanager.h"
#include "components/settingseditor/settingsscales.h"
#include "settings.h"

// The dialog's ranges are the limits of Settings.
static_assert(SettingsScales::kMinThumbnailerThreads == Settings::MinThumbnailerThreads);
static_assert(SettingsScales::kMaxThumbnailerThreads == Settings::MaxThumbnailerThreads);
static_assert(SettingsScales::kMinPanelHideDelayMs == Settings::MinPanelHideDelayMs);
static_assert(SettingsScales::kMaxPanelHideDelayMs == Settings::MaxPanelHideDelayMs);
static_assert(SettingsScales::kPanelHideDelayStepMs == Settings::PanelHideDelayStepMs);

namespace {
using namespace SettingsScales;

ColorScheme schemeWithAccent(const ColorScheme &scheme, const QColor &accent) {
    BaseColorScheme base;
    base.accent = accent;
    base.background = scheme.background;
    base.background_fullscreen = scheme.background_fullscreen;
    base.text = scheme.text;
    base.icons = scheme.icons;
    base.folder_icons = scheme.folder_icons;
    base.thumbnail_folder_icons = scheme.thumbnail_folder_icons;
    base.widget = scheme.widget;
    base.widget_border = scheme.widget_border;
    base.folderview = scheme.folderview;
    base.folderview_topbar = scheme.folderview_topbar;
    base.thumbpanel = scheme.thumbpanel;
    base.scrollbar = scheme.scrollbar;
    base.overlay = scheme.overlay;
    base.overlay_text = scheme.overlay_text;
    base.status_pending = scheme.status_pending;
    base.status_error = scheme.status_error;
    base.status_processing = scheme.status_processing;
    base.status_success = scheme.status_success;
    base.danger = scheme.danger;
    base.trash = scheme.trash;
    base.tid = scheme.tid;
    return ColorScheme(base);
}
} // namespace

//------------------------------------------------------------------------------
AppSettingsStore::AppSettingsStore(Settings &settings, ActionManager &actions,
                                   ScriptManager &scripts, QObject *parent)
    : QObject(parent), mSettings(settings), mActions(actions), mScripts(scripts) {}

SettingsValues AppSettingsStore::load() {
    Settings &s = mSettings;
    SettingsValues v;

    GeneralSettings &general = v.general;
    general.language = s.language();
    general.fullscreenMode = s.fullscreenMode();
    general.startInFolderView = s.defaultViewMode() == MODE_FOLDERVIEW;
    general.standbyMode = s.standbyMode();
    general.rememberLastFolder = s.rememberLastFolder();
    general.windowTitleExtendedInfo = s.windowTitleExtendedInfo();
    general.infoBarFullscreen = s.infoBarFullscreen();
    general.cursorAutohide = s.cursorAutohide();
    general.smoothScroll = s.enableSmoothScroll();
    general.smoothZoom = s.enableSmoothZoom();
    general.zoomIndicatorMode = s.zoomIndicatorMode();
    general.autoResizeWindow = s.autoResizeWindow();
    general.autoResizeLimitStep = autoResizeLimitStep(s.autoResizeLimit());
    general.panelEnabled = s.panelEnabled();
    general.squareThumbnails = s.squareThumbnails();
    general.panelPinned = s.panelPinned();
    general.panelFullscreenOnly = s.panelFullscreenOnly();
    general.panelCenterSelection = s.panelCenterSelection();
    general.showSubfoldersInPanel = s.showSubfoldersInPanel();
    general.panelHideDelayMs = s.panelHideDelayMs();
    general.thumbPanelStyle = s.thumbPanelStyle();
    general.panelSizeStep = panelSizeStep(s.panelPreviewsSize());
    general.panelPosition = s.panelPosition();
    general.folderEndAction = s.folderEndAction();
    general.sortingMode = s.sortingMode();
    general.sortFolders = s.sortFolders();
    general.showHiddenFiles = s.showHiddenFiles();
    general.slideshowIntervalMs = s.slideshowInterval();
    general.loopSlideshow = s.loopSlideshow();

    ViewSettings &view = v.view;
    view.imageFitMode = s.imageFitMode();
    view.keepFitMode = s.keepFitMode();
    view.focusPoint = s.focusPointIn1to1Mode();
    view.transparencyGrid = s.transparencyGrid();
    view.expandImage = s.expandImage();
    view.expandLimit = s.expandLimit();
    view.unlockMinZoom = s.unlockMinZoom();
    view.zoomStepPercent = zoomStepPercent(s.zoomStep());
    view.useFixedZoomLevels = s.useFixedZoomLevels();
    view.zoomLevels = s.zoomLevels();
    view.scalingFilter = s.scalingFilter();
    view.casSharpeningPercent = casPercent(s.casSharpening());
    view.casContrastPercent = casPercent(s.casContrast());
    view.colorManagementEnabled = s.colorManagementEnabled();
    view.monitorProfileType = s.monitorColorProfileType();
    view.monitorProfilePath = s.monitorColorProfilePath();
    view.hdrToneMappingEnabled = s.hdrToneMappingEnabled();
    view.hdrOperator = s.hdrToneMappingOperator();
    view.hdrTargetWhiteLevel = s.hdrTargetWhiteLevel();

    ThemeSettings &theme = v.theme;
    theme.themeMode = s.themeMode();
    theme.customAccent = s.hasCustomAccent();
    theme.accentColor = s.colorScheme().accent;
    theme.backgroundOpacityPercent = opacityPercent(s.backgroundOpacity());
    theme.thumbnailOpacityPercent = opacityPercent(s.thumbnailOpacity());
    theme.useBlackBackground = s.useBlackBackground();

    ControlsSettings &controls = v.controls;
    controls.clickableEdges = s.clickableEdges();
    controls.clickableEdgesVisible = s.clickableEdgesVisible();
    controls.imageScrolling = s.imageScrolling();
    controls.mouseScrollingSpeedStep = mouseScrollingSpeedStep(s.mouseScrollingSpeed());
    controls.trackpadDetection = s.trackpadDetection();

    AdvancedSettings &advanced = v.advanced;
    advanced.usePreloader = s.usePreloader();
    advanced.thumbnailerThreads = s.thumbnailerThreadCount();
    advanced.useThumbnailCache = s.useThumbnailCache();
    advanced.thumbnailResolution = s.thumbnailResolution();
    advanced.thumbnailCacheMaxSizeMB = s.thumbnailCacheMaxSizeMB();
    advanced.excludedCachePaths = s.excludedCachePaths();
    advanced.unloadThumbs = s.unloadThumbs();
    advanced.showSaveOverlay = s.showSaveOverlay();
    advanced.jpegQuality = s.JPEGSaveQuality();
    advanced.modernQuality = s.modernSaveQuality();
    advanced.pngCompression = s.pngSaveQuality();
    advanced.confirmTrash = s.confirmTrash();
    advanced.confirmDelete = s.confirmDelete();
    advanced.multiInstance = s.multiInstance();
    advanced.memoryLimitMB = s.memoryAllocationLimit();

    UpscaleSettings &upscale = v.upscale;
    upscale.useUpscayl = s.useUpscayl();
    upscale.model = s.upscaylModel();
    upscale.preloadUpscayl = s.preloadUpscayl();
    upscale.limitEnabled = s.upscaylLimitEnabled();
    upscale.limitPercent = s.upscaylLimitValue();
    return v;
}

SettingsEnvironment AppSettingsStore::environment() {
    return {.upscaylModels = mSettings.availableUpscaylModels(),
            .defaultUpscaylModel = Settings::defaultUpscaylModel(),
            .defaultZoomLevels = mSettings.defaultZoomLevels()};
}

void AppSettingsStore::apply(const SettingsValues &values, const ShortcutList &shortcuts) {
    storeGeneral(values.general);
    storeView(values.view);
    storeControls(values.controls);
    storeAdvanced(values.advanced);
    storeUpscale(values.upscale);
    storeTheme(values.theme);
    storeShortcuts(mActions, shortcuts);
    mScripts.saveScripts();
    mActions.saveShortcuts();
    mSettings.sendChangeNotification();
}

void AppSettingsStore::storeGeneral(const GeneralSettings &general) {
    Settings &s = mSettings;
    s.setLanguage(general.language);
    s.setFullscreenMode(general.fullscreenMode);
    s.setDefaultViewMode(general.startInFolderView ? MODE_FOLDERVIEW : MODE_DOCUMENT);
    s.setStandbyMode(general.standbyMode);
    s.setRememberLastFolder(general.rememberLastFolder);
    s.setWindowTitleExtendedInfo(general.windowTitleExtendedInfo);
    s.setInfoBarFullscreen(general.infoBarFullscreen);
    s.setCursorAutohide(general.cursorAutohide);
    s.setEnableSmoothScroll(general.smoothScroll);
    s.setEnableSmoothZoom(general.smoothZoom);
    s.setZoomIndicatorMode(static_cast<ZoomIndicatorMode>(general.zoomIndicatorMode));
    s.setAutoResizeWindow(general.autoResizeWindow);
    s.setAutoResizeLimit(autoResizeLimitPercent(general.autoResizeLimitStep));
    s.setPanelEnabled(general.panelEnabled);
    s.setSquareThumbnails(general.squareThumbnails);
    s.setPanelPinned(general.panelPinned);
    s.setPanelFullscreenOnly(general.panelFullscreenOnly);
    s.setPanelCenterSelection(general.panelCenterSelection);
    s.setShowSubfoldersInPanel(general.showSubfoldersInPanel);
    s.setPanelHideDelayMs(general.panelHideDelayMs);
    s.setThumbPanelStyle(static_cast<ThumbPanelStyle>(general.thumbPanelStyle));
    s.setPanelPreviewsSize(panelSizePixels(general.panelSizeStep));
    s.setPanelPosition(static_cast<PanelPosition>(general.panelPosition));
    s.setFolderEndAction(static_cast<FolderEndAction>(general.folderEndAction));
    s.setSortingMode(static_cast<SortingMode>(general.sortingMode));
    s.setSortFolders(general.sortFolders);
    s.setShowHiddenFiles(general.showHiddenFiles);
    s.setSlideshowInterval(general.slideshowIntervalMs);
    s.setLoopSlideshow(general.loopSlideshow);
}

void AppSettingsStore::storeView(const ViewSettings &view) {
    Settings &s = mSettings;
    s.setImageFitMode(static_cast<ImageFitMode>(view.imageFitMode));
    s.setKeepFitMode(view.keepFitMode);
    s.setFocusPointIn1to1Mode(static_cast<ImageFocusPoint>(view.focusPoint));
    s.setTransparencyGrid(view.transparencyGrid);
    s.setExpandImage(view.expandImage);
    s.setExpandLimit(view.expandLimit);
    s.setUnlockMinZoom(view.unlockMinZoom);
    s.setZoomStep(zoomStep(view.zoomStepPercent));
    s.setUseFixedZoomLevels(view.useFixedZoomLevels);
    s.setZoomLevels(view.zoomLevels);
    s.setScalingFilter(static_cast<ScalingFilter>(view.scalingFilter));
    s.setCasSharpening(casValue(view.casSharpeningPercent));
    s.setCasContrast(casValue(view.casContrastPercent));
    s.setColorManagementEnabled(view.colorManagementEnabled);
    s.setMonitorColorProfileType(view.monitorProfileType);
    s.setMonitorColorProfilePath(view.monitorProfilePath);
    s.setHdrToneMappingEnabled(view.hdrToneMappingEnabled);
    s.setHdrToneMappingOperator(view.hdrOperator);
    s.setHdrTargetWhiteLevel(view.hdrTargetWhiteLevel);
}

void AppSettingsStore::storeTheme(const ThemeSettings &theme) {
    Settings &s = mSettings;
    s.setBackgroundOpacity(opacity(theme.backgroundOpacityPercent));
    s.setThemeMode(static_cast<ThemeMode>(theme.themeMode));
    s.setThumbnailOpacity(opacity(theme.thumbnailOpacityPercent));
    s.setUseBlackBackground(theme.useBlackBackground);
    s.setHasCustomAccent(theme.customAccent);
    if (theme.customAccent)
        s.setColorScheme(schemeWithAccent(s.colorScheme(), theme.accentColor));
    else
        s.clearCustomAccent();
    s.saveTheme();
}

void AppSettingsStore::storeControls(const ControlsSettings &controls) {
    Settings &s = mSettings;
    s.setClickableEdges(controls.clickableEdges);
    s.setClickableEdgesVisible(controls.clickableEdgesVisible);
    s.setImageScrolling(static_cast<ImageScrolling>(controls.imageScrolling));
    s.setMouseScrollingSpeed(mouseScrollingSpeed(controls.mouseScrollingSpeedStep));
    s.setTrackpadDetection(controls.trackpadDetection);
}

void AppSettingsStore::storeAdvanced(const AdvancedSettings &advanced) {
    Settings &s = mSettings;
    s.setUsePreloader(advanced.usePreloader);
    s.setThumbnailerThreadCount(advanced.thumbnailerThreads);
    s.setUseThumbnailCache(advanced.useThumbnailCache);
    // Changing the resolution invalidates the thumbnail cache, so an
    // unchanged one is not written again.
    if (s.thumbnailResolution() != advanced.thumbnailResolution)
        s.setThumbnailResolution(advanced.thumbnailResolution);
    s.setThumbnailCacheMaxSizeMB(advanced.thumbnailCacheMaxSizeMB);
    s.setExcludedCachePaths(advanced.excludedCachePaths);
    s.setUnloadThumbs(advanced.unloadThumbs);
    s.setShowSaveOverlay(advanced.showSaveOverlay);
    s.setJPEGSaveQuality(advanced.jpegQuality);
    s.setModernSaveQuality(advanced.modernQuality);
    s.setPngSaveQuality(advanced.pngCompression);
    s.setConfirmTrash(advanced.confirmTrash);
    s.setConfirmDelete(advanced.confirmDelete);
    s.setMultiInstance(advanced.multiInstance);
    s.setMemoryAllocationLimit(advanced.memoryLimitMB);
}

void AppSettingsStore::storeUpscale(const UpscaleSettings &upscale) {
    Settings &s = mSettings;
    s.setUseUpscayl(upscale.useUpscayl);
    s.setUpscaylModel(upscale.model);
    s.setPreloadUpscayl(upscale.preloadUpscayl);
    s.setUpscaylLimitEnabled(upscale.limitEnabled);
    s.setUpscaylLimitValue(upscale.limitPercent);
}

//------------------------------------------------------------------------------
QColor AppSettingsStore::previewThemeMode(int themeMode) {
    mSettings.setThemeMode(static_cast<ThemeMode>(themeMode));
    mSettings.loadTheme();
    mSettings.sendChangeNotification();
    return mSettings.colorScheme().accent;
}

QColor AppSettingsStore::previewBlackBackground(bool useBlackBackground) {
    mSettings.setUseBlackBackground(useBlackBackground);
    mSettings.loadTheme();
    mSettings.sendChangeNotification();
    return mSettings.colorScheme().accent;
}

void AppSettingsStore::previewThumbnailOpacity(qreal opacity) {
    mSettings.setThumbnailOpacity(opacity);
    mSettings.loadTheme();
    mSettings.sendChangeNotification();
}

QColor AppSettingsStore::previewAccent(std::optional<QColor> customAccent) {
    if (customAccent)
        saveCustomAccent(*customAccent);
    else
        mSettings.clearCustomAccent();
    mSettings.sendChangeNotification();
    return mSettings.colorScheme().accent;
}

void AppSettingsStore::saveCustomAccent(const QColor &customAccent) {
    mSettings.setHasCustomAccent(true);
    mSettings.setColorScheme(schemeWithAccent(mSettings.colorScheme(), customAccent));
    mSettings.saveTheme();
}

//------------------------------------------------------------------------------
qint64 AppSettingsStore::thumbnailCacheBytes() {
    return ThumbnailCache::currentDiskUsageBytes();
}

void AppSettingsStore::clearThumbnailCache() {
    emit clearThumbnailCacheRequested();
}

//------------------------------------------------------------------------------
AppShortcutStore::AppShortcutStore(ActionManager &actions, ScriptManager &scripts)
    : mActions(actions), mScripts(scripts) {}

ShortcutList AppShortcutStore::shortcuts() {
    ShortcutList list;
    const QMap<QString, QString> &all = mActions.allShortcuts(); // <shortcut, action>
    list.reserve(all.size());
    for (auto it = all.cbegin(); it != all.cend(); ++it)
        list.append({.action = it.value(), .shortcut = it.key()});
    return list;
}

ShortcutList AppShortcutStore::resetShortcuts() {
    mActions.resetDefaults();
    return shortcuts();
}

QString AppShortcutStore::actionForShortcut(const QString &shortcut) {
    return mActions.actionForShortcut(shortcut);
}

QStringList AppShortcutStore::actionNames() {
    return mActions.actionList();
}

QStringList AppShortcutStore::scriptNames() {
    return mScripts.scriptNames();
}

bool AppShortcutStore::scriptExists(const QString &name) {
    return mScripts.scriptExists(name);
}

ScriptDefinition AppShortcutStore::script(const QString &name) {
    const Script script = mScripts.getScript(name);
    return {.command = script.command, .blocking = script.blocking};
}

void AppShortcutStore::addScript(const QString &name, const ScriptDefinition &script) {
    mScripts.addScript(name, Script(script.command, script.blocking));
}

ShortcutList AppShortcutStore::removeScript(const QString &name, const ShortcutList &table) {
    storeShortcuts(mActions, table);
    mActions.removeAllShortcuts(kScriptActionPrefix.toString() + name);
    const ShortcutList remaining = shortcuts();
    mScripts.removeScript(name);
    return remaining;
}

//------------------------------------------------------------------------------
void storeShortcuts(ActionManager &actions, const ShortcutList &table) {
    actions.removeAllShortcuts();
    for (const ShortcutEntry &entry : table)
        actions.addShortcut(entry.shortcut, entry.action);
}
