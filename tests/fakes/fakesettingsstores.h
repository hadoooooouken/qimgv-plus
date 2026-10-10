#pragma once

#include <QMap>
#include <QStringList>

#include "components/settingseditor/settingsstores.h"

// In-memory stores of the settings editor for the tests: they keep what
// the editor writes and count the previews, the applies and the cache
// clears.
class FakeSettingsValueStore final : public ISettingsValueStore {
public:
    SettingsValues load() override { return stored; }
    SettingsEnvironment environment() override { return environmentValues; }

    void apply(const SettingsValues &values, const ShortcutList &shortcuts) override {
        stored = values;
        appliedShortcuts = shortcuts;
        ++applyCount;
    }

    QColor previewThemeMode(int themeMode) override {
        previewedThemeMode = themeMode;
        return themeAccent(themeMode);
    }

    QColor previewBlackBackground(bool useBlackBackground) override {
        previewedBlackBackground = useBlackBackground;
        return customAccent.value_or(themeAccent(previewedThemeMode));
    }

    void previewThumbnailOpacity(qreal opacity) override {
        previewedThumbnailOpacity = opacity;
        ++opacityPreviewCount;
    }

    QColor previewAccent(std::optional<QColor> accent) override {
        customAccent = accent;
        return accent.value_or(themeAccent(previewedThemeMode));
    }

    qint64 thumbnailCacheBytes() override { return cacheBytes; }
    void clearThumbnailCache() override {
        cacheBytes = 0;
        ++cacheClearCount;
    }

    // The accent of each theme mode, distinct per mode.
    static QColor themeAccent(int themeMode) {
        return QColor::fromRgb(themeMode, themeMode, themeMode);
    }

    SettingsValues stored;
    SettingsEnvironment environmentValues;
    ShortcutList appliedShortcuts;
    int applyCount = 0;
    int previewedThemeMode = 0;
    bool previewedBlackBackground = false;
    qreal previewedThumbnailOpacity = 0.0;
    int opacityPreviewCount = 0;
    std::optional<QColor> customAccent;
    qint64 cacheBytes = 0;
    int cacheClearCount = 0;
};

class FakeShortcutScriptStore final : public IShortcutScriptStore {
public:
    ShortcutList shortcuts() override { return live; }
    ShortcutList resetShortcuts() override {
        live = defaults;
        return live;
    }
    QString actionForShortcut(const QString &shortcut) override {
        for (const ShortcutEntry &entry : live) {
            if (entry.shortcut == shortcut)
                return entry.action;
        }
        return {};
    }
    QStringList actionNames() override { return actions; }

    QStringList scriptNames() override { return scripts.keys(); }
    bool scriptExists(const QString &name) override { return scripts.contains(name); }
    ScriptDefinition script(const QString &name) override { return scripts.value(name); }
    void addScript(const QString &name, const ScriptDefinition &script) override {
        scripts.insert(name, script);
    }
    ShortcutList removeScript(const QString &name, const ShortcutList &table) override {
        live.clear();
        const QString action = kScriptActionPrefix.toString() + name;
        for (const ShortcutEntry &entry : table) {
            if (entry.action != action)
                live.append(entry);
        }
        scripts.remove(name);
        return live;
    }

    ShortcutList live;
    ShortcutList defaults;
    QStringList actions;
    QMap<QString, ScriptDefinition> scripts;
};
