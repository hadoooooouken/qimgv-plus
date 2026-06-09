#pragma once

#include "gui/customwidgets/overlaywidget.h"

class QSlider;
class QLabel;

class CasSettingsOverlay : public OverlayWidget {
    Q_OBJECT

public:
    explicit CasSettingsOverlay(FloatingWidgetContainer *parent = nullptr);
    ~CasSettingsOverlay();

    void setCustomPosition(const QPoint &globalPos);

signals:
    void casSettingsChanged(float sharpening, float contrast);

public slots:
    void show();
    void hide();

protected:
    void recalculateGeometry() override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onSliderValueChanged();
    void onResetClicked();

private:
    void setupUi();
    void updateValueLabels();

    QSlider *m_sharpenSlider = nullptr;
    QLabel  *m_sharpenValLabel = nullptr;
    QSlider *m_contrastSlider = nullptr;
    QLabel  *m_contrastValLabel = nullptr;

    QPoint customGlobalPos;
    bool hasCustomPos = false;
    QPoint dragStartPosition;
    QPoint dragStartWidgetPosition;
    bool isDragging = false;
};
