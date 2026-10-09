#include "themebridge.h"

#include "gui/quick/bridges/publish.h"

#include <QDebug>

#include <optional>

//------------------------------------------------------------------------------
ThemeBridge::ThemeBridge(const ThemeSnapshot &initial, QObject *parent)
    : QObject(parent), mSnapshot(initial) {}

//------------------------------------------------------------------------------
const ThemeColors &ThemeBridge::colors() const { return mSnapshot.colors; }

const ThemeFonts &ThemeBridge::fonts() const { return mSnapshot.fonts; }

const ThemeMetrics &ThemeBridge::metrics() const { return mSnapshot.metrics; }

const ThemeSurfaces &ThemeBridge::surfaces() const {
  return mSnapshot.surfaces;
}

bool ThemeBridge::dark() const { return mSnapshot.dark; }

const QString &ThemeBridge::iconFontFamily() const {
  return mSnapshot.iconFontFamily;
}

//------------------------------------------------------------------------------
QString ThemeBridge::glyph(FluentIcons::FluentIcon icon) const {
  const std::optional<char32_t> codepoint = FluentIcons::codepoint(icon);
  if (!codepoint) {
    qWarning() << "ThemeBridge: no glyph registered for" << icon;
    return {};
  }
  return QString::fromUcs4(&*codepoint, 1);
}

//------------------------------------------------------------------------------
void ThemeBridge::apply(const ThemeSnapshot &snapshot) {
  publishIfChanged(mSnapshot.colors, snapshot.colors, *this,
                   &ThemeBridge::colorsChanged);
  publishIfChanged(mSnapshot.fonts, snapshot.fonts, *this,
                   &ThemeBridge::fontsChanged);
  publishIfChanged(mSnapshot.metrics, snapshot.metrics, *this,
                   &ThemeBridge::metricsChanged);
  publishIfChanged(mSnapshot.surfaces, snapshot.surfaces, *this,
                   &ThemeBridge::surfacesChanged);
  publishIfChanged(mSnapshot.dark, snapshot.dark, *this,
                   &ThemeBridge::darkChanged);
  publishIfChanged(mSnapshot.iconFontFamily, snapshot.iconFontFamily, *this,
                   &ThemeBridge::iconFontFamilyChanged);
}
