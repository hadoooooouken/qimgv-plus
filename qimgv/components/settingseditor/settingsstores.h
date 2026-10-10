#pragma once

#include <QColor>
#include <QString>
#include <QStringList>

#include <optional>

#include "settingsvalues.h"

// Ports of the settings editor to the application's settings, shortcuts and
// scripts. The application implements them over Settings, ActionManager and
// ScriptManager; the tests use fakes. GUI thread only.

// The dialog's values and the live theme preview.
class ISettingsValueStore {
public:
    virtual ~ISettingsValueStore() = default;

    [[nodiscard]] virtual SettingsValues load() = 0;
    [[nodiscard]] virtual SettingsEnvironment environment() = 0;
    // Stores values and shortcuts (the shortcut table replaces every
    // shortcut), saves the theme, the scripts and the shortcuts and announces
    // the change.
    virtual void apply(const SettingsValues &values, const ShortcutList &shortcuts) = 0;

    // Theme preview: written to the settings at once and announced. The ones
    // returning a colour reload the theme and return its accent colour.
    [[nodiscard]] virtual QColor previewThemeMode(int themeMode) = 0;
    [[nodiscard]] virtual QColor previewBlackBackground(bool useBlackBackground) = 0;
    virtual void previewThumbnailOpacity(qreal opacity) = 0;
    // customAccent replaces the theme's accent and is saved with the theme;
    // std::nullopt returns to the theme's own accent.
    [[nodiscard]] virtual QColor previewAccent(std::optional<QColor> customAccent) = 0;

    // Disk usage of the thumbnail cache, in bytes.
    [[nodiscard]] virtual qint64 thumbnailCacheBytes() = 0;
    // Clears the thumbnail cache before returning.
    virtual void clearThumbnailCache() = 0;
};

// The application's actions, shortcuts and scripts.
class IShortcutScriptStore {
public:
    virtual ~IShortcutScriptStore() = default;

    [[nodiscard]] virtual ShortcutList shortcuts() = 0;
    // Restores the default shortcuts (at once, as the widget dialog did) and
    // returns them.
    [[nodiscard]] virtual ShortcutList resetShortcuts() = 0;
    // Action bound to shortcut in the application, empty for none.
    [[nodiscard]] virtual QString actionForShortcut(const QString &shortcut) = 0;
    [[nodiscard]] virtual QStringList actionNames() = 0;

    [[nodiscard]] virtual QStringList scriptNames() = 0;
    [[nodiscard]] virtual bool scriptExists(const QString &name) = 0;
    [[nodiscard]] virtual ScriptDefinition script(const QString &name) = 0;
    // Adds or replaces the script called name.
    virtual void addScript(const QString &name, const ScriptDefinition &script) = 0;
    // Takes over table as the application's shortcuts, removes the script
    // called name with its shortcuts and returns the shortcuts left.
    [[nodiscard]] virtual ShortcutList removeScript(const QString &name,
                                                    const ShortcutList &table) = 0;
};
