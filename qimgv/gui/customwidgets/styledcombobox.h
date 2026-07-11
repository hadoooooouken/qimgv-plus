#ifndef STYLEDCOMBOBOX_H
#define STYLEDCOMBOBOX_H

#include <QComboBox>
#include <QPainter>
#include <QKeyEvent>
#include <QColor>
#include "utils/imagelib.h"

class StyledComboBox : public QComboBox
{
public:
    StyledComboBox(QWidget *parent = nullptr);
    void setIconPath(QString path);

    // Logical width reserved for the trailing dropdown icon, including the
    // gap between the icon and the widget's right edge. Single source of
    // truth for subclasses that need to lay out content next to the icon
    // (e.g. a manually-drawn label) without overlapping it.
    int iconAreaWidth() const;

protected:
    void paintEvent(QPaintEvent *e) override;
    void keyPressEvent(QKeyEvent *event) override;
    bool event(QEvent *e) override;

    // Color applied to the dropdown icon. Defaults to the theme's icon
    // color; subclasses override to force a specific color in specific
    // visual states (e.g. a highlighted/active state).
    virtual QColor iconColor() const;
    // Re-applies iconColor() to the currently loaded icon pixmap.
    void refreshIconColor();

    // Gap between the icon and the widget's right edge, in logical pixels.
    static constexpr int kIconRightMargin = 8;

private:
    bool hiResPixmap;
    QPixmap downArrow;
    qreal dpr, pixmapDrawScale;
    // Original (non-@2x) icon path, kept so the icon can be reloaded at
    // the correct resolution if the widget's devicePixelRatio changes at
    // runtime (e.g. the window is moved to a monitor with a different
    // system scale factor, or the system scale factor itself changes).
    QString iconResourcePath;
};

#endif // STYLEDCOMBOBOX_H
