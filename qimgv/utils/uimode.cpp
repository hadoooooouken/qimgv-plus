#include "uimode.h"

#include <QString>

namespace {
using namespace Qt::StringLiterals;

constexpr QLatin1StringView quickName = "quick"_L1;
constexpr QLatin1StringView widgetsName = "widgets"_L1;
} // namespace

QLatin1StringView uiModeName(UiMode mode) {
  switch (mode) {
  case UiMode::Widgets:
    return widgetsName;
  case UiMode::Quick:
    break;
  }
  return quickName;
}

std::optional<UiMode> uiModeFromName(QStringView name) {
  if (name.compare(quickName, Qt::CaseInsensitive) == 0)
    return UiMode::Quick;
  if (name.compare(widgetsName, Qt::CaseInsensitive) == 0)
    return UiMode::Widgets;
  return std::nullopt;
}
