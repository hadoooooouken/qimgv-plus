#pragma once

#include <QObject>
#include <QPoint>
#include <QPointF>
#include <QSize>

#include "viewtransform.h"

// Live state of the view that hosts the transform. The controller samples it
// at the start of every operation, so geometry always matches the current
// viewport, the way the viewer used to read it directly.
class IViewSurface {
public:
    virtual ~IViewSurface() = default;
    // Viewport size in logical pixels.
    [[nodiscard]] virtual QSize viewportSize() const = 0;
    // Pointer position in viewport coordinates (may lie outside it).
    [[nodiscard]] virtual QPointF pointerPosition() const = 0;
};

// Owns the zoom/pan/fit state of one image view (ViewTransform) and its
// panorama camera (PanoramaView), and notifies the view after each change.
// Viewers forward user intents here and render what transform() describes.
class ViewTransformController final : public QObject {
    Q_OBJECT
public:
    // `surface` must outlive the controller.
    explicit ViewTransformController(const IViewSurface &surface, QObject *parent = nullptr);

    [[nodiscard]] const ViewTransform &transform() const;
    [[nodiscard]] const PanoramaView &panorama() const;

    // Configuration and environment.
    // Returns whether a setting that affects the fit scale changed.
    bool applyConfig(const ViewTransformConfig &config);
    void setExpandImage(bool enabled);
    void setExpandSmallImagesInFitMode(bool enabled);
    void setDevicePixelRatio(qreal dpr);
    void changeDevicePixelRatio(qreal dpr);
    void syncViewport();
    void refitToViewport();

    // Image lifecycle.
    void clear();
    [[nodiscard]] PreservedView preservedView();
    void showImage(QSize imageSize);
    void showTransformedImage(QSize imageSize, const PreservedView &view);

    // Fit and zoom.
    void setFitMode(ImageFitMode mode);
    void forceFitMode(ImageFitMode mode);
    void applyFitMode();
    void setZoomAnchor(QPointF viewportPos);
    void zoomTo(float scale);

    // Scrolling.
    void scrollBy(QPointF scrollDelta);
    void scrollTo(Qt::Orientation axis, int scrollValue);
    void saveViewportPosition();

    // Locks.
    void toggleLockZoom();
    void toggleLockView();

    // Panorama camera.
    void dragPanorama(QPoint delta);
    void zoomPanoramaByWheel(int angleDelta);
    void zoomPanoramaByGesture(int moveDistance);
    void resetPanorama();

signals:
    // Scale or image position changed; emitted first, so the view can render
    // the new geometry before the more specific signals below.
    void transformChanged();
    void scaleChanged(qreal scale);
    void positionChanged();
    // The whole image was centred by fit-to-window.
    void imageCentered();
    // An anchored zoom was applied (the view refreshes high-quality scaling).
    void anchoredZoomApplied();
    void panoramaChanged();

private:
    template <typename Operation>
    void run(Operation &&operation);

    const IViewSurface &surface;
    ViewTransform viewTransform;
    PanoramaView panoramaView;
};
