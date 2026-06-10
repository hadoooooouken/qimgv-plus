#pragma once

#include "gui/customwidgets/overlaywidget.h"
#include "gui/customwidgets/entryinfoitem.h"
#include <QWheelEvent>

class IconWidget;
class IconButton;
class QVBoxLayout;

class ImageInfoOverlay : public OverlayWidget
{
    Q_OBJECT

public:
    explicit ImageInfoOverlay(FloatingWidgetContainer *parent = nullptr);
    ~ImageInfoOverlay();
    void setExifInfo(QMap<QString, QString>);

public slots:
    void show();

protected:
    void wheelEvent(QWheelEvent *event);

private:
    IconWidget *headerIcon = nullptr;
    IconButton *closeButton = nullptr;
    QVBoxLayout *entryLayout = nullptr;

    QList<EntryInfoItem*> entries;
    QLabel entryStub;

    void setupUi();
};
