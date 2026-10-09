#pragma once

#include <QtQml/qqmlregistration.h>

#include "gui/quick/ui/overlays/adjustmentslidermodel.h"
#include "utils/coloradjustments.h"

// Sliders of the colour adjustments overlay (ColorAdjustmentsOverlay.qml),
// with the ranges, defaults and value texts of the widget overlay. Every edit
// previews the adjustments live (previewChanged(); the GPU renderer applies
// them per frame, so no throttling is needed). Compare previews the
// unadjusted image while held; apply requests the edit and resets the
// sliders.
//
// Owned by OverlayCoordinator. GUI thread only.
class ColorAdjustmentsEditor final : public AdjustmentSliderModel {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by OverlayCoordinator")

public:
    // Row order of the overlay.
    enum Row { Exposure, Contrast, Brightness, Temperature, Tint, Saturation, Hue, RowCount };
    Q_ENUM(Row)

    explicit ColorAdjustmentsEditor(QObject *parent = nullptr);

    [[nodiscard]] ColorAdjustments adjustments() const;

    // Shows the image without adjustments while comparing is true.
    Q_INVOKABLE void setComparing(bool comparing);
    // Requests the adjustments to be applied to the image and resets the
    // sliders.
    Q_INVOKABLE void apply();

signals:
    // The adjustments the viewer shows now.
    void previewChanged(const ColorAdjustments &adjustments);
    void applyRequested(const ColorAdjustments &adjustments);

private:
    void valuesEdited() override;

    bool mComparing = false;
};
