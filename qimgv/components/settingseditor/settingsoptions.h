#pragma once

#include <QList>
#include <QString>
#include <QVariant>
#include <QVariantList>

// The choices of the settings dialog's combo boxes and radio button groups,
// in display order, with their translated texts (translation context
// "SettingsDialog", as in the widget dialog). The values are those stored:
// the int values of the settings enums, language codes, monitor profile keys
// or white levels in nits.
struct SettingsOption {
    QVariant value;
    QString text;
    // Shown next to the choice; empty for most.
    QString description;
};

using SettingsOptionList = QList<SettingsOption>;

namespace SettingsOptions {

// Language code of the system language entry.
inline constexpr QStringView kSystemLanguage = u"system";
// Language used when the stored one is not offered.
inline constexpr QStringView kFallbackLanguage = u"en_US";
// Monitor profile key that selects a profile file.
inline constexpr QStringView kCustomMonitorProfile = u"Custom";

[[nodiscard]] SettingsOptionList languages();
[[nodiscard]] SettingsOptionList zoomIndicatorModes();
[[nodiscard]] SettingsOptionList thumbPanelStyles();
[[nodiscard]] SettingsOptionList panelPositions();
[[nodiscard]] SettingsOptionList folderEndActions();
[[nodiscard]] SettingsOptionList sortingModes();
[[nodiscard]] SettingsOptionList fitModes();
[[nodiscard]] SettingsOptionList focusPoints();
[[nodiscard]] SettingsOptionList scalingFilters();
[[nodiscard]] SettingsOptionList monitorProfiles();
[[nodiscard]] SettingsOptionList hdrOperators();
[[nodiscard]] SettingsOptionList hdrTargetWhiteLevels();
[[nodiscard]] SettingsOptionList themeModes();
[[nodiscard]] SettingsOptionList imageScrollingModes();

// Position of value in options, -1 when it is not offered.
[[nodiscard]] qsizetype indexOf(const SettingsOptionList &options, const QVariant &value);
// options for QML: a list of {value, text, description} maps.
[[nodiscard]] QVariantList toVariantList(const SettingsOptionList &options);

} // namespace SettingsOptions
