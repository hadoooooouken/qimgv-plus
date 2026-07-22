#ifndef STYLEDCOMBOBOX_H
#define STYLEDCOMBOBOX_H

#include <QComboBox>
#include <QPainter>
#include <QKeyEvent>
#include <QColor>
#include "utils/iconfontmanager.h"

class StyledComboBox : public QComboBox
{
public:
    StyledComboBox(QWidget *parent = nullptr);
    void setIcon(FluentIcon icon, int sizePx);

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
    QColor applyEnabledState(const QColor &color) const;
    // Re-renders the glyph with the current iconColor(). Also used to pick
    // up a devicePixelRatio change at runtime.
    void refreshIcon();

    // Gap between the icon and the widget's right edge, in logical pixels.
    static constexpr int kIconRightMargin = 8;

private:
    QPixmap downArrow;
    qreal dpr;
    FluentIcon iconGlyph = FluentIcon::ChevronDown12;
    int iconSizePx = 0;
    bool iconSet = false;
};

#endif // STYLEDCOMBOBOX_H
