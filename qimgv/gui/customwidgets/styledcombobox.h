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

protected:
    void paintEvent(QPaintEvent *e) override;
    void keyPressEvent(QKeyEvent *event) override;

    // Color applied to the dropdown icon. Defaults to the theme's icon
    // color; subclasses override to force a specific color in specific
    // visual states (e.g. a highlighted/active state).
    virtual QColor iconColor() const;
    // Re-applies iconColor() to the currently loaded icon pixmap.
    void refreshIconColor();

private:
    bool hiResPixmap;
    QPixmap downArrow;
    qreal dpr, pixmapDrawScale;
};

#endif // STYLEDCOMBOBOX_H
