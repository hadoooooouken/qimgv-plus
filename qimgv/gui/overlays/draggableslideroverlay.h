#pragma once

#include "gui/customwidgets/overlaywidget.h"
#include <QPoint>
#include <QString>

class QFormLayout;
class QSlider;
class QLabel;
class QHBoxLayout;
enum class FluentIcon;

class DraggableSliderOverlay : public OverlayWidget {
    Q_OBJECT

public:
    explicit DraggableSliderOverlay(FloatingWidgetContainer *parent = nullptr);
    virtual ~DraggableSliderOverlay();

    void setCustomPosition(const QPoint &globalPos);

protected:
    QWidget *createHeader(const QString &title, FluentIcon icon);
    void addSliderRow(QFormLayout *formLayout,
                      const QString &labelText,
                      QSlider *&slider,
                      QLabel *&valLabel,
                      int min, int max, int defaultValue);

    void recalculateGeometry() override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

    QPoint customGlobalPos;
    bool hasCustomPos = false;
    QPoint dragStartPosition;
    QPoint dragStartWidgetPosition;
    bool isDragging = false;
};
