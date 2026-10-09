#pragma once

#include <QtQml/qqmlregistration.h>

#include "gui/quick/ui/overlays/adjustmentslidermodel.h"

// FidelityFX CAS parameters for the viewer's CAS filter.
struct CasParameters {
    // Settings::casSharpening() default and range.
    static constexpr float kDefaultSharpening = 1.0f;
    static constexpr float kDefaultContrast = 0.0f;

    float sharpening = kDefaultSharpening;
    float contrast = kDefaultContrast;

    friend bool operator==(const CasParameters &, const CasParameters &) = default;
};

// Sliders of the CAS settings overlay (CasSettingsOverlay.qml): sharpening
// and contrast in hundredths, with the ranges and defaults of the widget
// overlay. load() shows the current parameters when the overlay opens; every
// edit publishes parametersEdited(), which the application stores and
// applies to the viewer.
//
// Owned by OverlayCoordinator. GUI thread only.
class CasSettingsEditor final : public AdjustmentSliderModel {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by OverlayCoordinator")

public:
    enum Row { Sharpening, Contrast, RowCount };
    Q_ENUM(Row)

    explicit CasSettingsEditor(QObject *parent = nullptr);

    [[nodiscard]] CasParameters parameters() const;
    // Shows parameters (rounded to the slider steps) without an edit.
    void load(const CasParameters &parameters);

signals:
    void parametersEdited(const CasParameters &parameters);

private:
    void valuesEdited() override;
};
