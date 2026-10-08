#pragma once

#include <QFont>
#include <QtGlobal>

namespace UiMetrics {

inline constexpr int kCompactIconSizePx = 16;
inline constexpr int kStandardIconSizePx = 20;

// Point sizes of the secondary text styles, derived from the application
// font. Shared by the widget stylesheet (Settings::loadStylesheet()) and the
// Qt Quick Theme bridge so both UIs scale text identically.
struct Typography {
  int smallPointSize = 0;
  int sectionPointSize = 0;
  int largePointSize = 0;
};

inline constexpr float kSmallFontScale = 0.9f;
inline constexpr int kMinSmallFontPointSize = 8;
inline constexpr int kSectionFontPointSizeIncrement = 1;
inline constexpr float kLargeFontScale = 1.8f;

[[nodiscard]] inline Typography typographyFor(const QFont &baseFont) {
  const int basePointSize = baseFont.pointSize();
  return {
      .smallPointSize =
          qMax(static_cast<int>(basePointSize * kSmallFontScale),
               kMinSmallFontPointSize),
      .sectionPointSize = basePointSize + kSectionFontPointSizeIncrement,
      .largePointSize = static_cast<int>(basePointSize * kLargeFontScale),
  };
}

} // namespace UiMetrics
