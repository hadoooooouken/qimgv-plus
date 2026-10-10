#pragma once

#include <QLatin1StringView>
#include <QStringView>

#include <optional>

// User interface the application runs: the Qt Quick UI, or the legacy widget
// UI kept as a fallback until it is removed (docs/QML_MIGRATION_PLAN.md,
// S4.2). Chosen by the --ui command-line option, else by the hidden
// "userInterface" setting.
enum class UiMode { Quick, Widgets };

inline constexpr UiMode defaultUiMode = UiMode::Quick;

// The name used by --ui and the setting ("quick", "widgets").
[[nodiscard]] QLatin1StringView uiModeName(UiMode mode);

// Parses a name case-insensitively; std::nullopt for an unknown name.
[[nodiscard]] std::optional<UiMode> uiModeFromName(QStringView name);
