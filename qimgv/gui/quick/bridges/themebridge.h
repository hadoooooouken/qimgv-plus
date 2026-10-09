#pragma once

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

#include "gui/quick/bridges/themedata.h"
#include "gui/uimetrics.h"
#include "utils/fluenticon.h"

// Exposes the glyph catalogue to QML: Theme.glyph(FluentIcons.Settings20).
namespace FluentIconsForeign {
Q_NAMESPACE
QML_FOREIGN_NAMESPACE(FluentIcons)
QML_NAMED_ELEMENT(FluentIcons)
} // namespace FluentIconsForeign

// Colours, fonts, control metrics, dialog surfaces and icon glyphs for QML
// (Theme singleton).
//
// Like SettingsBridge, it is fed by the composition root: QuickUiHost builds
// a ThemeSnapshot whenever Settings::settingsChanged fires (a theme switch is
// only announced through that signal) and passes it to apply(), which emits
// only for the parts that changed.
//
// Icons are drawn as text: an item uses Theme.iconFontFamily as its font
// family and Theme.glyph(icon) as its text.
//
// GUI thread only.
class ThemeBridge : public QObject {
  Q_OBJECT
  QML_NAMED_ELEMENT(Theme)
  QML_SINGLETON
  QML_UNCREATABLE("Provided by QuickUiHost via setExternalSingletonInstance()")
  Q_PROPERTY(ThemeColors colors READ colors NOTIFY colorsChanged FINAL)
  Q_PROPERTY(ThemeFonts fonts READ fonts NOTIFY fontsChanged FINAL)
  Q_PROPERTY(ThemeMetrics metrics READ metrics NOTIFY metricsChanged FINAL)
  Q_PROPERTY(ThemeSurfaces surfaces READ surfaces NOTIFY surfacesChanged FINAL)
  Q_PROPERTY(bool dark READ dark NOTIFY darkChanged FINAL)
  Q_PROPERTY(QString iconFontFamily READ iconFontFamily NOTIFY iconFontFamilyChanged FINAL)
  Q_PROPERTY(int compactIconSize READ compactIconSize CONSTANT FINAL)
  Q_PROPERTY(int standardIconSize READ standardIconSize CONSTANT FINAL)

public:
  explicit ThemeBridge(const ThemeSnapshot &initial, QObject *parent = nullptr);

  [[nodiscard]] const ThemeColors &colors() const;
  [[nodiscard]] const ThemeFonts &fonts() const;
  [[nodiscard]] const ThemeMetrics &metrics() const;
  [[nodiscard]] const ThemeSurfaces &surfaces() const;
  [[nodiscard]] bool dark() const;
  [[nodiscard]] const QString &iconFontFamily() const;
  [[nodiscard]] static constexpr int compactIconSize() {
    return UiMetrics::kCompactIconSizePx;
  }
  [[nodiscard]] static constexpr int standardIconSize() {
    return UiMetrics::kStandardIconSizePx;
  }

  // Text of icon in the icon font. Returns an empty string and logs a
  // warning when no glyph is registered for icon.
  Q_INVOKABLE QString glyph(FluentIcons::FluentIcon icon) const;

  // Publishes snapshot; emits the notify signal of every changed part.
  void apply(const ThemeSnapshot &snapshot);

signals:
  void colorsChanged();
  void fontsChanged();
  void metricsChanged();
  void surfacesChanged();
  void darkChanged();
  void iconFontFamilyChanged();

private:
  ThemeSnapshot mSnapshot;
};
