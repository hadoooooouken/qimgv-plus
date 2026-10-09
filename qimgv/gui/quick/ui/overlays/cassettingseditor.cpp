#include "cassettingseditor.h"

#include <QCoreApplication>

namespace {
// Slider units per parameter unit.
constexpr float kSliderUnitsPerUnit = 100.0f;

int toSlider(float value) {
    return qRound(value * kSliderUnitsPerUnit);
}

// Ranges of CasSettingsOverlay; the labels use its translation context.
QList<SliderSpec> casSpecs() {
    const auto label = [](const char *text) {
        return QCoreApplication::translate("CasSettingsOverlay", text);
    };
    constexpr int kMinimumSharpening = 1;
    constexpr int kMaximum = 100;
    constexpr int kMinimumContrast = 0;
    return {
        {label("Sharpening"), kMinimumSharpening, kMaximum,
         toSlider(CasParameters::kDefaultSharpening), SliderValueFormat::Hundredths},
        {label("Contrast"), kMinimumContrast, kMaximum,
         toSlider(CasParameters::kDefaultContrast), SliderValueFormat::Hundredths},
    };
}
} // namespace

CasSettingsEditor::CasSettingsEditor(QObject *parent)
    : AdjustmentSliderModel(casSpecs(), parent) {}

CasParameters CasSettingsEditor::parameters() const {
    return {.sharpening = value(Sharpening) / kSliderUnitsPerUnit,
            .contrast = value(Contrast) / kSliderUnitsPerUnit};
}

void CasSettingsEditor::load(const CasParameters &parameters) {
    loadValues({toSlider(parameters.sharpening), toSlider(parameters.contrast)});
}

void CasSettingsEditor::valuesEdited() {
    emit parametersEdited(parameters());
}
