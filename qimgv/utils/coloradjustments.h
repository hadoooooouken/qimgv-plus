#pragma once

#include <cmath>
#include <numbers>

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

    friend bool operator==(const ColorAdjustments &,
                           const ColorAdjustments &) = default;
};

// Affine colour transform of straight (non-premultiplied) RGB in [0, 1]:
// out[i] = sum_j m[i][j] * in[j] + offset, clamped to [0, 1] by the consumer.
struct ColorMatrix {
    float m[3][3];
    float offset;
};

// The one definition of the ColorAdjustments -> ColorMatrix mapping, shared
// by the CPU path (ImageLib) and the QRhi renderer.
// Pure arithmetic, safe on any thread.
[[nodiscard]] inline ColorMatrix colorAdjustmentMatrix(const ColorAdjustments &adjustments) {
    // Rec.709 luma weights (saturation pivots around luma).
    constexpr float kLumaR = 0.2126f;
    constexpr float kLumaG = 0.7152f;
    constexpr float kLumaB = 0.0722f;
    // Unit grey axis component 1 / sqrt(3): hue rotates around (1, 1, 1).
    constexpr float kGreyAxis = 0.57735f;
    constexpr float kExposureBase = 2.0f;
    // Tint moves green against the mean of red and blue.
    constexpr float kTintRedBlueShare = 0.5f;
    // Contrast pivots around mid grey.
    constexpr float kContrastPivot = 0.5f;
    constexpr float kDegreesPerHalfTurn = 180.0f;

    const auto multiply = [](const float a[3][3], const float b[3][3], float c[3][3]) {
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                c[i][j] = a[i][0] * b[0][j] + a[i][1] * b[1][j] + a[i][2] * b[2][j];
            }
        }
    };
    const auto assign = [](float target[3][3], const float source[3][3]) {
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) target[i][j] = source[i][j];
        }
    };

    // 1 & 2. White balance (temperature & tint) and exposure.
    const float temperature = adjustments.temperature;
    const float tint = adjustments.tint;
    const float factor = std::pow(kExposureBase, adjustments.exposure);
    const float wR = (1.0f + temperature + tint * kTintRedBlueShare) * factor;
    const float wG = (1.0f - tint) * factor;
    const float wB = (1.0f - temperature + tint * kTintRedBlueShare) * factor;

    float current[3][3] = {
        {wR,   0.0f, 0.0f},
        {0.0f, wG,   0.0f},
        {0.0f, 0.0f, wB  }
    };

    // 3. Hue rotation around the grey axis.
    if (std::abs(adjustments.hue) > kColorAdjustmentEpsilon) {
        const float hueRad = adjustments.hue * std::numbers::pi_v<float> / kDegreesPerHalfTurn;
        const float cosAngle = std::cos(hueRad);
        const float sinAngle = std::sin(hueRad);
        const float k = kGreyAxis;
        const float cosInv = 1.0f - cosAngle;

        const float hueMatrix[3][3] = {
            { cosAngle + k * k * cosInv,      -k * sinAngle + k * k * cosInv,  k * sinAngle + k * k * cosInv },
            { k * sinAngle + k * k * cosInv,  cosAngle + k * k * cosInv,       -k * sinAngle + k * k * cosInv },
            { -k * sinAngle + k * k * cosInv, k * sinAngle + k * k * cosInv,   cosAngle + k * k * cosInv }
        };

        float product[3][3];
        multiply(hueMatrix, current, product);
        assign(current, product);
    }

    // 4. Saturation.
    const float saturation = adjustments.saturation;
    if (std::abs(saturation - ColorAdjustments::kNeutralSaturation) > kColorAdjustmentEpsilon) {
        const float rWeight = kLumaR * (1.0f - saturation);
        const float gWeight = kLumaG * (1.0f - saturation);
        const float bWeight = kLumaB * (1.0f - saturation);

        const float saturationMatrix[3][3] = {
            { saturation + rWeight, gWeight,              bWeight },
            { rWeight,              saturation + gWeight, bWeight },
            { rWeight,              gWeight,              saturation + bWeight }
        };

        float product[3][3];
        multiply(saturationMatrix, current, product);
        assign(current, product);
    }

    // 5 & 6. Contrast and brightness.
    const float contrast = adjustments.contrast;
    ColorMatrix result;
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            result.m[i][j] = current[i][j] * contrast;
        }
    }
    result.offset = adjustments.brightness * contrast + kContrastPivot * (1.0f - contrast);
    return result;
}
