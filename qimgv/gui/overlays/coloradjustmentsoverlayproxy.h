#pragma once

#include <QObject>
#include "gui/overlays/coloradjustmentsoverlay.h"

class ColorAdjustmentsOverlayProxy : public QObject {
    Q_OBJECT
public:
    explicit ColorAdjustmentsOverlayProxy(FloatingWidgetContainer *parent = nullptr);
    ~ColorAdjustmentsOverlayProxy();

    void init();
    void show();
    void hide();
    bool isHidden() const;

    ColorAdjustmentsOverlay *overlayWidget();
    void setCustomPosition(const QPoint &globalPos);

signals:
    void adjustmentsChanged(const ColorAdjustments &adjustments);
    void applyRequested(const ColorAdjustments &adjustments);

private:
    FloatingWidgetContainer *container;
    ColorAdjustmentsOverlay *overlay = nullptr;
};
