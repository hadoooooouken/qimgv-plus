#pragma once

#include <QDebug>
#include <QColor>
#include <QPalette>

enum ColorSchemes {
    COLORS_LIGHT,
    COLORS_DARK
};

struct BaseColorScheme {
    int tid;
    QColor background;
    QColor background_fullscreen;
    QColor text;
    QColor icons;
    QColor widget;
    QColor widget_border;
    QColor accent;
    QColor folderview;
    QColor folderview_topbar;
    QColor thumbpanel;
    QColor scrollbar;
    QColor overlay_text;
    QColor overlay;
    QColor status_pending;
    QColor status_error;
    QColor status_processing;
    QColor status_success;
};

class ColorScheme {
public:
    ColorScheme();
    ColorScheme(BaseColorScheme base);
    void setBaseColors(BaseColorScheme base);
    // index of theme name
    int tid;
    // base
    QColor background;
    QColor background_fullscreen;
    QColor text;
    QColor icons;
    QColor widget;
    QColor widget_border;
    QColor accent;
    QColor folderview;
    QColor folderview_topbar;
    QColor thumbpanel;
    QColor scrollbar;
    QColor scrollbar_hover;
    QColor overlay_text;
    QColor overlay;
    QColor status_pending;
    QColor status_error;
    QColor status_processing;
    QColor status_success;
    // extended
    QColor text_hc2;
    QColor text_hc;
    QColor text_lc;
    QColor text_lc2;
    QColor button;
    QColor button_hover;
    QColor button_pressed;
    QColor panel_button;
    QColor panel_button_hover;
    QColor panel_button_pressed;
    QColor folderview_hc;
    QColor folderview_hc2;
    QColor thumbpanel_hc;
    QColor thumbpanel_hc2;
    QColor thumbpanel_text;
    QColor folderview_button_hover;
    QColor folderview_button_pressed;
    QColor input_field_focus;
    QColor danger;
    QColor trash;


private:
    void createColorVariants();
};

class ThemeStore {
public:
    static ColorScheme colorScheme(ColorSchemes name);
};
