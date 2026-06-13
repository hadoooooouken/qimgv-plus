#pragma once

#include "draggableslideroverlay.h"

class QSlider;
class QLabel;
class QTimer;

class ColorAdjustmentsOverlay : public DraggableSliderOverlay {
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

signals:
    void adjustmentsChanged(float exposure, float contrast, float brightness,
                            float temperature, float tint, float saturation, float hue);
    void applyRequested(float exposure, float contrast, float brightness,
                        float temperature, float tint, float saturation, float hue);

public slots:
    void show();
    void hide();
    void resetAdjustments();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onSliderValueChanged();
    void onTimerTimeout();
    void onSliderReleased();

private:
    void setupUi();
    void updateValueLabels();

    QTimer  *m_updateTimer = nullptr;
    bool     m_pendingUpdate = false;

    QSlider *m_brightnessSlider = nullptr;
    QLabel  *m_brightnessValLabel = nullptr;
    QSlider *m_contrastSlider = nullptr;
    QLabel  *m_contrastValLabel = nullptr;
    QSlider *m_saturationSlider = nullptr;
    QLabel  *m_saturationValLabel = nullptr;
    QSlider *m_hueSlider = nullptr;
    QLabel  *m_hueValLabel = nullptr;
    QSlider *m_exposureSlider = nullptr;
    QLabel  *m_exposureValLabel = nullptr;
    QSlider *m_temperatureSlider = nullptr;
    QLabel  *m_temperatureValLabel = nullptr;
    QSlider *m_tintSlider = nullptr;
    QLabel  *m_tintValLabel = nullptr;
};
