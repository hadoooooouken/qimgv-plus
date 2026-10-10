#pragma once

#include <QColor>
#include <QObject>
#include <QStringList>
#include <QStringListModel>
#include <QVariantList>

#include "scripteditormodel.h"
#include "settingsoptions.h"
#include "settingsscales.h"
#include "settingsvalues.h"
#include "shortcuteditormodel.h"
#include "shortcuttablemodel.h"

class ISettingsValueStore;
class IShortcutScriptStore;

// The settings dialog's state and rules, shared by the widget and the Qt
// Quick dialog: loads the values (load()), keeps the edited draft per page,
// applies it (apply()), and owns the shortcut table, the script list and
// the two sub-dialogs (shortcut creator, script editor).
//
// As in the widget dialog, some edits take effect at once, before apply():
// the theme mode, the black background, the thumbnail bar opacity and the
// custom accent are previewed through the store; scripts are added, edited
// and removed at once, and "Reset to defaults" restores the default
// shortcuts at once. Shortcut edits stay in the table until apply().
//
// Values that the stored settings do not offer fall back to a valid choice
// on load: the language to English, the scaling filter to bilinear, the
// monitor profile, the HDR operator and white level to their first choice,
// and the Upscayl model to the default (or first) installed one. Without
// Upscayl models every Upscayl option is off.
//
// GUI thread only.
class SettingsEditorModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(GeneralSettings general READ general WRITE setGeneral NOTIFY generalChanged FINAL)
    Q_PROPERTY(ViewSettings view READ view WRITE setView NOTIFY viewChanged FINAL)
    Q_PROPERTY(ThemeSettings theme READ theme WRITE setTheme NOTIFY themeChanged FINAL)
    Q_PROPERTY(ControlsSettings controls READ controls WRITE setControls NOTIFY controlsChanged FINAL)
    Q_PROPERTY(AdvancedSettings advanced READ advanced WRITE setAdvanced NOTIFY advancedChanged FINAL)
    Q_PROPERTY(UpscaleSettings upscale READ upscale WRITE setUpscale NOTIFY upscaleChanged FINAL)
    Q_PROPERTY(SettingsRanges ranges READ ranges CONSTANT FINAL)

    Q_PROPERTY(QVariantList languages READ languages CONSTANT FINAL)
    Q_PROPERTY(QVariantList zoomIndicatorModes READ zoomIndicatorModes CONSTANT FINAL)
    Q_PROPERTY(QVariantList thumbPanelStyles READ thumbPanelStyles CONSTANT FINAL)
    Q_PROPERTY(QVariantList panelPositions READ panelPositions CONSTANT FINAL)
    Q_PROPERTY(QVariantList folderEndActions READ folderEndActions CONSTANT FINAL)
    Q_PROPERTY(QVariantList sortingModes READ sortingModes CONSTANT FINAL)
    Q_PROPERTY(QVariantList fitModes READ fitModes CONSTANT FINAL)
    Q_PROPERTY(QVariantList focusPoints READ focusPoints CONSTANT FINAL)
    Q_PROPERTY(QVariantList scalingFilters READ scalingFilters CONSTANT FINAL)
    Q_PROPERTY(QVariantList monitorProfiles READ monitorProfiles CONSTANT FINAL)
    Q_PROPERTY(QVariantList hdrOperators READ hdrOperators CONSTANT FINAL)
    Q_PROPERTY(QVariantList hdrTargetWhiteLevels READ hdrTargetWhiteLevels CONSTANT FINAL)
    Q_PROPERTY(QVariantList themeModes READ themeModes CONSTANT FINAL)
    Q_PROPERTY(QVariantList imageScrollingModes READ imageScrollingModes CONSTANT FINAL)

    Q_PROPERTY(QStringList upscaylModels READ upscaylModels NOTIFY environmentChanged FINAL)
    Q_PROPERTY(bool upscaylAvailable READ isUpscaylAvailable NOTIFY environmentChanged FINAL)
    // The Upscayl options follow "Use Upscayl"; the limit slider also
    // follows its check box.
    Q_PROPERTY(bool upscaylOptionsEnabled READ upscaylOptionsEnabled NOTIFY upscaleStateChanged FINAL)
    Q_PROPERTY(bool upscaylLimitSliderEnabled READ upscaylLimitSliderEnabled NOTIFY upscaleStateChanged FINAL)
    // The profile file row shows for a custom monitor profile with colour
    // management on; the CAS options show for the CAS filter.
    Q_PROPERTY(bool customProfileVisible READ customProfileVisible NOTIFY viewChanged FINAL)
    Q_PROPERTY(bool casOptionsVisible READ casOptionsVisible NOTIFY viewChanged FINAL)
    Q_PROPERTY(QString thumbnailCacheSizeText READ thumbnailCacheSizeText NOTIFY thumbnailCacheSizeChanged FINAL)

    Q_PROPERTY(QAbstractItemModel *shortcuts READ shortcutTable CONSTANT FINAL)
    // Script names, sorted; role "display".
    Q_PROPERTY(QAbstractItemModel *scripts READ scriptList CONSTANT FINAL)
    Q_PROPERTY(ShortcutEditorModel *shortcutEditor READ shortcutEditor CONSTANT FINAL)
    Q_PROPERTY(ScriptEditorModel *scriptEditor READ scriptEditor CONSTANT FINAL)

    Q_PROPERTY(QString windowTitle READ windowTitle CONSTANT FINAL)
    Q_PROPERTY(QString aboutText READ aboutText CONSTANT FINAL)
    Q_PROPERTY(QString applicationVersion READ applicationVersion CONSTANT FINAL)
    Q_PROPERTY(QString qtVersion READ qtVersion CONSTANT FINAL)
    Q_PROPERTY(QString colorProfileDialogTitle READ colorProfileDialogTitle CONSTANT FINAL)
    Q_PROPERTY(QStringList colorProfileFilters READ colorProfileFilters CONSTANT FINAL)

public:
    // The pages of the dialog, in sidebar order.
    enum class Page { General, View, Theme, Controls, Scripts, Advanced, Upscale, About, Count };
    Q_ENUM(Page)

    // The stores must outlive the model.
    SettingsEditorModel(ISettingsValueStore &values, IShortcutScriptStore &shortcuts,
                        QObject *parent = nullptr);

    [[nodiscard]] GeneralSettings general() const;
    void setGeneral(const GeneralSettings &general);
    [[nodiscard]] ViewSettings view() const;
    void setView(const ViewSettings &view);
    // Stores the draft only; the previewed values change through their own
    // setters below.
    [[nodiscard]] ThemeSettings theme() const;
    void setTheme(const ThemeSettings &theme);
    [[nodiscard]] ControlsSettings controls() const;
    void setControls(const ControlsSettings &controls);
    [[nodiscard]] AdvancedSettings advanced() const;
    void setAdvanced(const AdvancedSettings &advanced);
    [[nodiscard]] UpscaleSettings upscale() const;
    void setUpscale(const UpscaleSettings &upscale);
    [[nodiscard]] static SettingsRanges ranges();

    [[nodiscard]] SettingsValues values() const;
    void setValues(const SettingsValues &values);

    [[nodiscard]] static QVariantList languages();
    [[nodiscard]] static QVariantList zoomIndicatorModes();
    [[nodiscard]] static QVariantList thumbPanelStyles();
    [[nodiscard]] static QVariantList panelPositions();
    [[nodiscard]] static QVariantList folderEndActions();
    [[nodiscard]] static QVariantList sortingModes();
    [[nodiscard]] static QVariantList fitModes();
    [[nodiscard]] static QVariantList focusPoints();
    [[nodiscard]] static QVariantList scalingFilters();
    [[nodiscard]] static QVariantList monitorProfiles();
    [[nodiscard]] static QVariantList hdrOperators();
    [[nodiscard]] static QVariantList hdrTargetWhiteLevels();
    [[nodiscard]] static QVariantList themeModes();
    [[nodiscard]] static QVariantList imageScrollingModes();

    [[nodiscard]] QStringList upscaylModels() const;
    [[nodiscard]] bool isUpscaylAvailable() const;
    [[nodiscard]] bool upscaylOptionsEnabled() const;
    [[nodiscard]] bool upscaylLimitSliderEnabled() const;
    [[nodiscard]] bool customProfileVisible() const;
    [[nodiscard]] bool casOptionsVisible() const;
    [[nodiscard]] QString thumbnailCacheSizeText() const;

    [[nodiscard]] ShortcutTableModel *shortcutTable();
    [[nodiscard]] QStringListModel *scriptList();
    [[nodiscard]] ShortcutEditorModel *shortcutEditor();
    [[nodiscard]] ScriptEditorModel *scriptEditor();

    [[nodiscard]] static QString windowTitle();
    [[nodiscard]] static QString aboutText();
    [[nodiscard]] static QString applicationVersion();
    [[nodiscard]] static QString qtVersion();
    [[nodiscard]] static QString colorProfileDialogTitle();
    [[nodiscard]] static QStringList colorProfileFilters();

    // Reads every value, the shortcuts, the scripts and the cache size.
    Q_INVOKABLE void load();
    // Stores the draft and the shortcut table.
    Q_INVOKABLE void apply();

    // "Load defaults" of the fixed zoom levels.
    Q_INVOKABLE void resetZoomLevels();
    // A picked monitor profile file; non-local URLs are ignored.
    Q_INVOKABLE void setMonitorProfileFile(const QUrl &url);

    // Previewed theme values.
    Q_INVOKABLE void setThemeMode(int themeMode);
    Q_INVOKABLE void setUseBlackBackground(bool useBlackBackground);
    // preview: false while the slider is dragged (the value is previewed
    // when it is released).
    Q_INVOKABLE void setThumbnailOpacityPercent(int percent, bool preview);
    Q_INVOKABLE void setCustomAccent(bool customAccent);
    Q_INVOKABLE void setAccentColor(const QColor &color);

    Q_INVOKABLE void clearThumbnailCache();

    // Shortcut table: the creator / editor opens through shortcutEditor().
    Q_INVOKABLE void addShortcut();
    Q_INVOKABLE void editShortcut(int row);
    Q_INVOKABLE void removeShortcut(int row);
    Q_INVOKABLE void resetShortcuts();

    // Scripts by row of scripts(); the editor opens through scriptEditor().
    Q_INVOKABLE void addScript();
    Q_INVOKABLE void editScript(int row);
    Q_INVOKABLE void removeScript(int row);

    // Texts next to the sliders (SettingsScales).
    Q_INVOKABLE static QString autoResizeLimitText(int step);
    Q_INVOKABLE static QString panelHideDelayText(int ms);
    Q_INVOKABLE static QString expandLimitText(int limit);
    Q_INVOKABLE static QString zoomStepText(int percent);
    Q_INVOKABLE static QString casValueText(int percent);
    Q_INVOKABLE static QString percentText(int percent);
    Q_INVOKABLE static QString mouseScrollingSpeedText(int step);
    Q_INVOKABLE static QString thumbnailResolutionText(int pixels);
    Q_INVOKABLE static QString pngCompressionText(int level);

signals:
    void generalChanged();
    void viewChanged();
    void themeChanged();
    void controlsChanged();
    void advancedChanged();
    void upscaleChanged();
    void upscaleStateChanged();
    void environmentChanged();
    void thumbnailCacheSizeChanged();
    // A shortcut was put into the table at row (to select it).
    void shortcutPut(int row);

private:
    void normalize(SettingsValues &values) const;
    void refreshScripts();
    void refreshThumbnailCacheSize();
    void onShortcutAccepted();
    void onScriptAccepted();

    ISettingsValueStore &mValueStore;
    IShortcutScriptStore &mShortcutStore;
    SettingsValues mValues;
    SettingsEnvironment mEnvironment;
    qint64 mThumbnailCacheBytes = 0;
    ShortcutTableModel mShortcuts;
    QStringListModel mScripts;
    ShortcutEditorModel mShortcutEditor;
    ScriptEditorModel mScriptEditor;
};
