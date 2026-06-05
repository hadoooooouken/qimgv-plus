#include "zoomindicatoroverlay.h"

ZoomIndicatorOverlay::ZoomIndicatorOverlay(FloatingWidgetContainer *parent) : OverlayWidget(parent) {
    hideDelay = 2000;
    visibilityTimer.setSingleShot(true);
    visibilityTimer.setInterval(hideDelay);

    layout.setContentsMargins(0,0,0,0);
    layout.addWidget(&label);

    QFont font = label.font();
    if (font.pointSizeF() > 0) {
        font.setPointSizeF(font.pointSizeF() * 2.0);
    } else if (font.pixelSize() > 0) {
        font.setPixelSize(font.pixelSize() * 2);
    } else {
        font.setPointSize(18);
    }
    label.setFont(font);

    fm = new QFontMetrics(label.font());
    label.setAlignment(Qt::AlignCenter);

    this->setLayout(&layout);
    this->setPosition(FloatingWidgetPosition::BOTTOMLEFT);
    setHorizontalMargin(0);
    setVerticalMargin(16);

    setFadeEnabled(true);
    setFadeDuration(300);

    connect(&visibilityTimer, &QTimer::timeout, this, &ZoomIndicatorOverlay::hideAnimated);

    if(parent)
        setContainerSize(parent->size());
}

void ZoomIndicatorOverlay::setScale(qreal scale) {
    label.setText(QString::number(qRound(scale * 100.0))+"%");
    label.setFixedSize(fm->horizontalAdvance(label.text()) + 14, fm->height() + 12);
    recalculateGeometry();
}

void ZoomIndicatorOverlay::recalculateGeometry() {
    OverlayWidget::recalculateGeometry();
}

void ZoomIndicatorOverlay::show() {
    OverlayWidget::show();
}

// "blink" the widget; show then fade out immediately
void ZoomIndicatorOverlay::show(int duration) {
    visibilityTimer.stop();
    OverlayWidget::show();
    // fade out after delay
    visibilityTimer.setInterval(duration);
    visibilityTimer.start();
}
