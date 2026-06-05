#ifndef COLORSELECTORBUTTON_H
#define COLORSELECTORBUTTON_H

#include <QPainter>
#include <QColorDialog>
#include "gui/customwidgets/clickablelabel.h"

class ColorSelectorButton : public ClickableLabel {
    Q_OBJECT
public:
    explicit ColorSelectorButton(QWidget *parent = nullptr);

    void setColor(QColor &newColor);
    QColor color();
    void setDescription(QString text);
    void setShowAlpha(bool showAlpha);

signals:
    void colorChanged(QColor color);

protected:
    void paintEvent(QPaintEvent *e);

private slots:
    void showColorSelector();

private:
    QColor mColor;
    QString mDescription;
    bool mShowAlpha = false;
};

#endif // COLORSELECTORBUTTON_H
