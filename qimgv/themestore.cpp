#include "themestore.h"

ColorScheme ThemeStore::colorScheme(ColorSchemes name) {
  BaseColorScheme base = {-1};
  QPalette p;
  switch (name) {
  case COLORS_LIGHT: // v2, works with w10 titlebars
    base.accent = p.highlight().color();
    base.background = QColor(0xff1a1a1a);
    base.background_fullscreen = QColor(0xff1a1a1a);
    base.folderview = QColor(0xfff2f2f2);
    base.folderview_topbar = QColor(0xffffffff);
    base.thumbpanel = QColor(0xfff2f2f2);
    base.text = QColor(0xff303030);
    base.icons = QColor(0xff1a1a1a);
    base.folder_icons = QColor(0xff656768);
    base.overlay = QColor(0xff1a1a1a);
    base.overlay_text = QColor(0xffd2d2d2);
    base.scrollbar = QColor(0xffaaaaaa);
    base.widget = QColor(0xffffffff);
    base.widget_border = QColor(0xffc3c3c3);
    base.status_pending = QColor(0xffcc7a00); // Amber/orange suitable for light theme
    base.status_error = QColor(0xffd32f2f);   // Red suitable for light theme
    base.status_processing = QColor(0xff0066cc); // Blue suitable for light theme
    base.status_success = QColor(0xff2e7d32);    // Green suitable for light theme
    base.danger = QColor(0xffd32f2f);
    base.trash  = QColor(0xfff57c00);
    base.tid = static_cast<int>(name);
    break;
  case COLORS_DARK:
    base.background = QColor(0xff1f1f1f);
    base.background_fullscreen = QColor(0xff1a1a1a);
    base.text = QColor(0xffd2d2d2);
    base.icons = QColor(0xffe8e8e8);
    base.folder_icons = QColor(0xffa4a4a4);
    base.widget = QColor(0xff181818);
    base.widget_border = QColor(0xff2b2b2b);
    base.accent = p.highlight().color();
    base.folderview = QColor(0xff1f1f1f);
    base.folderview_topbar = QColor(0xff181818);
    base.thumbpanel = QColor(0xff1f1f1f);
    base.scrollbar = QColor(0xff4d4d4d);
    base.overlay_text = QColor(0xffd2d2d2);
    base.overlay = QColor(0xff1a1a1a);
    base.status_pending = QColor(0xffffaa00);
    base.status_error = QColor(0xffff3333);
    base.status_processing = QColor(0xff33aaff);
    base.status_success = QColor(0xff33cc33);
    base.danger = QColor(0xfffb5555);
    base.trash = QColor(0xffffb900);
    base.tid = static_cast<int>(name);
    break;
  }
  return ColorScheme(base);
}

//---------------------------------------------------------------------

ColorScheme::ColorScheme() { tid = -1; }

ColorScheme::ColorScheme(BaseColorScheme base) { setBaseColors(base); }

void ColorScheme::setBaseColors(BaseColorScheme base) {
  background = base.background;
  background_fullscreen = base.background_fullscreen;
  text = base.text;
  icons = base.icons;
  folder_icons = base.folder_icons;
  widget = base.widget;
  widget_border = base.widget_border;
  accent = base.accent;
  folderview = base.folderview;
  folderview_topbar = base.folderview_topbar;
  thumbpanel = base.thumbpanel;
  overlay = base.overlay;
  overlay_text = base.overlay_text;
  scrollbar = base.scrollbar;
  status_pending = base.status_pending;
  status_error = base.status_error;
  status_processing = base.status_processing;
  status_success = base.status_success;
  danger = base.danger;
  trash = base.trash;
  tid = base.tid;
  createColorVariants();
}

void ColorScheme::createColorVariants() {
  if (widget.valueF() <= 0.45f) { // dark theme
    // top bar buttons
    panel_button.setHsv(folderview_topbar.hue(), folderview_topbar.saturation(),
                        qMin(folderview_topbar.value() + 20, 255));
    panel_button_hover.setHsv(folderview_topbar.hue(),
                              folderview_topbar.saturation(),
                              qMin(folderview_topbar.value() + 26, 255));
    panel_button_pressed.setHsv(folderview_topbar.hue(),
                                folderview_topbar.saturation(),
                                qMin(folderview_topbar.value() + 15, 255));
    folderview_hc.setHsv(folderview.hue(), folderview.saturation(),
                         qMin(folderview.value() + 12, 255));
    folderview_hc2.setHsv(folderview.hue(), folderview.saturation(),
                          qMin(folderview.value() + 28, 255));
    thumbpanel_hc = (qGray(thumbpanel.rgb()) > 128) ? QColor(0, 0, 0)
                                                    : QColor(255, 255, 255);
    thumbpanel_hc2.setHsv(thumbpanel.hue(), thumbpanel.saturation(),
                          qMin(thumbpanel.value() + 18, 255));
    thumbpanel_hc.setAlpha(thumbpanel.alpha());
    thumbpanel_hc2.setAlpha(thumbpanel.alpha());
    folderview_button_pressed = folderview_hc;
    folderview_button_hover = folderview_hc2;
    // regular buttons - from widget bg
    button.setHsv(widget.hue(), widget.saturation(),
                  qMin(widget.value() + 21, 255));
    button_hover = QColor(button.lighter(112));
    button_pressed = QColor(button.darker(112));
    scrollbar_hover = scrollbar.lighter(120);
    // text
    text_hc = QColor(text.lighter(110));
    text_hc2 = (qGray(folderview.rgb()) > 128) ? QColor(0, 0, 0)
                                               : QColor(255, 255, 255);
    thumbpanel_text = (qGray(thumbpanel.rgb()) > 128) ? QColor(0, 0, 0)
                                                      : QColor(255, 255, 255);
    text_lc = QColor(text.darker(115));
    text_lc2 = QColor(text.darker(160));
  } else { // light theme
    // top bar buttons
    panel_button.setHsv(folderview_topbar.hue(), folderview_topbar.saturation(),
                        qMax(folderview_topbar.value() - 30, 0));
    panel_button_hover.setHsv(folderview_topbar.hue(),
                              folderview_topbar.saturation(),
                              qMax(folderview_topbar.value() - 45, 0));
    panel_button_pressed.setHsv(folderview_topbar.hue(),
                                folderview_topbar.saturation(),
                                qMax(folderview_topbar.value() - 55, 0));
    folderview_hc.setHsv(folderview.hue(), folderview.saturation(),
                         qMax(folderview.value() - 25, 0));
    folderview_hc2.setHsv(folderview.hue(), folderview.saturation(),
                          qMax(folderview.value() - 60, 0));
    thumbpanel_hc = (qGray(thumbpanel.rgb()) > 128) ? QColor(0, 0, 0)
                                                    : QColor(255, 255, 255);
    thumbpanel_hc2.setHsv(thumbpanel.hue(), thumbpanel.saturation(),
                          qMax(thumbpanel.value() - 40, 0));
    thumbpanel_hc.setAlpha(thumbpanel.alpha());
    thumbpanel_hc2.setAlpha(thumbpanel.alpha());
    folderview_button_pressed = folderview_hc2;
    folderview_button_hover = folderview_hc;
    // regular buttons - from widget bg
    button.setHsv(widget.hue(), widget.saturation(),
                  qMax(widget.value() - 42, 0));
    button_hover = QColor(button.darker(106));
    button_pressed = QColor(button.darker(118));
    scrollbar_hover = scrollbar.darker(120);
    // text
    text_hc = QColor(text.darker(104));
    text_hc2 = (qGray(folderview.rgb()) > 128) ? QColor(0, 0, 0)
                                               : QColor(255, 255, 255);
    thumbpanel_text = (qGray(thumbpanel.rgb()) > 128) ? QColor(0, 0, 0)
                                                      : QColor(255, 255, 255);
    text_lc = QColor(text.lighter(130));
    text_lc2 = QColor(text.lighter(160));
  }
  // misc
  input_field_focus = QColor(accent);
}
