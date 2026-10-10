#pragma once

#include <QObject>

#include "components/settingseditor/settingsstores.h"

class ActionManager;
class ScriptManager;
class Settings;

// ISettingsValueStore over the application's Settings, for the settings
// dialog of both UIs: reads and stores the values exactly as the widget
// dialog did, so either UI writes the same configuration. Applying also
// stores the shortcut table into the ActionManager and saves the shortcuts
// and the scripts.
//
// The thumbnail cache is owned by Core: clearThumbnailCache() only emits
// clearThumbnailCacheRequested(), which the UI forwards to Core over a
// direct connection, so the cache is cleared when it returns.
//
// GUI thread only.
class AppSettingsStore final : public QObject, public ISettingsValueStore {
    Q_OBJECT

public:
    // settings, actions and scripts must outlive the store.
    AppSettingsStore(Settings &settings, ActionManager &actions, ScriptManager &scripts,
                     QObject *parent = nullptr);

    [[nodiscard]] SettingsValues load() override;
    [[nodiscard]] SettingsEnvironment environment() override;
    void apply(const SettingsValues &values, const ShortcutList &shortcuts) override;

    [[nodiscard]] QColor previewThemeMode(int themeMode) override;
    [[nodiscard]] QColor previewBlackBackground(bool useBlackBackground) override;
    void previewThumbnailOpacity(qreal opacity) override;
    [[nodiscard]] QColor previewAccent(std::optional<QColor> customAccent) override;

    [[nodiscard]] qint64 thumbnailCacheBytes() override;
    void clearThumbnailCache() override;

signals:
    void clearThumbnailCacheRequested();

private:
    void storeGeneral(const GeneralSettings &general);
    void storeView(const ViewSettings &view);
    void storeTheme(const ThemeSettings &theme);
    void storeControls(const ControlsSettings &controls);
    void storeAdvanced(const AdvancedSettings &advanced);
    void storeUpscale(const UpscaleSettings &upscale);
    // The current scheme with customAccent, set and saved with the theme.
    void saveCustomAccent(const QColor &customAccent);

    Settings &mSettings;
    ActionManager &mActions;
    ScriptManager &mScripts;
};

// IShortcutScriptStore over the application's ActionManager and
// ScriptManager. GUI thread only.
class AppShortcutStore final : public IShortcutScriptStore {
public:
    // actions and scripts must outlive the store.
    AppShortcutStore(ActionManager &actions, ScriptManager &scripts);

    [[nodiscard]] ShortcutList shortcuts() override;
    [[nodiscard]] ShortcutList resetShortcuts() override;
    [[nodiscard]] QString actionForShortcut(const QString &shortcut) override;
    [[nodiscard]] QStringList actionNames() override;

    [[nodiscard]] QStringList scriptNames() override;
    [[nodiscard]] bool scriptExists(const QString &name) override;
    [[nodiscard]] ScriptDefinition script(const QString &name) override;
    void addScript(const QString &name, const ScriptDefinition &script) override;
    [[nodiscard]] ShortcutList removeScript(const QString &name,
                                            const ShortcutList &table) override;

private:
    ActionManager &mActions;
    ScriptManager &mScripts;
};

// Replaces the application's shortcuts with table (not saved).
void storeShortcuts(ActionManager &actions, const ShortcutList &table);
