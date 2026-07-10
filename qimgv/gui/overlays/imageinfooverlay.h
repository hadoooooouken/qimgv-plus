#pragma once

#include "gui/customwidgets/overlaywidget.h"
#include "gui/customwidgets/entryinfoitem.h"
#include <QWheelEvent>
#include <QMouseEvent>

class IconWidget;
class IconButton;
class QVBoxLayout;

class ImageInfoOverlay : public OverlayWidget
{
    Q_OBJECT

public:
    explicit ImageInfoOverlay(FloatingWidgetContainer *parent = nullptr);
    ~ImageInfoOverlay();
    void setExifInfo(QList<QPair<QString, QString>>);

public slots:
    void show();

protected:
    void wheelEvent(QWheelEvent *event);
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void recalculateGeometry() override;

private:
    IconWidget *headerIcon = nullptr;
    IconButton *closeButton = nullptr;
    QVBoxLayout *entryLayout = nullptr;

    QList<EntryInfoItem*> entries;
    QLabel entryStub;

    bool mDragging = false;
    bool mManuallyPositioned = false;
    QPoint mDragStartPos;
    QPoint mDragStartWidgetPos;

    void setupUi();
};
