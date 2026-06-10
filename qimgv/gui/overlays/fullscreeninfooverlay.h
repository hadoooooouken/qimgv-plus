#pragma once

#include <QWidget>
#include "gui/customwidgets/overlaywidget.h"

class QLabel;

class FullscreenInfoOverlay : public OverlayWidget {
    Q_OBJECT

public:
    explicit FullscreenInfoOverlay(FloatingWidgetContainer *parent = nullptr);
    ~FullscreenInfoOverlay();
    void setInfo(QString pos, QString fileName, QString info);

private:
    QLabel *posLabel = nullptr;
    QLabel *nameLabel = nullptr;
    QLabel *infoLabel = nullptr;

    void setupUi();
};
