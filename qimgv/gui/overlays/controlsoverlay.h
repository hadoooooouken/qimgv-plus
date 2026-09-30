#pragma once

#include <QHBoxLayout>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QTimer>
#include <QDebug>
#include "gui/customwidgets/floatingwidget.h"
#include "gui/customwidgets/actionbutton.h"

class ControlsOverlay : public FloatingWidget
{
    Q_OBJECT
public:
    explicit ControlsOverlay(FloatingWidgetContainer *parent);

public slots:
    void show();
    void hide();
    void onPointerMoved();

private slots:
    void onHideTimeout();

private:
    QHBoxLayout layout;
    ActionButton *closeButton, *settingsButton, *folderViewButton;
    QGraphicsOpacityEffect *fadeEffect;
    QPropertyAnimation *fadeAnimation;
    QTimer hideTimer;
    QSize contentsSize();
    void fitToContents();

protected:
    virtual void recalculateGeometry();
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
};
