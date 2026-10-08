#pragma once

#include <QList>
#include <QPoint>
#include <QPointF>
#include <QRect>
#include <QSize>
#include <QSizeF>
#include <QString>

#include "settings_types.h"

// Viewer settings that drive the view transform. The viewer that owns the
// transform reads them from Settings; the model itself never touches global
// state, so it can be exercised by unit tests and shared by both UIs.
struct ViewTransformConfig {
    bool expandImage = false;
    // Raw "expand limit" setting. Values below 1 mean "no limit"; see
    // ViewTransform::effectiveExpandLimit().
    float expandLimit = 0.0f;
    bool keepFitMode = false;
    ImageFitMode defaultFitMode = FIT_WINDOW;
    float zoomStep = 0.1f;
    ImageFocusPoint focusPoint = FOCUS_TOP;
    bool useFixedZoomLevels = false;
    // Ascending; see parseZoomLevels().
    QList<float> zoomLevels;
    bool unlockMinZoom = false;

    bool operator==(const ViewTransformConfig &) const = default;
};

// Parses the comma-separated "zoom levels" setting into an ascending list.
[[nodiscard]] QList<float> parseZoomLevels(const QString &levels);

enum class ViewLock {
    None,
    Zoom, // keep the scale across images
    All   // keep the scale and the relative viewport position
};

// What a rotate/mirror of the displayed image keeps from the previous view.
struct PreservedView {
    float scale = 1.0f;
    ImageFitMode fitMode = FIT_WINDOW;
    // Viewport centre relative to the image, [0..1] on both axes.
    QPointF relativeCenter;
};

// Side effects of the last operations that a viewer reacts to beyond the
// geometry itself. Collected by the model, drained by the controller.
struct ViewTransformEvents {
    // An anchored zoom changed (or re-applied) the scale; the widget viewer
    // requests a high-quality rescale at this point.
    bool anchoredZoom = false;
    // The image was centred as a whole by a fit-to-window operation.
    bool imageCentered = false;
};

// UI-independent zoom / pan / fit model of the image viewer.
//
// Units:
//  - image size: physical source pixels;
//  - viewport size and image position: logical (device-independent) pixels,
//    viewport coordinates, origin at the viewport top-left;
//  - scale: device pixels per source pixel, so 1.0 shows the image 1:1 on
//    screen at any device pixel ratio. The image covers
//    imageSize * scale / devicePixelRatio logical pixels.
//
// The image position is kept on whole logical pixels, which is where the
// widget viewer's scroll offsets used to place it.
class ViewTransform {
public:
    static constexpr float kMaxScale = 40.0f;
    // Scales closer than this to a fit scale count as that fit mode.
    static constexpr float kScaleEpsilon = 0.001f;

    // --- configuration and environment ---------------------------------
    [[nodiscard]] const ViewTransformConfig &config() const;
    // Applies changed settings the way the viewer does on a settings
    // change: re-fits the displayed image when a fit-relevant setting
    // changed. Returns whether such a setting changed.
    bool applyConfig(const ViewTransformConfig &newConfig, QPointF pointer);
    // Session override of config().expandImage; re-fits the image.
    void setExpandImage(bool enabled, QPointF pointer);
    // Temporarily upscale small images in fit-window mode even when
    // expandImage is off.
    void setExpandSmallImagesInFitMode(bool enabled);
    [[nodiscard]] float effectiveExpandLimit() const;

    void setViewportSize(QSize size);
    [[nodiscard]] QSize viewportSize() const;
    // Whole-pixel viewport centre (QRect::center() semantics).
    [[nodiscard]] QPointF viewportCenter() const;
    void setDevicePixelRatio(qreal dpr);
    [[nodiscard]] qreal devicePixelRatio() const;
    // Device pixel ratio change while displaying: updates limits and
    // re-applies the fit mode.
    void changeDevicePixelRatio(qreal dpr, QPointF pointer);
    // Viewport resize while visible: re-fits or keeps the free view in
    // bounds, then refreshes the saved viewport position.
    void refitToViewport(QPointF pointer);

    // --- image lifecycle ------------------------------------------------
    [[nodiscard]] bool hasImage() const;
    [[nodiscard]] QSize imageSize() const;
    // Drops the image; the scale returns to 1 and the (empty) image origin
    // moves to the viewport centre.
    void clear();
    // Presents a new image: applies the default or kept fit mode, or the
    // locked scale/position.
    void showImage(QSize newImageSize, QPointF pointer);
    [[nodiscard]] PreservedView preservedView() const;
    // Presents a rotated/mirrored version of the previous image, keeping
    // its scale or fit mode and the relative viewport position.
    void showTransformedImage(QSize newImageSize, const PreservedView &view,
                              QPointF pointer);

    // --- queries ----------------------------------------------------------
    [[nodiscard]] float scale() const;
    [[nodiscard]] QPointF imagePosition() const;
    [[nodiscard]] ImageFitMode fitMode() const;
    [[nodiscard]] float minScale() const;
    [[nodiscard]] float fitWindowScale() const;
    [[nodiscard]] float fitWidthScale() const;
    [[nodiscard]] float fitHeightScale() const;
    // Image size on screen, logical pixels.
    [[nodiscard]] QSizeF scaledSizeF() const;
    [[nodiscard]] QSize scaledSize() const;
    // Image rect on screen, rounded to whole pixels (QRect from corners).
    [[nodiscard]] QRect scaledRect() const;
    // True when the source fits into the viewport at 1:1.
    [[nodiscard]] bool imageFits() const;
    // True when the image fits into the viewport at the current scale.
    [[nodiscard]] bool scaledImageFits() const;
    // Viewport centre relative to the image, [0..1] on both axes.
    [[nodiscard]] QPointF relativeViewportCenter() const;

    // --- fit modes --------------------------------------------------------
    void setFitMode(ImageFitMode mode, QPointF pointer);
    // Fits ignoring the expand limit (the explicit "fit width/height/window"
    // actions).
    void forceFitMode(ImageFitMode mode, QPointF pointer);
    void applyFitMode(QPointF pointer);
    // Fit mode a manual scale corresponds to (FIT_FREE if none).
    [[nodiscard]] ImageFitMode fitModeForScale(float value) const;

    // --- zoom -------------------------------------------------------------
    [[nodiscard]] float zoomInScale(float baseScale) const;
    [[nodiscard]] float zoomOutScale(float baseScale) const;
    // Scale reached by the right-button zoom gesture moving `moveDistance`
    // logical pixels up.
    [[nodiscard]] float gestureZoomScale(int moveDistance) const;
    // Remembers which image point is under `viewportPos`; anchored zooms keep
    // it there.
    void setZoomAnchor(QPointF viewportPos);
    // Manual zoom: anchored zoom, keep in bounds, re-detect the fit mode.
    void zoomTo(float newScale);

    // --- scrolling ----------------------------------------------------------
    // Scroll offset: how far the view is scrolled right/down, the negated
    // image position.
    [[nodiscard]] QPoint scrollPosition() const;
    // Scrolls by `scrollDelta` (positive = towards the right/bottom of the
    // image) and keeps the image in bounds.
    void scrollBy(QPointF scrollDelta);
    void scrollTo(Qt::Orientation axis, int scrollValue);

    // --- locks ----------------------------------------------------------------
    [[nodiscard]] ViewLock lock() const;
    void toggleLockZoom();
    void toggleLockView();
    // Stores the relative viewport position when the view is locked.
    void saveViewportPosition();

    ViewTransformEvents takeEvents();

private:
    void updateScaleLimits();
    void updateFitWindowScale();
    void updateFitWidthScale();
    void updateFitHeightScale();
    [[nodiscard]] float limitExpansion(float fitScale) const;
    [[nodiscard]] float windowFitScale() const;
    [[nodiscard]] float widthFitScale() const;
    [[nodiscard]] float heightFitScale() const;

    void zoom(float newScale);
    void zoomAnchored(float newScale);
    void fitWindow(bool force);
    void fitWidth(bool force, QPointF pointer);
    void fitHeight(bool force, QPointF pointer);
    void fitFree(float newScale, QPointF pointer);
    void focusOverflowingAxis(Qt::Orientation axis);
    void presentImage(QPointF pointer);
    void lockZoom();
    void applySavedViewportPosition();

    void centerImage();
    void centerIfNecessary();
    void snapToEdges();
    void centerOnImagePoint(QPointF imageOffset);
    [[nodiscard]] QPointF anchorImageOffset() const;

    ViewTransformConfig mConfig;
    QSize mImageSize;
    QSize mViewportSize;
    qreal mDpr = 1.0;

    float mScale = 1.0f;
    QPointF mImagePos;
    ImageFitMode mFitMode = FIT_WINDOW;
    bool mExpandSmallImagesInFitMode = false;

    float mMinScale = kInitialMinScale;
    float mFitWindowScale = kInitialFitScale;
    float mFitWidthScale = kInitialFitScale;
    float mFitHeightScale = kInitialFitScale;

    ViewLock mLock = ViewLock::None;
    float mLockedScale = 1.0f;
    QPointF mSavedViewportPos{kCenteredRelativePos, kCenteredRelativePos};

    // Anchor point in unscaled logical image coordinates, and where it has
    // to stay on the viewport.
    QPointF mZoomAnchorImage;
    QPointF mZoomAnchorViewport;

    ViewTransformEvents mEvents;

    static constexpr float kInitialMinScale = 0.01f;
    static constexpr float kInitialFitScale = 0.125f;
    static constexpr qreal kCenteredRelativePos = 0.5;
};

// Yaw/pitch/field-of-view state of the 360° panorama mode.
class PanoramaView {
public:
    static constexpr float kDefaultFov = 90.0f;
    static constexpr float kMinFov = 10.0f;
    static constexpr float kMaxFov = 140.0f;
    static constexpr float kMaxPitch = 89.0f;

    [[nodiscard]] float yaw() const;
    [[nodiscard]] float pitch() const;
    [[nodiscard]] float fov() const;

    // Mouse drag by `delta` logical pixels in a viewport `viewportWidth`
    // logical pixels wide.
    void drag(QPoint delta, int viewportWidth);
    void zoomByWheel(int angleDelta);
    // Right-button zoom gesture, same sensitivity as the flat viewer.
    void zoomByGesture(int moveDistance, qreal dpr);
    void reset();

private:
    void clampFov();

    float mYaw = 0.0f;
    float mPitch = 0.0f;
    float mFov = kDefaultFov;
};
