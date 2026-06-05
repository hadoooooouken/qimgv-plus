#include "themestore.h"

ColorScheme ThemeStore::colorScheme(ColorSchemes name) {
  BaseColorScheme base = {-1};
  QPalette p;
  switch (name) {
  case COLORS_SYSTEM:
  case COLORS_CUSTOMIZED:
    base.folderview_topbar = p.window().color();
    base.widget = p.window().color();
    base.widget_border = p.window().color();
    base.folderview = p.base().color();
    base.thumbpanel = p.base().color();
    base.text = p.text().color();
    base.icons = p.text().color();
    base.accent = p.highlight().color();
    base.scrollbar.setHsv(
        p.highlight().color().hue(),
        qBound(0, p.highlight().color().saturation() - 20, 240),
        qBound(0, p.highlight().color().value() - 35, 240));
    base.tid = static_cast<int>(name);
    break;
  case COLORS_LIGHT: // v2, works with w10 titlebars
    base.accent = p.highlight().color();
    base.background = QColor(0xff1a1a1a);
    base.background_fullscreen = QColor(0xff1a1a1a);
    base.folderview = QColor(0xfff2f2f2);
    base.folderview_topbar = QColor(0xffffffff);
    base.thumbpanel = QColor(0xfff2f2f2);
    base.icons = QColor(0xff656768);
    base.overlay = QColor(0xff1a1a1a);
    base.overlay_text = QColor(0xffd2d2d2);
    base.text = QColor(0xff353535);
    base.scrollbar = QColor(0xffaaaaaa);
    base.widget = QColor(0xffffffff);
    base.widget_border = QColor(0xffc3c3c3);
    base.tid = static_cast<int>(name);
    break;
  case COLORS_DARKBLUE:
    base.background = QColor(0xff18191a);
    base.background_fullscreen = QColor(0xff18191a);
    base.text = QColor(0xffcdd2d7);
    base.icons = QColor(0xffbabec3);
    base.widget = QColor(0xff232629);
    base.widget_border = QColor(0xff26292d);
    base.accent = p.highlight().color();
    base.folderview = QColor(0xff232629);
    base.folderview_topbar = QColor(0xff31363b);
    base.thumbpanel = QColor(0xff232629);
    base.scrollbar = QColor(0xff4f565c);
    base.overlay_text = QColor(0xffd2d2d2);
    base.overlay = QColor(0xff1a1a1a);
    base.tid = static_cast<int>(name);
    break;
  case COLORS_BLACK:
    base.background = QColor(0xff000000);
    base.background_fullscreen = QColor(0xff000000);
    base.text = QColor(0xffffffff);
    base.icons = QColor(0xffacacac);
    base.widget = QColor(0xff111111);
    base.widget_border = QColor(0xff222222);
    base.accent = p.highlight().color();
    base.folderview = QColor(0xff111111);
    base.folderview_topbar = QColor(0xff111111);
    base.thumbpanel = QColor(0x80000000);
    base.scrollbar = QColor(0xff343434);
    base.overlay_text = QColor(0xff999999);
    base.overlay = QColor(0xff000000);
    base.tid = static_cast<int>(name);
    break;
  case COLORS_DARK:
    base.background = QColor(0xff1a1a1a);
    base.background_fullscreen = QColor(0xff1a1a1a);
    base.text = QColor(0xffb6b6b6);
    base.icons = QColor(0xffa4a4a4);
    base.widget = QColor(0xff252525);
    base.widget_border = QColor(0xff2c2c2c);
    base.accent = p.highlight().color();
    base.folderview = QColor(0xff242424);
    base.folderview_topbar = QColor(0xff383838);
    base.thumbpanel = QColor(0xff242424);
    base.scrollbar = QColor(0xff5a5a5a);
    base.overlay_text = QColor(0xffd2d2d2);
    base.overlay = QColor(0xff1a1a1a);
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
  widget = base.widget;
  widget_border = base.widget_border;
  accent = base.accent;
  folderview = base.folderview;
  folderview_topbar = base.folderview_topbar;
  thumbpanel = base.thumbpanel;
  overlay = base.overlay;
  overlay_text = base.overlay_text;
  scrollbar = base.scrollbar;
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
