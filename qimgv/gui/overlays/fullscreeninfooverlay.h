#pragma once

#include <QWidget>
#include <QTimer>
#include "gui/customwidgets/overlaywidget.h"

class QLabel;

class FullscreenInfoOverlay : public OverlayWidget {
    Q_OBJECT

public:
    explicit FullscreenInfoOverlay(FloatingWidgetContainer *parent = nullptr);
    ~FullscreenInfoOverlay();
    void setInfo(QString pos, QString fileName, QString info);

public slots:
    void show();
    void hide();
    void onPointerMoved();

private slots:
    void onHideTimeout();

private:
    QLabel *posLabel = nullptr;
    QLabel *nameLabel = nullptr;
    QLabel *infoLabel = nullptr;
    QTimer hideTimer;
    bool mActive = false;

    void setupUi();
};
