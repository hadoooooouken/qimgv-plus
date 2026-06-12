#pragma once

#include "draggableslideroverlay.h"

class QSlider;
class QLabel;

class CasSettingsOverlay : public DraggableSliderOverlay {
    Q_OBJECT

public:
    explicit CasSettingsOverlay(FloatingWidgetContainer *parent = nullptr);
    ~CasSettingsOverlay();

signals:
    void casSettingsChanged(float sharpening, float contrast);

public slots:
    void show();
    void hide();

protected:
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
};
