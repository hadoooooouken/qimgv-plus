#pragma once

#include <QObject>
#include <QtQml/qqmlregistration.h>

#include "settings_types.h"

// QML-visible mirrors of the UI-relevant enums in settings_types.h. The
// legacy enums are unscoped and declared at global scope, so they cannot be
// registered with the meta-object system directly. Every enumerator takes its
// value from the legacy enumerator it mirrors, so a static_cast converts
// between the two without a lookup table.
//
// QML access is scoped: SettingsEnums.FitMode.Window.
namespace SettingsEnums {
Q_NAMESPACE
QML_ELEMENT
Q_CLASSINFO("RegisterEnumClassesUnscoped", "false")

enum class FitMode {
  Window = FIT_WINDOW,
  Width = FIT_WIDTH,
  Original = FIT_ORIGINAL,
  Height = FIT_HEIGHT,
  Free = FIT_FREE,
};
Q_ENUM_NS(FitMode)

enum class ScalingFilter {
  Nearest = QI_FILTER_NEAREST,
  Bilinear = QI_FILTER_BILINEAR,
  Smart = QI_FILTER_SMART,
  Cas = QI_FILTER_CAS,
  SmartGpu = QI_FILTER_SMART_GPU,
  Mks2021 = QI_FILTER_MKS2021,
  Mks2021Gpu = QI_FILTER_MKS2021_GPU,
};
Q_ENUM_NS(ScalingFilter)

enum class FocusPoint {
  Top = FOCUS_TOP,
  Center = FOCUS_CENTER,
  Cursor = FOCUS_CURSOR,
};
Q_ENUM_NS(FocusPoint)

enum class ImageScrolling {
  None = SCROLL_NONE,
  ByTrackpad = SCROLL_BY_TRACKPAD,
  ByTrackpadAndWheel = SCROLL_BY_TRACKPAD_AND_WHEEL,
};
Q_ENUM_NS(ImageScrolling)

enum class PanelPosition {
  Top = PANEL_TOP,
  Bottom = PANEL_BOTTOM,
  Left = PANEL_LEFT,
  Right = PANEL_RIGHT,
};
Q_ENUM_NS(PanelPosition)

enum class PanelStyle {
  Simple = TH_PANEL_SIMPLE,
  Extended = TH_PANEL_EXTENDED,
};
Q_ENUM_NS(PanelStyle)

enum class SortingMode {
  Name = SORT_NAME,
  NameDescending = SORT_NAME_DESC,
  Size = SORT_SIZE,
  SizeDescending = SORT_SIZE_DESC,
  Time = SORT_TIME,
  TimeDescending = SORT_TIME_DESC,
};
Q_ENUM_NS(SortingMode)

enum class ZoomIndicatorMode {
  Disabled = INDICATOR_DISABLED,
  Enabled = INDICATOR_ENABLED,
  Auto = INDICATOR_AUTO,
};
Q_ENUM_NS(ZoomIndicatorMode)

enum class CropAction {
  Crop = ACTION_CROP,
  CropAndSave = ACTION_CROP_SAVE,
};
Q_ENUM_NS(CropAction)

} // namespace SettingsEnums
