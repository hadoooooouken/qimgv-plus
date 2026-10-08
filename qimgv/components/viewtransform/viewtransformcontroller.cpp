#include "viewtransformcontroller.h"

#include <utility>

ViewTransformController::ViewTransformController(const IViewSurface &surface, QObject *parent)
    : QObject(parent),
      surface(surface) {
    viewTransform.setViewportSize(surface.viewportSize());
}

const ViewTransform &ViewTransformController::transform() const {
    return viewTransform;
}

const PanoramaView &ViewTransformController::panorama() const {
    return panoramaView;
}

// Runs one transform operation against the current viewport and pointer,
// then publishes what changed.
template <typename Operation>
void ViewTransformController::run(Operation &&operation) {
    const float scaleBefore = viewTransform.scale();
    const QPointF positionBefore = viewTransform.imagePosition();

    viewTransform.setViewportSize(surface.viewportSize());
    std::forward<Operation>(operation)(viewTransform, surface.pointerPosition());

    const ViewTransformEvents events = viewTransform.takeEvents();
    const bool scaleChangedNow = viewTransform.scale() != scaleBefore;
    const bool positionChangedNow = viewTransform.imagePosition() != positionBefore;
    if (scaleChangedNow || positionChangedNow)
        emit transformChanged();
    if (scaleChangedNow)
        emit scaleChanged(viewTransform.scale());
    if (positionChangedNow)
        emit positionChanged();
    if (events.imageCentered)
        emit imageCentered();
    if (events.anchoredZoom)
        emit anchoredZoomApplied();
}

// --- configuration and environment -----------------------------------------

bool ViewTransformController::applyConfig(const ViewTransformConfig &config) {
    bool fitScaleSettingsChanged = false;
    run([&](ViewTransform &t, QPointF pointer) {
        fitScaleSettingsChanged = t.applyConfig(config, pointer);
    });
    return fitScaleSettingsChanged;
}

void ViewTransformController::setExpandImage(bool enabled) {
    run([enabled](ViewTransform &t, QPointF pointer) { t.setExpandImage(enabled, pointer); });
}

void ViewTransformController::setExpandSmallImagesInFitMode(bool enabled) {
    viewTransform.setExpandSmallImagesInFitMode(enabled);
}

void ViewTransformController::setDevicePixelRatio(qreal dpr) {
    viewTransform.setDevicePixelRatio(dpr);
}

void ViewTransformController::changeDevicePixelRatio(qreal dpr) {
    run([dpr](ViewTransform &t, QPointF pointer) { t.changeDevicePixelRatio(dpr, pointer); });
}

void ViewTransformController::syncViewport() {
    run([](ViewTransform &, QPointF) {});
}

void ViewTransformController::refitToViewport() {
    run([](ViewTransform &t, QPointF pointer) { t.refitToViewport(pointer); });
}

// --- image lifecycle ----------------------------------------------------------

void ViewTransformController::clear() {
    run([](ViewTransform &t, QPointF) { t.clear(); });
}

PreservedView ViewTransformController::preservedView() {
    viewTransform.setViewportSize(surface.viewportSize());
    return viewTransform.preservedView();
}

void ViewTransformController::showImage(QSize imageSize) {
    run([imageSize](ViewTransform &t, QPointF pointer) { t.showImage(imageSize, pointer); });
}

void ViewTransformController::showTransformedImage(QSize imageSize, const PreservedView &view) {
    run([imageSize, &view](ViewTransform &t, QPointF pointer) {
        t.showTransformedImage(imageSize, view, pointer);
    });
}

// --- fit and zoom ---------------------------------------------------------------

void ViewTransformController::setFitMode(ImageFitMode mode) {
    run([mode](ViewTransform &t, QPointF pointer) { t.setFitMode(mode, pointer); });
}

void ViewTransformController::forceFitMode(ImageFitMode mode) {
    run([mode](ViewTransform &t, QPointF pointer) { t.forceFitMode(mode, pointer); });
}

void ViewTransformController::applyFitMode() {
    run([](ViewTransform &t, QPointF pointer) { t.applyFitMode(pointer); });
}

void ViewTransformController::setZoomAnchor(QPointF viewportPos) {
    run([viewportPos](ViewTransform &t, QPointF) { t.setZoomAnchor(viewportPos); });
}

void ViewTransformController::zoomTo(float scale) {
    run([scale](ViewTransform &t, QPointF) { t.zoomTo(scale); });
}

// --- scrolling ----------------------------------------------------------------------

void ViewTransformController::scrollBy(QPointF scrollDelta) {
    run([scrollDelta](ViewTransform &t, QPointF) { t.scrollBy(scrollDelta); });
}

void ViewTransformController::scrollTo(Qt::Orientation axis, int scrollValue) {
    run([axis, scrollValue](ViewTransform &t, QPointF) { t.scrollTo(axis, scrollValue); });
}

void ViewTransformController::saveViewportPosition() {
    run([](ViewTransform &t, QPointF) { t.saveViewportPosition(); });
}

// --- locks ----------------------------------------------------------------------------

void ViewTransformController::toggleLockZoom() {
    run([](ViewTransform &t, QPointF) { t.toggleLockZoom(); });
}

void ViewTransformController::toggleLockView() {
    run([](ViewTransform &t, QPointF) { t.toggleLockView(); });
}

// --- panorama -----------------------------------------------------------------------

void ViewTransformController::dragPanorama(QPoint delta) {
    panoramaView.drag(delta, surface.viewportSize().width());
    emit panoramaChanged();
}

void ViewTransformController::zoomPanoramaByWheel(int angleDelta) {
    panoramaView.zoomByWheel(angleDelta);
    emit panoramaChanged();
}

void ViewTransformController::zoomPanoramaByGesture(int moveDistance) {
    panoramaView.zoomByGesture(moveDistance, viewTransform.devicePixelRatio());
    emit panoramaChanged();
}

void ViewTransformController::resetPanorama() {
    panoramaView.reset();
    emit panoramaChanged();
}
