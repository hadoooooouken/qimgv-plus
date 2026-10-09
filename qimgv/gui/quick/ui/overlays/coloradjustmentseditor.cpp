#include "coloradjustmentseditor.h"

#include <QCoreApplication>

namespace {
// Slider units per adjustment unit (exposure in stops, multipliers and
// offsets); hue is in whole degrees.
constexpr float kSliderUnitsPerUnit = 100.0f;

// Ranges and defaults of ColorAdjustmentsOverlay. The labels use its
// translation context, so the existing translations apply.
QList<SliderSpec> colorAdjustmentSpecs() {
    const auto label = [](const char *text) {
        return QCoreApplication::translate("ColorAdjustmentsOverlay", text);
    };
    // Defaults: the identity transform (ColorAdjustments{}) in slider units.
    constexpr int kNeutral = 0;
    constexpr int kNeutralMultiplier = 100;
    return {
        {label("Exposure"), -300, 300, kNeutral, SliderValueFormat::SignedHundredths},
        {label("Contrast"), 0, 300, kNeutralMultiplier, SliderValueFormat::Percent},
        {label("Brightness"), -100, 100, kNeutral, SliderValueFormat::Percent},
        {label("Temperature"), -50, 50, kNeutral, SliderValueFormat::Integer},
        {label("Tint"), -50, 50, kNeutral, SliderValueFormat::Integer},
        {label("Saturation"), 0, 200, kNeutralMultiplier, SliderValueFormat::Percent},
        {label("Hue"), -180, 180, kNeutral, SliderValueFormat::Degrees},
    };
}
} // namespace

ColorAdjustmentsEditor::ColorAdjustmentsEditor(QObject *parent)
    : AdjustmentSliderModel(colorAdjustmentSpecs(), parent) {}

ColorAdjustments ColorAdjustmentsEditor::adjustments() const {
    const auto scaled = [this](Row row) { return value(row) / kSliderUnitsPerUnit; };
    return {
        .exposure = scaled(Exposure),
        .contrast = scaled(Contrast),
        .brightness = scaled(Brightness),
        .temperature = scaled(Temperature),
        .tint = scaled(Tint),
        .saturation = scaled(Saturation),
        .hue = static_cast<float>(value(Hue)),
    };
}

void ColorAdjustmentsEditor::setComparing(bool comparing) {
    if (mComparing == comparing)
        return;
    mComparing = comparing;
    emit previewChanged(mComparing ? ColorAdjustments{} : adjustments());
}

void ColorAdjustmentsEditor::apply() {
    emit applyRequested(adjustments());
    resetAll();
}

void ColorAdjustmentsEditor::valuesEdited() {
    if (!mComparing)
        emit previewChanged(adjustments());
}
