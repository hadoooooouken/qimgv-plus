#include <QFont>
#include <QFontMetrics>
#include <QTest>

#include "gui/dialogsurfaces.h"
#include "gui/quick/adapters/themesnapshotbuilder.h"
#include "gui/uimetrics.h"
#include "testsuites.h"
#include "themestore.h"

namespace {
using namespace Qt::StringLiterals;

// Fonts whose text line is shorter and much taller than the reference
// height the context menu sizes were designed for. Point sizes, like the
// application font (the text styles derive point sizes from it).
constexpr int kSmallFontPointSize = 6;
constexpr int kLargeFontPointSize = 32;

QFont fontWithPointSize(int pointSize) {
  QFont font;
  font.setPointSize(pointSize);
  return font;
}

QColor grey(int value) { return QColor::fromRgb(value, value, value); }
} // namespace

// The control metrics and dialog surfaces of the Qt Quick Theme bridge, and
// the theme snapshot built from them.
class UiMetricsTests : public QObject {
  Q_OBJECT

private slots:
  void darkDialogSurfacesAreLighterTints() {
    const DialogSurfaces::Colors colors = DialogSurfaces::colorsFor(true);
    // The values the stylesheet used for the dark scheme.
    QCOMPARE(colors.window, grey(37));
    QCOMPARE(colors.text, grey(220));
    QCOMPARE(colors.tintedLc2, grey(43));
    QCOMPARE(colors.tintedLc, grey(51));
    QCOMPARE(colors.tinted, grey(57));
    QCOMPARE(colors.tintedHc, grey(72));
    QCOMPARE(colors.tintedHc2, grey(87));
  }

  void lightDialogSurfacesAreDarkerTints() {
    const DialogSurfaces::Colors colors = DialogSurfaces::colorsFor(false);
    QCOMPARE(colors.window, grey(245));
    QCOMPARE(colors.text, grey(30));
    QCOMPARE(colors.tintedLc2, grey(239));
    QCOMPARE(colors.tintedLc, grey(231));
    QCOMPARE(colors.tinted, grey(225));
    QCOMPARE(colors.tintedHc, grey(210));
    QCOMPARE(colors.tintedHc2, grey(195));
  }

  void smallFontsKeepTheDesignedSizes() {
    const UiMetrics::ControlMetrics metrics =
        UiMetrics::controlMetricsFor(fontWithPointSize(kSmallFontPointSize));
    QCOMPARE(metrics.contextMenuWidth, UiMetrics::kContextMenuWidthPx);
    QCOMPARE(metrics.contextMenuItemHeight, UiMetrics::kContextMenuItemHeightPx);
    QCOMPARE(metrics.renameOverlayWidth, UiMetrics::kRenameOverlayWidthPx);
    QCOMPARE(metrics.topPanelHeight, UiMetrics::kMinTopPanelHeightPx);
    QCOMPARE(metrics.overlayHeaderSize, UiMetrics::kMinOverlayHeaderSizePx);
  }

  void largeFontsScaleTheSizes() {
    const QFont font = fontWithPointSize(kLargeFontPointSize);
    const int textHeight = QFontMetrics(font).height();
    QVERIFY(textHeight > UiMetrics::kReferenceTextHeightPx);
    const UiMetrics::ControlMetrics metrics = UiMetrics::controlMetricsFor(font);

    QCOMPARE(metrics.buttonHeight,
             textHeight + 2 * static_cast<int>(textHeight * 0.25f));
    const qreal scale =
        static_cast<qreal>(textHeight) / UiMetrics::kReferenceTextHeightPx;
    QCOMPARE(metrics.contextMenuWidth, static_cast<int>(212 * scale));
    QCOMPARE(metrics.contextMenuItemHeight, static_cast<int>(32 * scale));
    QCOMPARE(metrics.renameOverlayWidth, static_cast<int>(380 * scale));
    QVERIFY(metrics.topPanelHeight > UiMetrics::kMinTopPanelHeightPx);
    QVERIFY(metrics.overlayHeaderSize > UiMetrics::kMinOverlayHeaderSizePx);
  }

  void frameSizesAreFixed() {
    for (const int pointSize : {kSmallFontPointSize, kLargeFontPointSize}) {
      const UiMetrics::ControlMetrics metrics =
          UiMetrics::controlMetricsFor(fontWithPointSize(pointSize));
      QCOMPARE(metrics.tooltipBorderWidth, 1);
      QCOMPARE(metrics.tooltipBorderRadius, 6);
      QCOMPARE(metrics.contextMenuBorderRadius, 8);
    }
  }

  void snapshotCarriesTheSchemeMetricsAndSurfaces_data() {
    QTest::addColumn<int>("scheme");
    QTest::addColumn<bool>("dark");
    QTest::newRow("dark") << static_cast<int>(COLORS_DARK) << true;
    QTest::newRow("light") << static_cast<int>(COLORS_LIGHT) << false;
  }

  void snapshotCarriesTheSchemeMetricsAndSurfaces() {
    QFETCH(int, scheme);
    QFETCH(bool, dark);
    const ColorScheme colors =
        ThemeStore::colorScheme(static_cast<ColorSchemes>(scheme));
    const QFont font = fontWithPointSize(kLargeFontPointSize);
    const QString iconFamily = u"Icons"_s;

    const ThemeSnapshot snapshot = buildThemeSnapshot(colors, font, iconFamily);

    QCOMPARE(snapshot.dark, dark);
    QCOMPARE(snapshot.iconFontFamily, iconFamily);
    QCOMPARE(snapshot.colors.background, colors.background);
    QCOMPARE(snapshot.colors.button, colors.button);
    QCOMPARE(snapshot.colors.panelButtonHover, colors.panel_button_hover);
    QCOMPARE(snapshot.colors.comboFieldBorder, colors.combo_field_border);
    QCOMPARE(snapshot.colors.scrollbarHover, colors.scrollbar_hover);

    const UiMetrics::ControlMetrics metrics = UiMetrics::controlMetricsFor(font);
    QCOMPARE(snapshot.metrics.buttonHeight, metrics.buttonHeight);
    QCOMPARE(snapshot.metrics.contextMenuWidth, metrics.contextMenuWidth);
    QCOMPARE(snapshot.metrics.contextMenuItemHeight, metrics.contextMenuItemHeight);
    QCOMPARE(snapshot.metrics.tooltipBorderRadius, metrics.tooltipBorderRadius);
    QCOMPARE(snapshot.metrics.contextMenuBorderRadius,
             metrics.contextMenuBorderRadius);

    const DialogSurfaces::Colors surfaces = DialogSurfaces::colorsFor(dark);
    QCOMPARE(snapshot.surfaces.window, surfaces.window);
    QCOMPARE(snapshot.surfaces.tintedHc2, surfaces.tintedHc2);
    QCOMPARE(snapshot.fonts.base, font);
  }
};

int runUiMetricsTests(int argc, char **argv) {
  UiMetricsTests tests;
  return QTest::qExec(&tests, argc, argv);
}

#include "tst_uimetrics.moc"
