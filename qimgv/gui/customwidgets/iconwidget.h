#pragma once

#include <QWidget>
#include <QStyleOption>
#include <QPainter>
#include <QDebug>
#include <QMouseEvent>
#include "utils/iconfontmanager.h"
#include "utils/imagelib.h"

enum IconColorMode {
    ICON_COLOR_CUSTOM,
    ICON_COLOR_THEME,
    ICON_COLOR_THEME_FOLDER,
    ICON_COLOR_SOURCE
};

class IconWidget : public QWidget {
public:
    explicit IconWidget(QWidget *parent = nullptr);
    ~IconWidget();
    void setIconPath(QString path);
    // Fluent glyph mode. sizePx is the logical (pre-DPI) side length the
    // glyph is rendered at - callers pick it the same way they used to pick
    // a PNG file's design size (16/18/20/24/32/48). ICON_COLOR_SOURCE has no
    // meaning for a single-color glyph and is treated the same as whichever
    // color is currently set.
    void setIcon(FluentIcon icon, int sizePx);
    void setIconOffset(int x, int y);
    void setColorMode(IconColorMode _mode);
    void setColor(QColor _color);
    QSize minimumSizeHint() const;

    // Size menu-item and overlay-header icons are constrained to (see
    // paintEvent()). MenuItem::setIcon()'s default sizePx mirrors this.
    static constexpr int kMenuItemIconSizePx = 16;

protected:
    void paintEvent(QPaintEvent *event);

private slots:
    void onSettingsChanged();

private:
    void loadIcon();
    void applyColor();
    void renderGlyph();
    QColor themeColor() const;

    QString iconPath;
    bool glyphMode = false;
    FluentIcon glyphIcon = FluentIcon::Folder;
    int glyphSizePx = 0;
    // Display-size clamp used in paintEvent() for MenuItemIcon /
    // OverlayHeaderIcon widgets. Defaults to kMenuItemIconSizePx so
    // pixmap-path icons (setIconPath(), e.g. overlay header icons) keep
    // their existing fixed 16px look. Glyph icons (setIcon()) override it
    // with their own requested sizePx, so callers like MenuItem can resize
    // list-item icons past 16px just by asking for a bigger sizePx.
    int displayClampPx = kMenuItemIconSizePx;
    QColor color;
    IconColorMode colorMode = ICON_COLOR_THEME;
    bool hiResPixmap;
    QPoint iconOffset;
    QPixmap *pixmap;
    qreal dpr, pixmapDrawScale;
};
