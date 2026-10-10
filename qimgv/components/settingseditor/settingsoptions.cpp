#include "settingsoptions.h"

#include <QCoreApplication>
#include <QVariantMap>

#include "settings_types.h"

namespace {
using namespace Qt::StringLiterals;

constexpr char kContext[] = "SettingsDialog";

QString translated(const char *text) {
    return QCoreApplication::translate(kContext, text);
}

SettingsOption option(int value, const char *text, const char *description = nullptr) {
    return {.value = value,
            .text = translated(text),
            .description = description ? translated(description) : QString()};
}

// White levels of the HDR tone mapping, in nits.
constexpr int kBt2408WhiteNits = 203;
constexpr int kSrgbWhiteNits = 100;
constexpr int kDimWhiteNits = 80;
constexpr int kBrightWhiteNits = 300;

// Tone-mapping operators of HdrToneMapper, in its order.
constexpr int kBt2408Operator = 0;
constexpr int kReinhardJodieOperator = 1;
constexpr int kAcesOperator = 2;
constexpr int kHableOperator = 3;
} // namespace

namespace SettingsOptions {

SettingsOptionList languages() {
    // Native names, in the order of the language codes; not translated. The
    // system language entry comes first, as in the widget dialog.
    return {
        {.value = kSystemLanguage.toString(), .text = u"System language"_s},
        {.value = u"de_DE"_s, .text = u"Deutsch"_s},
        {.value = u"en_US"_s, .text = u"English"_s},
        {.value = u"es_ES"_s, .text = u"Español"_s},
        {.value = u"fr_FR"_s, .text = u"Français"_s},
        {.value = u"ja_JP"_s, .text = u"日本語"_s},
        {.value = u"ru_RU"_s, .text = u"Русский"_s},
        {.value = u"tr_TR"_s, .text = u"Türkçe"_s},
        {.value = u"uk_UA"_s, .text = u"Українська"_s},
        {.value = u"zh_CN"_s, .text = u"简体中文"_s},
    };
}

SettingsOptionList zoomIndicatorModes() {
    return {
        option(INDICATOR_ENABLED, QT_TRANSLATE_NOOP("SettingsDialog", "On")),
        option(INDICATOR_DISABLED, QT_TRANSLATE_NOOP("SettingsDialog", "Off")),
        option(INDICATOR_AUTO, QT_TRANSLATE_NOOP("SettingsDialog", "Auto")),
    };
}

SettingsOptionList thumbPanelStyles() {
    return {
        option(TH_PANEL_SIMPLE, QT_TRANSLATE_NOOP("SettingsDialog", "Simple"),
               QT_TRANSLATE_NOOP("SettingsDialog", "Previews only")),
        option(TH_PANEL_EXTENDED, QT_TRANSLATE_NOOP("SettingsDialog", "Extended"),
               QT_TRANSLATE_NOOP("SettingsDialog", "Show filename and resolution")),
    };
}

SettingsOptionList panelPositions() {
    return {
        option(PANEL_TOP, QT_TRANSLATE_NOOP("SettingsDialog", "Top")),
        option(PANEL_BOTTOM, QT_TRANSLATE_NOOP("SettingsDialog", "Bottom")),
        option(PANEL_LEFT, QT_TRANSLATE_NOOP("SettingsDialog", "Left")),
        option(PANEL_RIGHT, QT_TRANSLATE_NOOP("SettingsDialog", "Right")),
    };
}

SettingsOptionList folderEndActions() {
    return {
        option(FOLDER_END_NO_ACTION, QT_TRANSLATE_NOOP("SettingsDialog", "Stop")),
        option(FOLDER_END_LOOP, QT_TRANSLATE_NOOP("SettingsDialog", "Loop folder")),
        option(FOLDER_END_GOTO_ADJACENT, QT_TRANSLATE_NOOP("SettingsDialog", "Go to the next folder")),
    };
}

SettingsOptionList sortingModes() {
    return {
        option(SORT_NAME, QT_TRANSLATE_NOOP("SettingsDialog", "A - Z")),
        option(SORT_NAME_DESC, QT_TRANSLATE_NOOP("SettingsDialog", "Z - A")),
        option(SORT_SIZE, QT_TRANSLATE_NOOP("SettingsDialog", "Size")),
        option(SORT_SIZE_DESC, QT_TRANSLATE_NOOP("SettingsDialog", "Size (desc)")),
        option(SORT_TIME, QT_TRANSLATE_NOOP("SettingsDialog", "Oldest")),
        option(SORT_TIME_DESC, QT_TRANSLATE_NOOP("SettingsDialog", "Newest")),
    };
}

SettingsOptionList fitModes() {
    return {
        option(FIT_WINDOW, QT_TRANSLATE_NOOP("SettingsDialog", "Fit to window")),
        option(FIT_WIDTH, QT_TRANSLATE_NOOP("SettingsDialog", "Fit to width")),
        option(FIT_ORIGINAL, QT_TRANSLATE_NOOP("SettingsDialog", "1:1")),
        option(FIT_HEIGHT, QT_TRANSLATE_NOOP("SettingsDialog", "Fit to height")),
    };
}

SettingsOptionList focusPoints() {
    return {
        option(FOCUS_TOP, QT_TRANSLATE_NOOP("SettingsDialog", "Top")),
        option(FOCUS_CENTER, QT_TRANSLATE_NOOP("SettingsDialog", "Center")),
        option(FOCUS_CURSOR, QT_TRANSLATE_NOOP("SettingsDialog", "At cursor")),
    };
}

SettingsOptionList scalingFilters() {
    return {
        option(QI_FILTER_NEAREST, QT_TRANSLATE_NOOP("SettingsDialog", "Nearest")),
        option(QI_FILTER_BILINEAR, QT_TRANSLATE_NOOP("SettingsDialog", "Bilinear")),
        option(QI_FILTER_SMART, QT_TRANSLATE_NOOP("SettingsDialog", "Smart sharpen")),
        option(QI_FILTER_MKS2021, QT_TRANSLATE_NOOP("SettingsDialog", "Magic Kernel Sharp 2021")),
        option(QI_FILTER_CAS, QT_TRANSLATE_NOOP("SettingsDialog", "FidelityFX-CAS (GPU)")),
        option(QI_FILTER_SMART_GPU, QT_TRANSLATE_NOOP("SettingsDialog", "Smart sharpen (GPU)")),
        option(QI_FILTER_MKS2021_GPU,
               QT_TRANSLATE_NOOP("SettingsDialog", "Magic Kernel Sharp 2021 (GPU)")),
    };
}

SettingsOptionList monitorProfiles() {
    const auto profile = [](const QString &key, const char *text) {
        return SettingsOption{.value = key, .text = translated(text)};
    };
    return {
        profile(u"System"_s, QT_TRANSLATE_NOOP("SettingsDialog", "System / Auto (Recommended)")),
        profile(u"sRGB"_s, QT_TRANSLATE_NOOP("SettingsDialog", "sRGB")),
        profile(u"DisplayP3"_s, QT_TRANSLATE_NOOP("SettingsDialog", "Display P3")),
        profile(u"AdobeRGB"_s, QT_TRANSLATE_NOOP("SettingsDialog", "Adobe RGB")),
        profile(u"Rec2020"_s, QT_TRANSLATE_NOOP("SettingsDialog", "Rec. 2020")),
        profile(u"ProPhoto"_s, QT_TRANSLATE_NOOP("SettingsDialog", "ProPhoto RGB")),
        profile(u"LinearSRGB"_s, QT_TRANSLATE_NOOP("SettingsDialog", "Linear sRGB")),
        profile(kCustomMonitorProfile.toString(),
                QT_TRANSLATE_NOOP("SettingsDialog", "Custom Profile (.icc/.icm)...")),
    };
}

SettingsOptionList hdrOperators() {
    return {
        option(kBt2408Operator, QT_TRANSLATE_NOOP("SettingsDialog", "ITU-R BT.2408 (Recommended)")),
        option(kReinhardJodieOperator, QT_TRANSLATE_NOOP("SettingsDialog", "Reinhard-Jodie")),
        option(kAcesOperator, QT_TRANSLATE_NOOP("SettingsDialog", "ACES Filmic")),
        option(kHableOperator, QT_TRANSLATE_NOOP("SettingsDialog", "Hable (Uncharted 2)")),
    };
}

SettingsOptionList hdrTargetWhiteLevels() {
    return {
        option(kBt2408WhiteNits,
               QT_TRANSLATE_NOOP("SettingsDialog", "203 nits (ITU-R BT.2408 Default)")),
        option(kSrgbWhiteNits, QT_TRANSLATE_NOOP("SettingsDialog", "100 nits (Standard sRGB)")),
        option(kDimWhiteNits, QT_TRANSLATE_NOOP("SettingsDialog", "80 nits (Dim Environment)")),
        option(kBrightWhiteNits, QT_TRANSLATE_NOOP("SettingsDialog", "300 nits (Bright Room)")),
    };
}

SettingsOptionList themeModes() {
    return {
        option(THEME_AUTO, QT_TRANSLATE_NOOP("SettingsDialog", "System Default (Auto)")),
        option(THEME_DARK, QT_TRANSLATE_NOOP("SettingsDialog", "Dark")),
        option(THEME_LIGHT, QT_TRANSLATE_NOOP("SettingsDialog", "Light")),
    };
}

SettingsOptionList imageScrollingModes() {
    return {
        option(SCROLL_NONE, QT_TRANSLATE_NOOP("SettingsDialog", "None")),
        option(SCROLL_BY_TRACKPAD, QT_TRANSLATE_NOOP("SettingsDialog", "Touchpad")),
        option(SCROLL_BY_TRACKPAD_AND_WHEEL,
               QT_TRANSLATE_NOOP("SettingsDialog", "Touchpad & Mouse Wheel")),
    };
}

qsizetype indexOf(const SettingsOptionList &options, const QVariant &value) {
    for (qsizetype i = 0; i < options.size(); ++i) {
        if (options.at(i).value == value)
            return i;
    }
    return -1;
}

QVariantList toVariantList(const SettingsOptionList &options) {
    QVariantList list;
    list.reserve(options.size());
    for (const SettingsOption &entry : options) {
        list.append(QVariantMap{{u"value"_s, entry.value},
                                {u"text"_s, entry.text},
                                {u"description"_s, entry.description}});
    }
    return list;
}

} // namespace SettingsOptions
