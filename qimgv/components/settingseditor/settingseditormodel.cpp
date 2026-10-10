#include "settingseditormodel.h"

#include <QCoreApplication>
#include <QDebug>
#include <QLocale>
#include <QUrl>

#include <algorithm>

#include "settings_types.h"
#include "settingsstores.h"

namespace {
using namespace Qt::StringLiterals;

constexpr char kContext[] = "SettingsDialog";

QString translated(const char *text) {
    return QCoreApplication::translate(kContext, text);
}

bool offers(const SettingsOptionList &options, const QVariant &value) {
    return SettingsOptions::indexOf(options, value) >= 0;
}

// value when options offer it, otherwise fallback.
template <typename T>
T offeredOr(const SettingsOptionList &options, const T &value, const T &fallback) {
    return offers(options, value) ? value : fallback;
}

constexpr int kFallbackScalingFilter = QI_FILTER_BILINEAR;

// value limited to range, as a slider or spin box shows it.
int bounded(int value, const SettingsRange &range) {
    return std::clamp(value, range.from, range.to);
}

// bounded() and moved to the range's step, as the snapping sliders do.
int boundedSnapped(int value, const SettingsRange &range) {
    return SettingsScales::snapped(bounded(value, range), range.step);
}
} // namespace

SettingsEditorModel::SettingsEditorModel(ISettingsValueStore &values,
                                         IShortcutScriptStore &shortcuts, QObject *parent)
    : QObject(parent),
      mValueStore(values),
      mShortcutStore(shortcuts),
      mShortcuts(this),
      mScripts(this),
      mShortcutEditor(shortcuts, this),
      mScriptEditor(shortcuts, this) {
    connect(&mShortcutEditor, &EditorSession::accepted, this,
            &SettingsEditorModel::onShortcutAccepted);
    connect(&mScriptEditor, &EditorSession::accepted, this,
            &SettingsEditorModel::onScriptAccepted);
    connect(this, &SettingsEditorModel::upscaleChanged, this,
            &SettingsEditorModel::upscaleStateChanged);
    connect(this, &SettingsEditorModel::environmentChanged, this,
            &SettingsEditorModel::upscaleStateChanged);
}

//--- pages --------------------------------------------------------------------
GeneralSettings SettingsEditorModel::general() const {
    return mValues.general;
}

void SettingsEditorModel::setGeneral(const GeneralSettings &general) {
    if (mValues.general == general)
        return;
    mValues.general = general;
    emit generalChanged();
}

ViewSettings SettingsEditorModel::view() const {
    return mValues.view;
}

void SettingsEditorModel::setView(const ViewSettings &view) {
    if (mValues.view == view)
        return;
    mValues.view = view;
    emit viewChanged();
}

ThemeSettings SettingsEditorModel::theme() const {
    return mValues.theme;
}

void SettingsEditorModel::setTheme(const ThemeSettings &theme) {
    if (mValues.theme == theme)
        return;
    mValues.theme = theme;
    emit themeChanged();
}

ControlsSettings SettingsEditorModel::controls() const {
    return mValues.controls;
}

void SettingsEditorModel::setControls(const ControlsSettings &controls) {
    if (mValues.controls == controls)
        return;
    mValues.controls = controls;
    emit controlsChanged();
}

AdvancedSettings SettingsEditorModel::advanced() const {
    return mValues.advanced;
}

void SettingsEditorModel::setAdvanced(const AdvancedSettings &advanced) {
    if (mValues.advanced == advanced)
        return;
    mValues.advanced = advanced;
    emit advancedChanged();
}

UpscaleSettings SettingsEditorModel::upscale() const {
    return mValues.upscale;
}

void SettingsEditorModel::setUpscale(const UpscaleSettings &upscale) {
    if (mValues.upscale == upscale)
        return;
    mValues.upscale = upscale;
    emit upscaleChanged();
}

SettingsRanges SettingsEditorModel::ranges() {
    return SettingsScales::ranges();
}

SettingsValues SettingsEditorModel::values() const {
    return mValues;
}

void SettingsEditorModel::setValues(const SettingsValues &values) {
    setGeneral(values.general);
    setView(values.view);
    setTheme(values.theme);
    setControls(values.controls);
    setAdvanced(values.advanced);
    setUpscale(values.upscale);
}

//--- options ------------------------------------------------------------------
QVariantList SettingsEditorModel::languages() {
    return SettingsOptions::toVariantList(SettingsOptions::languages());
}

QVariantList SettingsEditorModel::zoomIndicatorModes() {
    return SettingsOptions::toVariantList(SettingsOptions::zoomIndicatorModes());
}

QVariantList SettingsEditorModel::thumbPanelStyles() {
    return SettingsOptions::toVariantList(SettingsOptions::thumbPanelStyles());
}

QVariantList SettingsEditorModel::panelPositions() {
    return SettingsOptions::toVariantList(SettingsOptions::panelPositions());
}

QVariantList SettingsEditorModel::folderEndActions() {
    return SettingsOptions::toVariantList(SettingsOptions::folderEndActions());
}

QVariantList SettingsEditorModel::sortingModes() {
    return SettingsOptions::toVariantList(SettingsOptions::sortingModes());
}

QVariantList SettingsEditorModel::fitModes() {
    return SettingsOptions::toVariantList(SettingsOptions::fitModes());
}

QVariantList SettingsEditorModel::focusPoints() {
    return SettingsOptions::toVariantList(SettingsOptions::focusPoints());
}

QVariantList SettingsEditorModel::scalingFilters() {
    return SettingsOptions::toVariantList(SettingsOptions::scalingFilters());
}

QVariantList SettingsEditorModel::monitorProfiles() {
    return SettingsOptions::toVariantList(SettingsOptions::monitorProfiles());
}

QVariantList SettingsEditorModel::hdrOperators() {
    return SettingsOptions::toVariantList(SettingsOptions::hdrOperators());
}

QVariantList SettingsEditorModel::hdrTargetWhiteLevels() {
    return SettingsOptions::toVariantList(SettingsOptions::hdrTargetWhiteLevels());
}

QVariantList SettingsEditorModel::themeModes() {
    return SettingsOptions::toVariantList(SettingsOptions::themeModes());
}

QVariantList SettingsEditorModel::imageScrollingModes() {
    return SettingsOptions::toVariantList(SettingsOptions::imageScrollingModes());
}

//--- derived state ------------------------------------------------------------
QStringList SettingsEditorModel::upscaylModels() const {
    return mEnvironment.upscaylModels;
}

bool SettingsEditorModel::isUpscaylAvailable() const {
    return !mEnvironment.upscaylModels.isEmpty();
}

bool SettingsEditorModel::upscaylOptionsEnabled() const {
    return isUpscaylAvailable() && mValues.upscale.useUpscayl;
}

bool SettingsEditorModel::upscaylLimitSliderEnabled() const {
    return upscaylOptionsEnabled() && mValues.upscale.limitEnabled;
}

bool SettingsEditorModel::customProfileVisible() const {
    return mValues.view.colorManagementEnabled &&
           mValues.view.monitorProfileType == SettingsOptions::kCustomMonitorProfile;
}

bool SettingsEditorModel::casOptionsVisible() const {
    return mValues.view.scalingFilter == QI_FILTER_CAS;
}

QString SettingsEditorModel::thumbnailCacheSizeText() const {
    return QLocale().formattedDataSize(mThumbnailCacheBytes);
}

ShortcutTableModel *SettingsEditorModel::shortcutTable() {
    return &mShortcuts;
}

QStringListModel *SettingsEditorModel::scriptList() {
    return &mScripts;
}

ShortcutEditorModel *SettingsEditorModel::shortcutEditor() {
    return &mShortcutEditor;
}

ScriptEditorModel *SettingsEditorModel::scriptEditor() {
    return &mScriptEditor;
}

QString SettingsEditorModel::windowTitle() {
    return translated("Preferences — ") + QCoreApplication::applicationName();
}

QString SettingsEditorModel::aboutText() {
    return translated(
        "This is a fast and easy to use image viewer\n"
        "\n"
        "**Github page:** [https://github.com/hadoooooouken/qimgv-plus](https://github.com/hadoooooouken/qimgv-plus)\n"
        "\n"
        "**Original project:** [https://github.com/easymodo/qimgv](https://github.com/easymodo/qimgv)\n"
        "\n"
        "**Plus version developer:** [hadooooouken](https://github.com/hadoooooouken)\n"
        "\n"
        "**Original developer:** [easymodo](https://github.com/easymodo)\n"
        "\n"
        "[**Contributors**](https://github.com/hadoooooouken/qimgv-plus/graphs/contributors)\n"
        "\n"
        "qimgv is licensed under [GNU GPL Version 3](https://www.gnu.org/licenses/gpl-3.0.en.html)\n"
        "\n"
        "Report any issues / request features [here](https://github.com/hadoooooouken/qimgv-plus/issues)\n");
}

QString SettingsEditorModel::applicationVersion() {
    return QCoreApplication::applicationVersion();
}

QString SettingsEditorModel::qtVersion() {
    return QString::fromLatin1(qVersion());
}

QString SettingsEditorModel::colorProfileDialogTitle() {
    return translated("Select Monitor Color Profile");
}

QStringList SettingsEditorModel::colorProfileFilters() {
    return {translated("Color Profiles (*.icc *.icm)")};
}

//--- load / apply -------------------------------------------------------------
void SettingsEditorModel::load() {
    mEnvironment = mValueStore.environment();
    emit environmentChanged();
    SettingsValues loaded = mValueStore.load();
    normalize(loaded);
    setValues(loaded);
    mShortcuts.setEntries(mShortcutStore.shortcuts());
    refreshScripts();
    refreshThumbnailCacheSize();
}

void SettingsEditorModel::normalize(SettingsValues &values) const {
    values.general.language =
        offeredOr(SettingsOptions::languages(), values.general.language,
                  SettingsOptions::kFallbackLanguage.toString());
    values.view.scalingFilter = offeredOr(SettingsOptions::scalingFilters(),
                                          values.view.scalingFilter, kFallbackScalingFilter);

    const SettingsOptionList profiles = SettingsOptions::monitorProfiles();
    values.view.monitorProfileType = offeredOr(profiles, values.view.monitorProfileType,
                                               profiles.constFirst().value.toString());
    const SettingsOptionList operators = SettingsOptions::hdrOperators();
    values.view.hdrOperator =
        offeredOr(operators, values.view.hdrOperator, operators.constFirst().value.toInt());
    const SettingsOptionList whiteLevels = SettingsOptions::hdrTargetWhiteLevels();
    values.view.hdrTargetWhiteLevel = offeredOr(whiteLevels, values.view.hdrTargetWhiteLevel,
                                                whiteLevels.constFirst().value.toInt());

    const SettingsRanges r = SettingsScales::ranges();
    GeneralSettings &general = values.general;
    general.autoResizeLimitStep = bounded(general.autoResizeLimitStep, r.autoResizeLimit);
    general.panelHideDelayMs = boundedSnapped(general.panelHideDelayMs, r.panelHideDelay);
    general.panelSizeStep = bounded(general.panelSizeStep, r.panelSize);
    general.slideshowIntervalMs = bounded(general.slideshowIntervalMs, r.slideshowInterval);
    ViewSettings &view = values.view;
    view.expandLimit = bounded(view.expandLimit, r.expandLimit);
    view.zoomStepPercent = bounded(view.zoomStepPercent, r.zoomStep);
    view.casSharpeningPercent = bounded(view.casSharpeningPercent, r.casSharpening);
    view.casContrastPercent = bounded(view.casContrastPercent, r.casContrast);
    ThemeSettings &theme = values.theme;
    theme.backgroundOpacityPercent = bounded(theme.backgroundOpacityPercent, r.opacity);
    theme.thumbnailOpacityPercent = bounded(theme.thumbnailOpacityPercent, r.opacity);
    ControlsSettings &controls = values.controls;
    controls.mouseScrollingSpeedStep =
        bounded(controls.mouseScrollingSpeedStep, r.mouseScrollingSpeed);
    AdvancedSettings &advanced = values.advanced;
    advanced.thumbnailerThreads = bounded(advanced.thumbnailerThreads, r.thumbnailerThreads);
    advanced.thumbnailResolution =
        boundedSnapped(advanced.thumbnailResolution, r.thumbnailResolution);
    advanced.thumbnailCacheMaxSizeMB =
        bounded(advanced.thumbnailCacheMaxSizeMB, r.thumbnailCacheSize);
    advanced.jpegQuality = bounded(advanced.jpegQuality, r.quality);
    advanced.modernQuality = bounded(advanced.modernQuality, r.quality);
    advanced.pngCompression = bounded(advanced.pngCompression, r.pngCompression);
    advanced.memoryLimitMB = bounded(advanced.memoryLimitMB, r.memoryLimit);

    UpscaleSettings &upscale = values.upscale;
    upscale.limitPercent = boundedSnapped(upscale.limitPercent, r.upscaylLimit);
    const QStringList &models = mEnvironment.upscaylModels;
    if (models.isEmpty()) {
        upscale.useUpscayl = false;
        upscale.preloadUpscayl = false;
        upscale.limitEnabled = false;
    } else if (!models.contains(upscale.model)) {
        upscale.model = models.contains(mEnvironment.defaultUpscaylModel)
                            ? mEnvironment.defaultUpscaylModel
                            : models.constFirst();
    }
}

void SettingsEditorModel::apply() {
    mValueStore.apply(mValues, mShortcuts.entries());
}

//--- edits --------------------------------------------------------------------
void SettingsEditorModel::resetZoomLevels() {
    ViewSettings edited = mValues.view;
    edited.zoomLevels = mEnvironment.defaultZoomLevels;
    setView(edited);
}

void SettingsEditorModel::setMonitorProfileFile(const QUrl &url) {
    if (!url.isLocalFile()) {
        qWarning() << "SettingsEditorModel: not a local file:" << url;
        return;
    }
    ViewSettings edited = mValues.view;
    edited.monitorProfilePath = url.toLocalFile();
    setView(edited);
}

void SettingsEditorModel::setThemeMode(int themeMode) {
    if (!offers(SettingsOptions::themeModes(), themeMode)) {
        qWarning() << "SettingsEditorModel: unknown theme mode" << themeMode;
        return;
    }
    ThemeSettings edited = mValues.theme;
    edited.themeMode = themeMode;
    edited.accentColor = mValueStore.previewThemeMode(themeMode);
    setTheme(edited);
}

void SettingsEditorModel::setUseBlackBackground(bool useBlackBackground) {
    ThemeSettings edited = mValues.theme;
    edited.useBlackBackground = useBlackBackground;
    edited.accentColor = mValueStore.previewBlackBackground(useBlackBackground);
    setTheme(edited);
}

void SettingsEditorModel::setThumbnailOpacityPercent(int percent, bool preview) {
    ThemeSettings edited = mValues.theme;
    edited.thumbnailOpacityPercent = percent;
    setTheme(edited);
    if (preview)
        mValueStore.previewThumbnailOpacity(SettingsScales::opacity(percent));
}

void SettingsEditorModel::setCustomAccent(bool customAccent) {
    ThemeSettings edited = mValues.theme;
    edited.customAccent = customAccent;
    edited.accentColor = mValueStore.previewAccent(
        customAccent ? std::optional<QColor>(edited.accentColor) : std::nullopt);
    setTheme(edited);
}

void SettingsEditorModel::setAccentColor(const QColor &color) {
    if (!color.isValid())
        return;
    ThemeSettings edited = mValues.theme;
    edited.accentColor = mValueStore.previewAccent(color);
    setTheme(edited);
}

void SettingsEditorModel::clearThumbnailCache() {
    mValueStore.clearThumbnailCache();
    refreshThumbnailCacheSize();
}

void SettingsEditorModel::refreshThumbnailCacheSize() {
    mThumbnailCacheBytes = mValueStore.thumbnailCacheBytes();
    emit thumbnailCacheSizeChanged();
}

//--- shortcuts ----------------------------------------------------------------
void SettingsEditorModel::addShortcut() {
    mShortcutEditor.startAdd();
}

void SettingsEditorModel::editShortcut(int row) {
    const std::optional<ShortcutEntry> entry = mShortcuts.entryAt(row);
    if (!entry) {
        qWarning() << "SettingsEditorModel: no shortcut in row" << row;
        return;
    }
    mShortcutEditor.startEdit(*entry, row);
}

void SettingsEditorModel::removeShortcut(int row) {
    if (!mShortcuts.removeAt(row))
        qWarning() << "SettingsEditorModel: no shortcut in row" << row;
}

void SettingsEditorModel::resetShortcuts() {
    mShortcuts.setEntries(mShortcutStore.resetShortcuts());
}

void SettingsEditorModel::onShortcutAccepted() {
    const int row = mShortcuts.put(mShortcutEditor.result(), mShortcutEditor.editedRow());
    if (row >= 0)
        emit shortcutPut(row);
}

//--- scripts ------------------------------------------------------------------
void SettingsEditorModel::addScript() {
    mScriptEditor.startNew();
}

void SettingsEditorModel::editScript(int row) {
    const QStringList names = mScripts.stringList();
    if (row < 0 || row >= names.size()) {
        qWarning() << "SettingsEditorModel: no script in row" << row;
        return;
    }
    mScriptEditor.startEdit(names.at(row));
}

void SettingsEditorModel::removeScript(int row) {
    const QStringList names = mScripts.stringList();
    if (row < 0 || row >= names.size()) {
        qWarning() << "SettingsEditorModel: no script in row" << row;
        return;
    }
    mShortcuts.setEntries(mShortcutStore.removeScript(names.at(row), mShortcuts.entries()));
    refreshScripts();
}

void SettingsEditorModel::onScriptAccepted() {
    mShortcutStore.addScript(mScriptEditor.resultName(), mScriptEditor.resultScript());
    refreshScripts();
}

void SettingsEditorModel::refreshScripts() {
    QStringList names = mShortcutStore.scriptNames();
    names.sort();
    mScripts.setStringList(names);
}

//--- texts --------------------------------------------------------------------
QString SettingsEditorModel::autoResizeLimitText(int step) {
    return SettingsScales::autoResizeLimitText(step);
}

QString SettingsEditorModel::panelHideDelayText(int ms) {
    return SettingsScales::panelHideDelayText(ms);
}

QString SettingsEditorModel::expandLimitText(int limit) {
    return SettingsScales::expandLimitText(limit);
}

QString SettingsEditorModel::zoomStepText(int percent) {
    return SettingsScales::zoomStepText(percent);
}

QString SettingsEditorModel::casValueText(int percent) {
    return SettingsScales::casValueText(percent);
}

QString SettingsEditorModel::percentText(int percent) {
    return SettingsScales::percentText(percent);
}

QString SettingsEditorModel::mouseScrollingSpeedText(int step) {
    return SettingsScales::mouseScrollingSpeedText(step);
}

QString SettingsEditorModel::thumbnailResolutionText(int pixels) {
    return SettingsScales::thumbnailResolutionText(pixels);
}

QString SettingsEditorModel::pngCompressionText(int level) {
    return SettingsScales::pngCompressionText(level);
}
