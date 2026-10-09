#include "themesnapshotbuilder.h"

#include "gui/dialogsurfaces.h"
#include "gui/uimetrics.h"
#include "themestore.h"

namespace {

ThemeColors themeColors(const ColorScheme &scheme) {
  return {
      .background = scheme.background,
      .backgroundFullscreen = scheme.background_fullscreen,
      .text = scheme.text,
      .icons = scheme.icons,
      .folderIcons = scheme.folder_icons,
      .thumbnailFolderIcons = scheme.thumbnail_folder_icons,
      .widget = scheme.widget,
      .widgetBorder = scheme.widget_border,
      .accent = scheme.accent,
      .folderView = scheme.folderview,
      .folderViewTopBar = scheme.folderview_topbar,
      .thumbPanel = scheme.thumbpanel,
      .scrollbar = scheme.scrollbar,
      .scrollbarHover = scheme.scrollbar_hover,
      .overlayText = scheme.overlay_text,
      .overlay = scheme.overlay,
      .statusPending = scheme.status_pending,
      .statusError = scheme.status_error,
      .statusProcessing = scheme.status_processing,
      .statusSuccess = scheme.status_success,
      .danger = scheme.danger,
      .trash = scheme.trash,
      .textHc2 = scheme.text_hc2,
      .textHc = scheme.text_hc,
      .textLc = scheme.text_lc,
      .textLc2 = scheme.text_lc2,
      .button = scheme.button,
      .buttonHover = scheme.button_hover,
      .buttonPressed = scheme.button_pressed,
      .panelButton = scheme.panel_button,
      .panelButtonHover = scheme.panel_button_hover,
      .panelButtonPressed = scheme.panel_button_pressed,
      .folderViewHc = scheme.folderview_hc,
      .folderViewHc2 = scheme.folderview_hc2,
      .thumbPanelHc = scheme.thumbpanel_hc,
      .thumbPanelHc2 = scheme.thumbpanel_hc2,
      .thumbPanelText = scheme.thumbpanel_text,
      .folderViewButtonHover = scheme.folderview_button_hover,
      .folderViewButtonPressed = scheme.folderview_button_pressed,
      .inputFieldFocus = scheme.input_field_focus,
      .comboField = scheme.combo_field,
      .comboFieldHover = scheme.combo_field_hover,
      .comboFieldPressed = scheme.combo_field_pressed,
      .comboFieldBorder = scheme.combo_field_border,
  };
}

QFont withPointSize(const QFont &base, int pointSize) {
  QFont font = base;
  font.setPointSize(pointSize);
  return font;
}

ThemeFonts themeFonts(const QFont &base) {
  const UiMetrics::Typography typography = UiMetrics::typographyFor(base);
  return {
      .base = base,
      .compact = withPointSize(base, typography.smallPointSize),
      .section = withPointSize(base, typography.sectionPointSize),
      .large = withPointSize(base, typography.largePointSize),
  };
}

} // namespace

//------------------------------------------------------------------------------
ThemeSnapshot buildThemeSnapshot(const ColorScheme &scheme,
                                 const QFont &baseFont,
                                 const QString &iconFontFamily) {
  const UiMetrics::ControlMetrics metrics =
      UiMetrics::controlMetricsFor(baseFont);
  const bool dark = scheme.tid == COLORS_DARK;
  const DialogSurfaces::Colors surfaces = DialogSurfaces::colorsFor(dark);
  return {
      .colors = themeColors(scheme),
      .fonts = themeFonts(baseFont),
      .metrics =
          {
              .buttonHeight = metrics.buttonHeight,
              .topPanelHeight = metrics.topPanelHeight,
              .overlayHeaderSize = metrics.overlayHeaderSize,
              .contextMenuWidth = metrics.contextMenuWidth,
              .contextMenuItemHeight = metrics.contextMenuItemHeight,
              .renameOverlayWidth = metrics.renameOverlayWidth,
              .tooltipBorderWidth = metrics.tooltipBorderWidth,
              .tooltipBorderRadius = metrics.tooltipBorderRadius,
              .contextMenuBorderRadius = metrics.contextMenuBorderRadius,
          },
      .surfaces =
          {
              .window = surfaces.window,
              .text = surfaces.text,
              .tintedLc2 = surfaces.tintedLc2,
              .tintedLc = surfaces.tintedLc,
              .tinted = surfaces.tinted,
              .tintedHc = surfaces.tintedHc,
              .tintedHc2 = surfaces.tintedHc2,
          },
      .dark = dark,
      .iconFontFamily = iconFontFamily,
  };
}
