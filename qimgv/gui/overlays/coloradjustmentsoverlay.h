#pragma once

#include "gui/customwidgets/overlaywidget.h"

namespace Ui {
class ColorAdjustmentsOverlay;
}

class ColorAdjustmentsOverlay : public OverlayWidget {
    Q_OBJECT

public:
    explicit ColorAdjustmentsOverlay(FloatingWidgetContainer *parent = nullptr);
    ~ColorAdjustmentsOverlay();

    float brightness() const;
    float contrast() const;
    float saturation() const;
    float hue() const;
    float exposure() const;
    float temperature() const;
    float tint() const;

    void setCustomPosition(const QPoint &globalPos);

signals:
    void adjustmentsChanged(float brightness, float contrast, float saturation, float hue, float exposure, float temperature, float tint);
    void applyRequested(float brightness, float contrast, float saturation, float hue, float exposure, float temperature, float tint);

public slots:
    void show();
    void hide();
    void resetAdjustments();

protected:
    void recalculateGeometry() override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onSliderValueChanged();

private:
    Ui::ColorAdjustmentsOverlay *ui;
    QPoint customGlobalPos;
    bool hasCustomPos = false;
    QPoint dragStartPosition;
    QPoint dragStartWidgetPosition;
    bool isDragging = false;
};
