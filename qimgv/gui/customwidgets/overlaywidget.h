/* Base class for floating widgets.
 * It will automatically reposition itself according to FloatingWidgetPosition.
 */

#pragma once

#include "gui/customwidgets/floatingwidget.h"
#include "gui/uimetrics.h"
#include <QTimeLine>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QDebug>
#include <QPoint>

class IconWidget;
class QLabel;

enum FloatingWidgetPosition {
    LEFT,
    RIGHT,
    BOTTOM,
    TOP,
    TOPLEFT,
    TOPRIGHT,
    BOTTOMLEFT,
    BOTTOMRIGHT,
    CENTER
};

class OverlayWidget : public FloatingWidget
{
    Q_OBJECT
    Q_PROPERTY (qreal opacity READ opacity WRITE setOpacity)
public:
    OverlayWidget(FloatingWidgetContainer *parent);
    ~OverlayWidget();
    void setHorizontalMargin(int);
    void setVerticalMargin(int);
    int horizontalMargin();
    int verticalMargin();
    void setPosition(FloatingWidgetPosition pos);
    void setFadeDuration(int duration);
    void setFadeEnabled(bool mode);

public slots:
    void show();
    void hide();
    void hideAnimated();

private:
    QGraphicsOpacityEffect *opacityEffect;
    int mHorizontalMargin, mVerticalMargin;
    bool fadeEnabled;
    QPropertyAnimation *fadeAnimation;

private slots:
    void setOpacity(qreal opacity);
    qreal opacity() const;

protected:
    static constexpr int kHeaderIconSizePx = UiMetrics::kStandardIconSizePx;
    static constexpr int kCloseIconSizePx = UiMetrics::kCompactIconSizePx;

    // Nudges a header icon vertically so it lands on the neighboring title
    // label's optical (cap-height) center instead of the label's full
    // ascent/descent geometric center - same rationale and formula as
    // MenuItem::alignIconToTextBaseline(), reused here so every overlay
    // header (rename, copy, image info, save confirm, color adjustments,
    // CAS settings...) gets the same icon/text alignment as context menu
    // items. Call once, right after both widgets are created and the
    // label's final font is set.
    static void alignHeaderIconToLabel(IconWidget *icon, QLabel *label,
                                        QPoint extraOffset = QPoint(0, 0));
    static void alignCloseIcon(IconWidget &icon);

    virtual void recalculateGeometry();
    FloatingWidgetPosition position;
};
