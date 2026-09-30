#include "controlsoverlay.h"

namespace {
constexpr int kControlsHideTimeoutMs = 2000;
constexpr int kFadeDurationMs = 230;
} // namespace

ControlsOverlay::ControlsOverlay(FloatingWidgetContainer *parent) :
    FloatingWidget(parent)
{
    folderViewButton = new ActionButton("folderView", FluentIcon::Grid20, 20, 30);
    folderViewButton->setAccessibleName("ButtonSmall");
    folderViewButton->setObjectName("controlsOverlayLeftButton");
    folderViewButton->setColor(Qt::white);
    settingsButton = new ActionButton("openSettings", FluentIcon::Settings20, 20, 30);
    settingsButton->setAccessibleName("ButtonSmall");
    settingsButton->setColor(Qt::white);
    closeButton = new ActionButton("exit", FluentIcon::Dismiss16, 16, 30);
    closeButton->setAccessibleName("ButtonSmall");
    closeButton->setColor(Qt::white);

    layout.setContentsMargins(0,0,0,0);
    this->setContentsMargins(0,0,0,0);
    layout.setSpacing(0);
    layout.addWidget(folderViewButton);
    layout.addWidget(settingsButton);
    layout.addWidget(closeButton);
    setLayout(&layout);
    fitToContents();

    setMouseTracking(true);

    fadeEffect = new QGraphicsOpacityEffect(this);
    this->setGraphicsEffect(fadeEffect);
    fadeAnimation = new QPropertyAnimation(fadeEffect, "opacity", this);
    fadeAnimation->setDuration(kFadeDurationMs);
    fadeAnimation->setStartValue(1.0f);
    fadeAnimation->setEndValue(0.0f);
    fadeAnimation->setEasingCurve(QEasingCurve::OutQuart);

    hideTimer.setSingleShot(true);
    connect(&hideTimer, &QTimer::timeout, this, &ControlsOverlay::onHideTimeout);

    if(parent)
        setContainerSize(parent->size());
}

void ControlsOverlay::show() {
    fadeAnimation->stop();
    fadeEffect->setOpacity(1.0);
    FloatingWidget::show();
    hideTimer.start(kControlsHideTimeoutMs);
}

void ControlsOverlay::hide() {
    hideTimer.stop();
    fadeAnimation->stop();
    FloatingWidget::hide();
}

void ControlsOverlay::onPointerMoved() {
    if (!isVisible())
        return;

    if (fadeAnimation->state() == QAbstractAnimation::Running) {
        fadeAnimation->stop();
    }
    if (fadeEffect->opacity() < 1.0) {
        fadeEffect->setOpacity(1.0);
    }

    if (!rect().contains(mapFromGlobal(QCursor::pos()))) {
        hideTimer.start(kControlsHideTimeoutMs);
    } else {
        hideTimer.stop();
    }
}

void ControlsOverlay::onHideTimeout() {
    if (rect().contains(mapFromGlobal(QCursor::pos()))) {
        return;
    }
    fadeAnimation->stop();
    fadeAnimation->setStartValue(fadeEffect->opacity());
    fadeAnimation->setEndValue(0.0f);
    fadeAnimation->start();
}

QSize ControlsOverlay::contentsSize() {
    QSize newSize(0, 0);
    for(int i=0; i<layout.count(); i++) {
        newSize.setWidth(newSize.width() + layout.itemAt(i)->widget()->width());
        newSize.setHeight(layout.itemAt(i)->widget()->height());
    }
    return newSize;
}

void ControlsOverlay::fitToContents() {
    this->setFixedSize(contentsSize());
    recalculateGeometry();
}

void ControlsOverlay::recalculateGeometry() {
    setGeometry(containerSize().width() - width(), 0, width(), height());
}

void ControlsOverlay::enterEvent(QEnterEvent *event) {
    Q_UNUSED(event)
    hideTimer.stop();
    fadeAnimation->stop();
    fadeEffect->setOpacity(1.0);
}

void ControlsOverlay::leaveEvent(QEvent *event) {
    Q_UNUSED(event)
    if (isVisible()) {
        hideTimer.start(kControlsHideTimeoutMs);
    }
}
