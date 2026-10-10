#include "quickviewerport.h"

#include <utility>

#include "gui/quick/ui/imageviewportcontroller.h"

QuickViewerPort::QuickViewerPort(ImageViewportController &viewport, QObject *parent)
    : QObject(parent),
      viewport(viewport) {
}

void QuickViewerPort::showImage(std::shared_ptr<const QImage> image, const QString &filePath) {
    if (image)
        emit documentShown(image->size());
    viewport.showImage(std::move(image), filePath);
}

void QuickViewerPort::showAnimation(const QString &filePath, const QString &format, QSize size) {
    emit documentShown(size);
    viewport.showAnimation(filePath, format);
}

void QuickViewerPort::closeImage() {
    viewport.closeImage();
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
