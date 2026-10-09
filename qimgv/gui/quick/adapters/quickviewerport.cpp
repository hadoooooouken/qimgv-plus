#include "quickviewerport.h"

#include <utility>

#include "gui/quick/ui/imageviewportcontroller.h"

QuickViewerPort::QuickViewerPort(ImageViewportController &viewport)
    : viewport(viewport) {
}

void QuickViewerPort::showImage(std::shared_ptr<const QImage> image, const QString &filePath) {
    viewport.showImage(std::move(image), filePath);
}

// The window is not resized to the animation (autoResizeWindow) until the
// Quick main window takes over window geometry (S2.1).
void QuickViewerPort::showAnimation(const QString &filePath, const QString &format, QSize) {
    viewport.showAnimation(filePath, format);
}

void QuickViewerPort::closeImage() {
    viewport.closeImage();
}

void QuickViewerPort::showScaledImage(const QImage &) {
    // Intentionally not displayed; see the declaration.
}

void QuickViewerPort::showUpscaledCrop(const QImage &crop, const QRect &originalRect) {
    viewport.setUpscaledCrop(crop, originalRect);
}

void QuickViewerPort::hideUpscaledCrop() {
    viewport.hideUpscaledCrop();
}

void QuickViewerPort::refreshScaling() {
    viewport.refreshScaling();
}

QRect QuickViewerPort::visibleOriginalImageRect() const {
    return viewport.visibleOriginalImageRect();
}

float QuickViewerPort::currentScale() const {
    return viewport.currentScale();
}

float QuickViewerPort::devicePixelRatio() const {
    return viewport.devicePixelRatio();
}

bool QuickViewerPort::isBusyInteracting() const {
    return viewport.isBusyInteracting();
}

bool QuickViewerPort::isRenderingSettled() const {
    return viewport.isRenderingSettled();
}

bool QuickViewerPort::panoramaMode() const {
    return viewport.panoramaMode();
}
