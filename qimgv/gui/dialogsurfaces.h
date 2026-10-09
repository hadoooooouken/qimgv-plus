#pragma once

#include <QColor>

// Window and text colours of the application's dialogs (settings, print,
// resize) for the light and the dark scheme, and the tinted surfaces derived
// from the window colour: sidebars, groups, separators and slider grooves.
// Shared by the widget stylesheet and palette (Settings::loadStylesheet())
// and the Qt Quick Theme bridge.
namespace DialogSurfaces {

struct Colors {
  QColor window;
  QColor text;
  // From the faintest to the strongest contrast with the window colour.
  QColor tintedLc2;
  QColor tintedLc;
  QColor tinted;
  QColor tintedHc;
  QColor tintedHc2;
};

inline constexpr QRgb kDarkWindow = 0xff252525;
inline constexpr QRgb kDarkText = 0xffdcdcdc;
inline constexpr QRgb kLightWindow = 0xfff5f5f5;
inline constexpr QRgb kLightText = 0xff1e1e1e;

// A window colour at or below this HSV value counts as dark; tints of a dark
// window are lighter, tints of a light one darker.
inline constexpr float kDarkWindowMaxValue = 0.45f;
// HSV value offsets of the tints from the window colour.
inline constexpr int kTintLc2Offset = 6;
inline constexpr int kTintLcOffset = 14;
inline constexpr int kTintOffset = 20;
inline constexpr int kTintHcOffset = 35;
inline constexpr int kTintHc2Offset = 50;

[[nodiscard]] inline QColor tintOf(const QColor &window, int offset) {
  const int direction = window.valueF() <= kDarkWindowMaxValue ? 1 : -1;
  return QColor::fromHsv(window.hue(), window.saturation(),
                         window.value() + direction * offset)
      .toRgb();
}

[[nodiscard]] inline Colors colorsFor(bool dark) {
  const QColor window = QColor::fromRgb(dark ? kDarkWindow : kLightWindow);
  return {
      .window = window,
      .text = QColor::fromRgb(dark ? kDarkText : kLightText),
      .tintedLc2 = tintOf(window, kTintLc2Offset),
      .tintedLc = tintOf(window, kTintLcOffset),
      .tinted = tintOf(window, kTintOffset),
      .tintedHc = tintOf(window, kTintHcOffset),
      .tintedHc2 = tintOf(window, kTintHc2Offset),
  };
}

} // namespace DialogSurfaces
