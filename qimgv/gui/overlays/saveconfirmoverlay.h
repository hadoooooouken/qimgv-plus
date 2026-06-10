#pragma once

#include "gui/customwidgets/overlaywidget.h"
#include "settings.h"
#include <QPushButton>

class IconWidget;
class IconButton;

class SaveConfirmOverlay : public OverlayWidget
{
    Q_OBJECT
public:
    explicit SaveConfirmOverlay(FloatingWidgetContainer *parent = nullptr);
    ~SaveConfirmOverlay();

signals:
    void saveClicked();
    void saveAsClicked();
    void discardClicked();

private slots:
    void readSettings();

private:
    IconWidget *headerIcon = nullptr;
    IconButton *closeButton = nullptr;

    void setupUi();
};
