#pragma once

#include <QHBoxLayout>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QDebug>
#include "gui/customwidgets/floatingwidget.h"
#include "utils/imagelib.h"

enum ActiveHighlightZone {
    HIGHLIGHT_NONE,
    HIGHLIGHT_LEFT,
    HIGHLIGHT_RIGHT
};

class ClickZoneOverlay : public FloatingWidget
{
    Q_OBJECT
public:
    explicit ClickZoneOverlay(FloatingWidgetContainer *parent);
    ~ClickZoneOverlay();
    QRect leftZone();
    QRect rightZone();
    void highlightLeft();
    void highlightRight();
    void disableHighlight();
    void setHighlightedZone(ActiveHighlightZone zone);
    bool isHighlighted();
    void setPressed(bool mode);

private slots:
    void onAnimationFinished();

public slots:
    void readSettings();

private:
    // Layout constants
    static constexpr int kZoneWidth      = 110; // full-width hit zone (unchanged)
    static constexpr int kButtonWidth    = 100; // visible pill width
    static constexpr int kButtonEdgeMargin = 16; // gap from window edge to pill
    static constexpr int kButtonHeightDivisor = 3; // visible height = window_h / 3
    static constexpr int kButtonMinHeight  = 80; // safe minimum height
    static constexpr int kButtonRadius     = 8;  // rounded corner radius

    QPixmap* loadPixmap(QString path);
    QPixmap *pixmapLeft = nullptr, *pixmapRight = nullptr;
    // Hit zones (full height, used for mouse detection)
    QRect mLeftZone, mRightZone;
    // Visible pill rects (subset of hit zones, used for painting)
    QRect mLeftButton, mRightButton;
    QColor mButtonColor;
    qreal dpr, pixmapDrawScale;
    bool hiResPixmaps = false;
    bool isPressed = false;
    bool leftHovered = false, rightHovered = false;
    bool drawZones = true;
    ActiveHighlightZone activeZone = HIGHLIGHT_NONE;
    QGraphicsOpacityEffect *fadeEffect;
    QPropertyAnimation *fadeAnimation;
    void drawPixmap(QPainter &p, QPixmap *pixmap, QRect buttonRect);

protected:
    virtual void resizeEvent(QResizeEvent *event);
    virtual void paintEvent(QPaintEvent *event);
    virtual void recalculateGeometry();
};
