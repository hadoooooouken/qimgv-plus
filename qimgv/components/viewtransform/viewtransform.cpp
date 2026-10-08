#include "viewtransform.h"

#include <QStringList>
#include <QtGlobal>

#include <algorithm>
#include <cmath>
#include <utility>

namespace {
// Right-button zoom gesture: relative scale (or FOV) change per logical pixel
// of vertical mouse movement, multiplied by the device pixel ratio.
constexpr float kGestureZoomStepPerPixel = 0.003f;
// With "unlock min zoom" the image can shrink until its longer side spans
// this many device pixels.
constexpr qreal kUnlockedMinZoomSidePx = 10.0;

// Panorama drag: degrees of rotation per field of view dragged across the
// viewport width.
constexpr float kPanoramaDragSpeed = 2.0f;
constexpr float kPanoramaWheelZoomInFactor = 0.9f;
constexpr float kPanoramaWheelZoomOutFactor = 1.1f;

// Image coordinate along one axis that puts `imageOffset` (distance from the
// image's leading edge) at the viewport centre. Rounds the way the widget
// viewer's centerOn() + integer scroll offset did.
qreal centeredCoordinate(int viewportExtent, qreal imageOffset) {
    return std::ceil(viewportExtent / 2.0 - imageOffset);
}

// Leading-edge coordinate that centres an image of `imageExtent` inside the
// viewport (truncated to whole pixels, then halved in integer arithmetic).
qreal centeredEdge(int viewportExtent, qreal imageExtent) {
    return static_cast<int>(viewportExtent - imageExtent) / 2;
}
} // namespace

QList<float> parseZoomLevels(const QString &levels) {
    QList<float> result;
    const QStringList parts = levels.split(',');
    result.reserve(parts.size());
    for (const QString &part : parts)
        result.append(part.toFloat());
    std::sort(result.begin(), result.end());
    return result;
}

// --- configuration and environment -----------------------------------------

const ViewTransformConfig &ViewTransform::config() const {
    return mConfig;
}

bool ViewTransform::applyConfig(const ViewTransformConfig &newConfig, QPointF pointer) {
    const bool defaultFitModeChanged = mConfig.defaultFitMode != newConfig.defaultFitMode;
    const bool expandImageChanged = mConfig.expandImage != newConfig.expandImage;
    const float prevExpandLimit = effectiveExpandLimit();
    mConfig = newConfig;
    const bool fitScaleSettingsChanged = defaultFitModeChanged || expandImageChanged ||
                                         effectiveExpandLimit() != prevExpandLimit;
    updateScaleLimits();
    if (!hasImage())
        return fitScaleSettingsChanged;

    if (mFitMode == FIT_FREE) {
        if (mScale < mMinScale) {
            zoom(mMinScale);
            centerIfNecessary();
            snapToEdges();
        }
    } else if (fitScaleSettingsChanged) {
        if (defaultFitModeChanged)
            mFitMode = mConfig.defaultFitMode;
        applyFitMode(pointer);
    }
    return fitScaleSettingsChanged;
}

void ViewTransform::setExpandImage(bool enabled, QPointF pointer) {
    mConfig.expandImage = enabled;
    updateScaleLimits();
    applyFitMode(pointer);
}

void ViewTransform::setExpandSmallImagesInFitMode(bool enabled) {
    mExpandSmallImagesInFitMode = enabled;
}

float ViewTransform::effectiveExpandLimit() const {
    return mConfig.expandLimit < 1.0f ? kMaxScale : mConfig.expandLimit;
}

void ViewTransform::setViewportSize(QSize size) {
    mViewportSize = size;
}

QSize ViewTransform::viewportSize() const {
    return mViewportSize;
}

QPointF ViewTransform::viewportCenter() const {
    return QRect(QPoint(0, 0), mViewportSize).center();
}

void ViewTransform::setDevicePixelRatio(qreal dpr) {
    mDpr = dpr;
}

qreal ViewTransform::devicePixelRatio() const {
    return mDpr;
}

void ViewTransform::changeDevicePixelRatio(qreal dpr, QPointF pointer) {
    mDpr = dpr;
    if (!hasImage())
        return;
    updateScaleLimits();
    applyFitMode(pointer);
}

void ViewTransform::refitToViewport(QPointF pointer) {
    updateScaleLimits();
    if (mFitMode == FIT_FREE || mFitMode == FIT_ORIGINAL) {
        centerIfNecessary();
        snapToEdges();
    } else {
        applyFitMode(pointer);
    }
    saveViewportPosition();
}

// --- image lifecycle ----------------------------------------------------------

bool ViewTransform::hasImage() const {
    return !mImageSize.isEmpty();
}

QSize ViewTransform::imageSize() const {
    return mImageSize;
}

void ViewTransform::clear() {
    mImageSize = QSize();
    mScale = 1.0f;
    centerOnImagePoint(QPointF(0, 0));
}

void ViewTransform::showImage(QSize newImageSize, QPointF pointer) {
    mImageSize = newImageSize;
    updateScaleLimits();
    presentImage(pointer);
}

PreservedView ViewTransform::preservedView() const {
    return PreservedView{mScale, mFitMode, relativeViewportCenter()};
}

void ViewTransform::showTransformedImage(QSize newImageSize, const PreservedView &view,
                                         QPointF pointer) {
    mImageSize = newImageSize;
    updateScaleLimits();
    mFitMode = view.fitMode;
    if (mFitMode == FIT_FREE)
        zoom(view.scale);
    else
        applyFitMode(pointer);

    const QSizeF size = scaledSizeF();
    centerOnImagePoint(QPointF(size.width() * view.relativeCenter.x(),
                               size.height() * view.relativeCenter.y()));
    centerIfNecessary();
    snapToEdges();
}

void ViewTransform::presentImage(QPointF pointer) {
    if (!mConfig.keepFitMode || mFitMode == FIT_FREE)
        mFitMode = mConfig.defaultFitMode;

    if (mLock == ViewLock::None) {
        applyFitMode(pointer);
        return;
    }
    mFitMode = FIT_FREE;
    fitFree(mLockedScale, pointer);
    if (mLock == ViewLock::All)
        applySavedViewportPosition();
}

// --- queries --------------------------------------------------------------------

float ViewTransform::scale() const {
    return mScale;
}

QPointF ViewTransform::imagePosition() const {
    return mImagePos;
}

ImageFitMode ViewTransform::fitMode() const {
    return mFitMode;
}

float ViewTransform::minScale() const {
    return mMinScale;
}

float ViewTransform::fitWindowScale() const {
    return mFitWindowScale;
}

float ViewTransform::fitWidthScale() const {
    return mFitWidthScale;
}

float ViewTransform::fitHeightScale() const {
    return mFitHeightScale;
}

QSizeF ViewTransform::scaledSizeF() const {
    if (!hasImage())
        return QSizeF(0, 0);
    return QSizeF(mImageSize.width() / mDpr * mScale,
                  mImageSize.height() / mDpr * mScale);
}

QSize ViewTransform::scaledSize() const {
    return scaledSizeF().toSize();
}

QRect ViewTransform::scaledRect() const {
    const QSizeF size = scaledSizeF();
    const QPointF bottomRight = mImagePos + QPointF(size.width(), size.height());
    return QRect(mImagePos.toPoint(), bottomRight.toPoint());
}

bool ViewTransform::imageFits() const {
    if (!hasImage())
        return true;
    return mImageSize.width() <= mViewportSize.width() * mDpr &&
           mImageSize.height() <= mViewportSize.height() * mDpr;
}

bool ViewTransform::scaledImageFits() const {
    if (!hasImage())
        return true;
    const QSize size = scaledSize();
    return size.width() <= mViewportSize.width() &&
           size.height() <= mViewportSize.height();
}

QPointF ViewTransform::relativeViewportCenter() const {
    if (!hasImage())
        return QPointF(kCenteredRelativePos, kCenteredRelativePos);
    // Same probe point the widget viewer used: whole-pixel centre plus one.
    const QPointF center = viewportCenter() + QPointF(1, 1);
    const QSizeF size = scaledSizeF();
    return QPointF(qBound(qreal(0), (center.x() - mImagePos.x()) / size.width(), qreal(1)),
                   qBound(qreal(0), (center.y() - mImagePos.y()) / size.height(), qreal(1)));
}

// --- fit modes ------------------------------------------------------------------

void ViewTransform::setFitMode(ImageFitMode mode, QPointF pointer) {
    mFitMode = mode;
    applyFitMode(pointer);
}

void ViewTransform::forceFitMode(ImageFitMode mode, QPointF pointer) {
    mFitMode = mode;
    switch (mode) {
    case FIT_WINDOW:
        fitWindow(true);
        break;
    case FIT_WIDTH:
        fitWidth(true, pointer);
        break;
    case FIT_HEIGHT:
        fitHeight(true, pointer);
        break;
    default:
        applyFitMode(pointer);
        break;
    }
}

void ViewTransform::applyFitMode(QPointF pointer) {
    switch (mFitMode) {
    case FIT_ORIGINAL:
        fitFree(1.0f, pointer);
        break;
    case FIT_WIDTH:
        fitWidth(false, pointer);
        break;
    case FIT_WINDOW:
        // Auto-applied fit-to-window does not upscale small images unless
        // expansion is enabled or temporarily requested.
        if (imageFits() && !mConfig.expandImage && !mExpandSmallImagesInFitMode)
            fitFree(1.0f, pointer);
        else
            fitWindow(false);
        break;
    case FIT_HEIGHT:
        fitHeight(false, pointer);
        break;
    default:
        break;
    }
}

ImageFitMode ViewTransform::fitModeForScale(float value) const {
    if (std::abs(value - mFitWindowScale) < kScaleEpsilon)
        return FIT_WINDOW;
    if (std::abs(value - mFitWidthScale) < kScaleEpsilon)
        return FIT_WIDTH;
    if (std::abs(value - mFitHeightScale) < kScaleEpsilon)
        return FIT_HEIGHT;
    return FIT_FREE;
}

void ViewTransform::fitWindow(bool force) {
    if (!hasImage())
        return;
    const float targetScale = force ? windowFitScale() : mFitWindowScale;
    if (mScale != targetScale)
        zoom(targetScale);
    centerImage();
}

void ViewTransform::fitWidth(bool force, QPointF pointer) {
    if (!hasImage())
        return;
    // Capture the anchor in scale-invariant image coordinates before the
    // scale changes.
    if (mConfig.focusPoint == FOCUS_CURSOR)
        setZoomAnchor(pointer);
    updateFitWidthScale();
    const float targetScale = force ? widthFitScale() : mFitWidthScale;
    if (mScale != targetScale)
        zoom(targetScale);
    centerIfNecessary();
    if (scaledSize().height() > mViewportSize.height())
        focusOverflowingAxis(Qt::Vertical);
    snapToEdges();
}

void ViewTransform::fitHeight(bool force, QPointF pointer) {
    if (!hasImage())
        return;
    // See fitWidth() for why the anchor is captured first.
    if (mConfig.focusPoint == FOCUS_CURSOR)
        setZoomAnchor(pointer);
    updateFitHeightScale();
    const float targetScale = force ? heightFitScale() : mFitHeightScale;
    if (mScale != targetScale)
        zoom(targetScale);
    centerIfNecessary();
    if (scaledSize().width() > mViewportSize.width())
        focusOverflowingAxis(Qt::Horizontal);
    snapToEdges();
}

// Positions the image along the axis that does not fit after a fit-width or
// fit-height, according to the focus point setting.
void ViewTransform::focusOverflowingAxis(Qt::Orientation axis) {
    switch (mConfig.focusPoint) {
    case FOCUS_TOP:
        if (axis == Qt::Vertical)
            mImagePos.setY(0);
        else
            mImagePos.setX(0);
        break;
    case FOCUS_CURSOR:
        centerOnImagePoint(anchorImageOffset());
        break;
    case FOCUS_CENTER: {
        const QSizeF size = scaledSizeF();
        centerOnImagePoint(QPointF(size.width() / 2, size.height() / 2));
        break;
    }
    }
}

void ViewTransform::fitFree(float newScale, QPointF pointer) {
    if (!hasImage())
        return;
    if (mConfig.focusPoint == FOCUS_TOP) {
        zoom(newScale);
        centerIfNecessary();
        if (scaledSize().height() > mViewportSize.height()) {
            mImagePos.setX(centeredCoordinate(mViewportSize.width(), scaledSizeF().width() / 2));
            mImagePos.setY(0);
        }
        snapToEdges();
        return;
    }
    setZoomAnchor(mConfig.focusPoint == FOCUS_CENTER ? viewportCenter() : pointer);
    zoomAnchored(newScale);
    centerIfNecessary();
    snapToEdges();
}

void ViewTransform::updateScaleLimits() {
    if (!hasImage())
        return;
    updateFitWindowScale();
    updateFitWidthScale();
    updateFitHeightScale();
    if (mConfig.unlockMinZoom) {
        mMinScale = static_cast<float>(qMax(kUnlockedMinZoomSidePx / mImageSize.width(),
                                            kUnlockedMinZoomSidePx / mImageSize.height()));
    } else {
        mMinScale = imageFits() ? 1.0f : mFitWindowScale;
    }
    if (mLock != ViewLock::None && mLockedScale < mMinScale)
        mMinScale = mLockedScale;
}

void ViewTransform::updateFitWindowScale() {
    mFitWindowScale = limitExpansion(windowFitScale());
}

void ViewTransform::updateFitWidthScale() {
    if (!hasImage())
        return;
    mFitWidthScale = limitExpansion(widthFitScale());
}

void ViewTransform::updateFitHeightScale() {
    if (!hasImage())
        return;
    mFitHeightScale = limitExpansion(heightFitScale());
}

float ViewTransform::limitExpansion(float fitScale) const {
    const float limit = effectiveExpandLimit();
    return (mConfig.expandImage && fitScale > limit) ? limit : fitScale;
}

float ViewTransform::windowFitScale() const {
    return qMin(widthFitScale(), heightFitScale());
}

float ViewTransform::widthFitScale() const {
    return static_cast<float>(mViewportSize.width()) * static_cast<float>(mDpr) /
           static_cast<float>(mImageSize.width());
}

float ViewTransform::heightFitScale() const {
    return static_cast<float>(mViewportSize.height()) * static_cast<float>(mDpr) /
           static_cast<float>(mImageSize.height());
}

// --- zoom -------------------------------------------------------------------------

float ViewTransform::zoomInScale(float baseScale) const {
    const float stepFactor = 1.0f + mConfig.zoomStep;
    float newScale = baseScale * stepFactor;
    const QList<float> &levels = mConfig.zoomLevels;
    if (mConfig.useFixedZoomLevels && !levels.isEmpty()) {
        if (baseScale < levels.first()) {
            newScale = qMin(baseScale * stepFactor, levels.first());
        } else if (baseScale < levels.last()) {
            const auto next = std::upper_bound(levels.cbegin(), levels.cend(), baseScale);
            newScale = *next;
        }
    }
    return qBound(mMinScale, newScale, kMaxScale);
}

float ViewTransform::zoomOutScale(float baseScale) const {
    const float stepFactor = 1.0f + mConfig.zoomStep;
    float newScale = baseScale / stepFactor;
    const QList<float> &levels = mConfig.zoomLevels;
    if (mConfig.useFixedZoomLevels && !levels.isEmpty()) {
        if (baseScale > levels.last()) {
            newScale = qMax(levels.last(), baseScale / stepFactor);
        } else if (baseScale > levels.first()) {
            const auto next = std::lower_bound(levels.cbegin(), levels.cend(), baseScale);
            newScale = *std::prev(next);
        }
    }
    return qBound(mMinScale, newScale, kMaxScale);
}

float ViewTransform::gestureZoomScale(int moveDistance) const {
    return mScale * (1.0f + kGestureZoomStepPerPixel * static_cast<float>(moveDistance) *
                                static_cast<float>(mDpr));
}

void ViewTransform::setZoomAnchor(QPointF viewportPos) {
    mZoomAnchorViewport = viewportPos;
    mZoomAnchorImage = (viewportPos - mImagePos) / mScale;
}

QPointF ViewTransform::anchorImageOffset() const {
    return mZoomAnchorImage * mScale;
}

void ViewTransform::zoomTo(float newScale) {
    zoomAnchored(newScale);
    centerIfNecessary();
    snapToEdges();
    mFitMode = fitModeForScale(mScale);
}

void ViewTransform::zoom(float newScale) {
    if (!hasImage())
        return;
    // The image's top-left stays in place; callers reposition afterwards.
    mScale = qBound(mMinScale, newScale, kMaxScale);
}

void ViewTransform::zoomAnchored(float newScale) {
    if (mScale == newScale)
        return;
    zoom(newScale);
    const QPointF anchorTarget = mZoomAnchorViewport - anchorImageOffset();
    mImagePos = QPointF(qRound(anchorTarget.x()), qRound(anchorTarget.y()));
    mEvents.anchoredZoom = true;
}

// --- positioning ----------------------------------------------------------------

void ViewTransform::centerImage() {
    if (!hasImage())
        return;
    const QSizeF size = scaledSizeF();
    mImagePos = QPointF(centeredEdge(mViewportSize.width(), size.width()),
                        centeredEdge(mViewportSize.height(), size.height()));
    mEvents.imageCentered = true;
}

void ViewTransform::centerIfNecessary() {
    if (!hasImage())
        return;
    const QSize size = scaledSize();
    const QSizeF sizeF = scaledSizeF();
    if (size.width() <= mViewportSize.width())
        mImagePos.setX(centeredEdge(mViewportSize.width(), sizeF.width()));
    if (size.height() <= mViewportSize.height())
        mImagePos.setY(centeredEdge(mViewportSize.height(), sizeF.height()));
}

// Moves an image larger than the viewport so that no gap is left between its
// edges and the viewport edges.
void ViewTransform::snapToEdges() {
    if (!hasImage())
        return;
    const QRect imageRect = scaledRect();
    const int width = mViewportSize.width();
    const int height = mViewportSize.height();
    QPointF shift;
    if (imageRect.width() > width) {
        if (imageRect.left() > 0)
            shift.setX(imageRect.left());
        else if (imageRect.right() < width)
            shift.setX(imageRect.right() - width);
    }
    if (imageRect.height() > height) {
        if (imageRect.top() > 0)
            shift.setY(imageRect.top());
        else if (imageRect.bottom() < height)
            shift.setY(imageRect.bottom() - height);
    }
    mImagePos -= shift;
}

void ViewTransform::centerOnImagePoint(QPointF imageOffset) {
    mImagePos = QPointF(centeredCoordinate(mViewportSize.width(), imageOffset.x()),
                        centeredCoordinate(mViewportSize.height(), imageOffset.y()));
}

// --- scrolling --------------------------------------------------------------------

QPoint ViewTransform::scrollPosition() const {
    return -mImagePos.toPoint();
}

void ViewTransform::scrollBy(QPointF scrollDelta) {
    mImagePos = QPointF(std::ceil(mImagePos.x() - scrollDelta.x()),
                        std::ceil(mImagePos.y() - scrollDelta.y()));
    centerIfNecessary();
    snapToEdges();
}

void ViewTransform::scrollTo(Qt::Orientation axis, int scrollValue) {
    if (axis == Qt::Horizontal)
        mImagePos.setX(-scrollValue);
    else
        mImagePos.setY(-scrollValue);
    centerIfNecessary();
    snapToEdges();
}

// --- locks --------------------------------------------------------------------------

ViewLock ViewTransform::lock() const {
    return mLock;
}

void ViewTransform::toggleLockZoom() {
    if (!hasImage())
        return;
    if (mLock != ViewLock::Zoom) {
        mLock = ViewLock::Zoom;
        lockZoom();
    } else {
        mLock = ViewLock::None;
    }
}

void ViewTransform::toggleLockView() {
    if (!hasImage())
        return;
    if (mLock != ViewLock::All) {
        mLock = ViewLock::All;
        lockZoom();
    } else {
        mLock = ViewLock::None;
    }
}

void ViewTransform::lockZoom() {
    mLockedScale = mScale;
    mFitMode = FIT_FREE;
    saveViewportPosition();
}

void ViewTransform::saveViewportPosition() {
    if (mLock != ViewLock::All || !hasImage())
        return;
    mSavedViewportPos = relativeViewportCenter();
}

void ViewTransform::applySavedViewportPosition() {
    const QSizeF size = scaledSizeF();
    centerOnImagePoint(QPointF(size.width() * mSavedViewportPos.x(),
                               size.height() * mSavedViewportPos.y()));
    centerIfNecessary();
    snapToEdges();
}

ViewTransformEvents ViewTransform::takeEvents() {
    return std::exchange(mEvents, ViewTransformEvents{});
}

// --- PanoramaView ---------------------------------------------------------------------

float PanoramaView::yaw() const {
    return mYaw;
}

float PanoramaView::pitch() const {
    return mPitch;
}

float PanoramaView::fov() const {
    return mFov;
}

void PanoramaView::drag(QPoint delta, int viewportWidth) {
    if (viewportWidth <= 0)
        return;
    const float degreesPerPixel = mFov / static_cast<float>(viewportWidth) * kPanoramaDragSpeed;
    mYaw -= static_cast<float>(delta.x()) * degreesPerPixel;
    mPitch -= static_cast<float>(delta.y()) * degreesPerPixel;
    mPitch = qBound(-kMaxPitch, mPitch, kMaxPitch);
}

void PanoramaView::zoomByWheel(int angleDelta) {
    mFov *= (angleDelta > 0) ? kPanoramaWheelZoomInFactor : kPanoramaWheelZoomOutFactor;
    clampFov();
}

void PanoramaView::zoomByGesture(int moveDistance, qreal dpr) {
    // Moving up zooms in, which narrows the field of view.
    mFov *= (1.0f - kGestureZoomStepPerPixel * static_cast<float>(moveDistance) *
                        static_cast<float>(dpr));
    clampFov();
}

void PanoramaView::reset() {
    mYaw = 0.0f;
    mPitch = 0.0f;
    mFov = kDefaultFov;
}

void PanoramaView::clampFov() {
    mFov = qBound(kMinFov, mFov, kMaxFov);
}
