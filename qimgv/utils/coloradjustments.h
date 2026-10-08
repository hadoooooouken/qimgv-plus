#pragma once

#include <cmath>

// Threshold below which a colour adjustment component counts as "unchanged".
inline constexpr float kColorAdjustmentEpsilon = 0.001f;

// Interactive colour adjustments applied by the viewer (GPU preview), by
// ImageLib::applyColorAdjustments() (CPU apply/export) and by the batch
// converter. Default-constructed values are the identity transform.
struct ColorAdjustments {
    static constexpr float kNeutralExposure = 0.0f;
    static constexpr float kNeutralContrast = 1.0f;
    static constexpr float kNeutralBrightness = 0.0f;
    static constexpr float kNeutralTemperature = 0.0f;
    static constexpr float kNeutralTint = 0.0f;
    static constexpr float kNeutralSaturation = 1.0f;
    static constexpr float kNeutralHue = 0.0f;

    float exposure = kNeutralExposure;       // stops
    float contrast = kNeutralContrast;       // multiplier
    float brightness = kNeutralBrightness;   // offset
    float temperature = kNeutralTemperature; // white balance blue <-> amber
    float tint = kNeutralTint;               // white balance green <-> magenta
    float saturation = kNeutralSaturation;   // multiplier
    float hue = kNeutralHue;                 // degrees

    // True when at least one component differs from the identity transform.
    [[nodiscard]] bool hasAdjustments() const noexcept {
        return std::abs(exposure - kNeutralExposure) > kColorAdjustmentEpsilon ||
               std::abs(contrast - kNeutralContrast) > kColorAdjustmentEpsilon ||
               std::abs(brightness - kNeutralBrightness) > kColorAdjustmentEpsilon ||
               std::abs(temperature - kNeutralTemperature) > kColorAdjustmentEpsilon ||
               std::abs(tint - kNeutralTint) > kColorAdjustmentEpsilon ||
               std::abs(saturation - kNeutralSaturation) > kColorAdjustmentEpsilon ||
               std::abs(hue - kNeutralHue) > kColorAdjustmentEpsilon;
    }
};
