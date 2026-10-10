#pragma once

#include <QFont>
#include <QFontMetrics>
#include <QtGlobal>

namespace UiMetrics {

inline constexpr int kCompactIconSizePx = 16;
inline constexpr int kStandardIconSizePx = 20;

// Point sizes of the secondary text styles, derived from the application
// font. Used by the Qt Quick Theme bridge.
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

// Control sizes derived from the height of a text line in the application
// font, plus the fixed frame sizes of tooltips and context menus. Used by
// the Qt Quick Theme bridge, like Typography.
struct ControlMetrics {
  int buttonHeight = 0;
  int topPanelHeight = 0;
  int overlayHeaderSize = 0;
  int contextMenuWidth = 0;
  int contextMenuItemHeight = 0;
  int renameOverlayWidth = 0;
  int tooltipBorderWidth = 0;
  int tooltipBorderRadius = 0;
  int contextMenuBorderRadius = 0;
};

// Vertical text padding as a fraction of the text line height.
inline constexpr float kTextPaddingScale = 0.10f;
inline constexpr float kLargeTextPaddingScale = 0.25f;
// Folder view top bar: vertical margin of its items, their minimum text
// padding and the minimum bar height.
inline constexpr int kTopPanelVerticalMarginPx = 4;
inline constexpr int kMinTopPanelTextPaddingPx = 4;
inline constexpr int kMinTopPanelHeightPx = 38;
inline constexpr int kMinOverlayHeaderSizePx = 30;
// Widths and heights designed for this text line height; larger fonts scale
// them up, smaller ones keep the designed size.
inline constexpr int kReferenceTextHeightPx = 22;
inline constexpr int kContextMenuWidthPx = 212;
inline constexpr int kContextMenuItemHeightPx = 32;
inline constexpr int kRenameOverlayWidthPx = 380;
inline constexpr int kTooltipBorderWidthPx = 1;
inline constexpr int kTooltipBorderRadiusPx = 6;
inline constexpr int kContextMenuBorderRadiusPx = 8;

[[nodiscard]] inline ControlMetrics controlMetricsFor(const QFont &baseFont) {
  const int textHeight = QFontMetrics(baseFont).height();
  const int textPadding = static_cast<int>(textHeight * kTextPaddingScale);
  const int largeTextPadding =
      static_cast<int>(textHeight * kLargeTextPaddingScale);
  const int topPanelTextPadding = qMax(textPadding, kMinTopPanelTextPaddingPx);
  const qreal referenceScale =
      qMax(static_cast<qreal>(textHeight) / kReferenceTextHeightPx, 1.0);
  return {
      .buttonHeight = textHeight + largeTextPadding * 2,
      .topPanelHeight = qMax(textHeight + topPanelTextPadding * 2 +
                                 kTopPanelVerticalMarginPx * 2,
                             kMinTopPanelHeightPx),
      .overlayHeaderSize =
          qMax(textHeight + textPadding * 2, kMinOverlayHeaderSizePx),
      .contextMenuWidth = static_cast<int>(kContextMenuWidthPx * referenceScale),
      .contextMenuItemHeight =
          static_cast<int>(kContextMenuItemHeightPx * referenceScale),
      .renameOverlayWidth =
          static_cast<int>(kRenameOverlayWidthPx * referenceScale),
      .tooltipBorderWidth = kTooltipBorderWidthPx,
      .tooltipBorderRadius = kTooltipBorderRadiusPx,
      .contextMenuBorderRadius = kContextMenuBorderRadiusPx,
  };
}

} // namespace UiMetrics
