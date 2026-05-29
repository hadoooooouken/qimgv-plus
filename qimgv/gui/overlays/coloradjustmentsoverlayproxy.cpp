#include "coloradjustmentsoverlayproxy.h"

ColorAdjustmentsOverlayProxy::ColorAdjustmentsOverlayProxy(FloatingWidgetContainer *parent)
    : container(parent)
{
}

ColorAdjustmentsOverlayProxy::~ColorAdjustmentsOverlayProxy() {
    if (overlay) {
        overlay->deleteLater();
    }
}

void ColorAdjustmentsOverlayProxy::init() {
    if (overlay) {
        return;
    }
    overlay = new ColorAdjustmentsOverlay(container);
    connect(overlay, &ColorAdjustmentsOverlay::adjustmentsChanged, this, &ColorAdjustmentsOverlayProxy::adjustmentsChanged);
    connect(overlay, &ColorAdjustmentsOverlay::applyRequested, this, &ColorAdjustmentsOverlayProxy::applyRequested);
}

void ColorAdjustmentsOverlayProxy::show() {
    init();
    overlay->show();
}

void ColorAdjustmentsOverlayProxy::hide() {
    if (overlay) {
        overlay->hide();
    }
}

bool ColorAdjustmentsOverlayProxy::isHidden() const {
    return overlay ? overlay->isHidden() : true;
}

ColorAdjustmentsOverlay *ColorAdjustmentsOverlayProxy::overlayWidget() {
    init();
    return overlay;
}

void ColorAdjustmentsOverlayProxy::setCustomPosition(const QPoint &globalPos) {
    init();
    if (overlay) {
        overlay->setCustomPosition(globalPos);
    }
}
