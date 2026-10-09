#include "imageviewportcontroller.h"

#include <QDebug>
#include <QEasingCurve>
#include <QEvent>
#include <QGuiApplication>
#include <QQuickItemGrabResult>
#include <QQuickWindow>
#include <QStyleHints>
#include <QAccessibilityHints>

#include <cmath>
#include <utility>

#include "gui/quick/render/imagefiltermode.h"

namespace {
// Delay before the settle pass after the view stops moving (the widget
// viewer's high-quality rescale delay).
constexpr int kSettleDelayMs = 80;

// Settings::hdrToneMappingOperator() -> renderer operator; the values are
// the same (see imagerenderitem.cpp). Unknown values select BT.2408, as
// the settings dialog's default.
RenderEnums::ToneMapOperator toneMapOperatorFor(int settingsValue) {
    switch (settingsValue) {
    case static_cast<int>(RenderEnums::ToneMapOperator::Bt2408):
    case static_cast<int>(RenderEnums::ToneMapOperator::ReinhardJodie):
    case static_cast<int>(RenderEnums::ToneMapOperator::AcesFilmic):
    case static_cast<int>(RenderEnums::ToneMapOperator::Hable):
        return static_cast<RenderEnums::ToneMapOperator>(settingsValue);
    default:
        qWarning() << "ImageViewportController: unknown tone mapping operator"
                   << settingsValue << "- using BT.2408";
        return RenderEnums::ToneMapOperator::Bt2408;
    }
}
// Smooth zoom and scroll duration (ImageViewerV2::ANIMATION_SPEED).
constexpr int kMotionDurationMs = 150;
// Keyboard scroll step, logical pixels.
constexpr int kScrollStepPx = 240;
// The pointer is hidden this long after it stopped moving.
constexpr int kCursorHideTimeoutMs = 1000;
// The zoom indicator in auto mode stays this long after a zoom change.
constexpr int kZoomIndicatorAutoHideMs = 2000;
constexpr qreal kPercent = 100.0;
constexpr qreal kMinimumDevicePixelRatio = 1.0;
constexpr float kOneToOneScale = 1.0f;
constexpr qreal kAnimationStart = 0.0;
constexpr qreal kAnimationEnd = 1.0;

// Quintic smootherstep, the widget viewer's smooth zoom easing.
qreal smootherstep(qreal t) {
    constexpr qreal kQuinticA = 6.0;
    constexpr qreal kQuinticB = 15.0;
    constexpr qreal kQuinticC = 10.0;
    return t * t * t * (t * (t * kQuinticA - kQuinticB) + kQuinticC);
}

ViewTransformConfig viewTransformConfigFrom(const ViewerSettings &viewer) {
    ViewTransformConfig config;
    config.expandImage = viewer.expandImage;
    config.expandLimit = static_cast<float>(viewer.expandLimit);
    config.keepFitMode = viewer.keepFitMode;
    config.defaultFitMode = static_cast<ImageFitMode>(viewer.fitMode);
    config.zoomStep = static_cast<float>(viewer.zoomStep);
    config.focusPoint = static_cast<ImageFocusPoint>(viewer.focusPointIn1to1Mode);
    config.useFixedZoomLevels = viewer.useFixedZoomLevels;
    if (config.useFixedZoomLevels)
        config.zoomLevels = parseZoomLevels(viewer.zoomLevels);
    config.unlockMinZoom = viewer.unlockMinZoom;
    return config;
}

bool panelOccupiesSide(const PanelSettings &panel) {
    return panel.enabled && (panel.position == SettingsEnums::PanelPosition::Left ||
                             panel.position == SettingsEnums::PanelPosition::Right);
}

const QAccessibilityHints *accessibilityHints() {
    const QStyleHints *styleHints = QGuiApplication::styleHints();
    return styleHints ? styleHints->accessibility() : nullptr;
}
} // namespace

//------------------------------------------------------------------------------
ImageViewportController::ImageViewportController(const UiSettingsSnapshot &settings,
                                                 QObject *parent)
    : QObject(parent),
      mTransform(static_cast<const IViewSurface &>(*this)) {
    mClock.start();
    mPlayer.setLoop(true);

    mScaleTimer.setSingleShot(true);
    mScaleTimer.setInterval(kSettleDelayMs);
    connect(&mScaleTimer, &QTimer::timeout, this, &ImageViewportController::requestScaling);

    mCursorTimer.setSingleShot(true);
    mCursorTimer.setInterval(kCursorHideTimeoutMs);
    connect(&mCursorTimer, &QTimer::timeout, this, &ImageViewportController::hideCursor);

    mZoomIndicatorTimer.setSingleShot(true);
    mZoomIndicatorTimer.setInterval(kZoomIndicatorAutoHideMs);
    connect(&mZoomIndicatorTimer, &QTimer::timeout, this,
            [this]() { setZoomIndicatorVisible(false); });

    QEasingCurve zoomCurve;
    zoomCurve.setCustomType(smootherstep);
    mZoomAnimation.setDuration(kMotionDurationMs);
    mZoomAnimation.setStartValue(kAnimationStart);
    mZoomAnimation.setEndValue(kAnimationEnd);
    mZoomAnimation.setEasingCurve(zoomCurve);
    connect(&mZoomAnimation, &QVariantAnimation::valueChanged, this,
            &ImageViewportController::onZoomAnimationValue);
    connect(&mZoomAnimation, &QVariantAnimation::finished, this,
            &ImageViewportController::onZoomAnimationFinished);

    for (auto [animation, axis] : {std::pair{&mScrollAnimationX, Qt::Horizontal},
                                   std::pair{&mScrollAnimationY, Qt::Vertical}}) {
        animation->setDuration(kMotionDurationMs);
        animation->setEasingCurve(QEasingCurve::OutSine);
        connect(animation, &QVariantAnimation::valueChanged, this,
                [this, animation, axis](const QVariant &value) {
                    // Setting the start and end values of a stopped
                    // animation also reports a value.
                    if (animation->state() == QAbstractAnimation::Running)
                        mTransform.scrollTo(axis, value.toInt());
                });
        connect(animation, &QVariantAnimation::finished, this,
                [this]() { mTransform.saveViewportPosition(); });
    }

    connect(&mTransform, &ViewTransformController::transformChanged, this,
            &ImageViewportController::onTransformChanged);
    connect(&mTransform, &ViewTransformController::scaleChanged, this,
            &ImageViewportController::onScaleChanged);
    connect(&mTransform, &ViewTransformController::positionChanged, this,
            &ImageViewportController::onViewPositionChanged);
    connect(&mTransform, &ViewTransformController::imageCentered, this,
            [this]() { emit imageAreaChanged(mTransform.transform().scaledRect()); });
    connect(&mTransform, &ViewTransformController::anchoredZoomApplied, this,
            &ImageViewportController::requestScaling);
    connect(&mTransform, &ViewTransformController::panoramaChanged, this,
            &ImageViewportController::onPanoramaChanged);

    connect(&mPlayer, &AnimationPlayer::frameReady, this,
            [this](std::shared_ptr<const QImage> frame, int) { onAnimationFrame(std::move(frame)); });
    connect(&mPlayer, &AnimationPlayer::playingChanged, this,
            &ImageViewportController::applyItemSettled);
    connect(&mPlayer, &AnimationPlayer::playbackError, this,
            &ImageViewportController::playbackError);

    connect(&mPresentation, &FramePresentationTracker::presented, this,
            &ImageViewportController::onFramePresented);

    if (const QAccessibilityHints *hints = accessibilityHints()) {
        connect(hints, &QAccessibilityHints::motionPreferenceChanged, this,
                &ImageViewportController::reducedMotionChanged);
    } else {
        qWarning() << "ImageViewportController: no style hints; reduced motion is not followed";
    }

    applySettings(settings);
}

ImageViewportController::~ImageViewportController() {
    // The window may outlive the controller in tests; its render thread must
    // not call into a destroyed tracker.
    mPresentation.setWindow(nullptr);
    if (mWindow)
        mWindow->removeEventFilter(this);
}

//------------------------------------------------------------------------------
// View and window
//------------------------------------------------------------------------------
ImageRenderItem *ImageViewportController::view() const {
    return mView;
}

void ImageViewportController::setView(ImageRenderItem *view) {
    if (mView == view)
        return;
    if (mView)
        disconnect(mView, nullptr, this, nullptr);
    mView = view;
    if (mView) {
        connect(mView, &QQuickItem::widthChanged, this, &ImageViewportController::onViewportResized);
        connect(mView, &QQuickItem::heightChanged, this, &ImageViewportController::onViewportResized);
        connect(mView, &QQuickItem::windowChanged, this, &ImageViewportController::attachWindow);
        attachWindow(mView->window());
    } else {
        attachWindow(nullptr);
    }
    mTransform.syncViewport();
    updateClickZonesEnabled();
    pushDisplayState();
    emit viewChanged();
}

void ImageViewportController::attachWindow(QQuickWindow *window) {
    if (mWindow == window)
        return;
    if (mWindow)
        mWindow->removeEventFilter(this);
    mWindow = window;
    mPresentation.setWindow(window);
    if (mWindow) {
        mWindow->installEventFilter(this);
        syncDevicePixelRatio();
    }
}

bool ImageViewportController::eventFilter(QObject *watched, QEvent *event) {
    if (watched == mWindow && event->type() == QEvent::DevicePixelRatioChange)
        onDevicePixelRatioChanged();
    return QObject::eventFilter(watched, event);
}

// Pushes the complete display state into the view, which may be new.
void ImageViewportController::pushDisplayState() {
    if (!mView)
        return;
    mView->setImage(mImage);
    mView->setPlacement({.position = mTransform.transform().imagePosition(),
                         .scale = mTransform.transform().scale()});
    mView->setTransparencyGrid(mTransparencyGrid);
    mView->setColorAdjustments(mColorAdjustments);
    mView->setProjection(mPanorama ? RenderEnums::Projection::Equirectangular
                                   : RenderEnums::Projection::Flat);
    onPanoramaChanged();
    applyFilter();
    applyDisplayColor();
    applyItemSettled();
}

// HDR tone mapping and display colour management of the decoded image (the
// Quick UI uses DisplayPipeline::Gpu).
void ImageViewportController::applyDisplayColor() {
    if (!mView)
        return;
    const DisplayColorSettings &color = mSettings.displayColor;
    mView->setToneMapping({.enabled = color.toneMapping,
                           .op = toneMapOperatorFor(color.toneMapOperator),
                           .whiteNits = static_cast<float>(color.hdrWhiteLevel)});
    mView->setColorManagement({.enabled = color.colorManagement, .target = color.target});
}

void ImageViewportController::applyFilter() {
    if (!mView)
        return;
    const ImageFilterMode mode = imageFilterModeFor(mScalingFilter);
    mView->setSampling(mode.sampling);
    mView->setSharpening(mode.sharpening);
    mView->setResampling(mode.resampling);
    mView->setCasSharpening(mSettings.viewer.casSharpening);
    mView->setCasContrast(mSettings.viewer.casContrast);
}

void ImageViewportController::setCasParameters(float sharpening, float contrast) {
    mSettings.viewer.casSharpening = sharpening;
    mSettings.viewer.casContrast = contrast;
    applyFilter();
}

QSize ImageViewportController::viewportSize() const {
    if (!mView)
        return {};
    return QSizeF(mView->width(), mView->height()).toSize();
}

QPointF ImageViewportController::pointerPosition() const {
    return mPointerPosition;
}

void ImageViewportController::onViewportResized() {
    updateClickZonesEnabled();
    // A press position from before the resize must not start a drag-out.
    mInteraction.resetPressPosition(mPointerPosition.toPoint());
    if (mWindow && mWindow->isVisible()) {
        setRenderingSettled(false);
        stopPosAnimation();
        mTransform.refitToViewport();
        mScaleTimer.start();
    } else {
        mTransform.syncViewport();
    }
}

qreal ImageViewportController::windowDevicePixelRatio() const {
    return mWindow ? qMax(mWindow->effectiveDevicePixelRatio(), kMinimumDevicePixelRatio)
                   : kMinimumDevicePixelRatio;
}

void ImageViewportController::syncDevicePixelRatio() {
    const qreal dpr = windowDevicePixelRatio();
    if (mTransform.transform().devicePixelRatio() == dpr)
        return;
    mTransform.setDevicePixelRatio(dpr);
    mInteraction.setThresholds(InteractionThresholds::forDevicePixelRatio(dpr));
}

void ImageViewportController::onDevicePixelRatioChanged() {
    const qreal dpr = windowDevicePixelRatio();
    if (mTransform.transform().devicePixelRatio() == dpr)
        return;
    mInteraction.setThresholds(InteractionThresholds::forDevicePixelRatio(dpr));
    if (!mImage) {
        mTransform.setDevicePixelRatio(dpr);
        return;
    }
    mTransform.changeDevicePixelRatio(dpr);
    requestScaling();
}

//------------------------------------------------------------------------------
// Settings
//------------------------------------------------------------------------------
void ImageViewportController::applySettings(const UiSettingsSnapshot &settings) {
    // The first application takes effect even for default values.
    const bool first = !mSettingsApplied;
    mSettingsApplied = true;
    const bool viewerChanged = first || settings.viewer != mSettings.viewer;
    const bool panelChanged = first || settings.panel != mSettings.panel;
    const bool overlaysChanged = first || settings.overlays != mSettings.overlays;
    const bool displayColorChanged = first || settings.displayColor != mSettings.displayColor;
    if (!viewerChanged && !panelChanged && !overlaysChanged && !displayColorChanged)
        return;
    const bool useUpscaylBefore = mSettings.viewer.useUpscayl;
    const ScalingFilter filterBefore = mScalingFilter;
    mSettings = settings;

    // The renderer converts again from the kept source; no re-decode.
    if (displayColorChanged)
        applyDisplayColor();
    if (panelChanged || viewerChanged)
        updateClickZonesEnabled();
    if (overlaysChanged)
        updateZoomIndicator();
    if (!viewerChanged)
        return;

    mTransparencyGrid = settings.viewer.transparencyGrid;
    if (mView)
        mView->setTransparencyGrid(mTransparencyGrid);
    mScalingFilter = configuredScalingFilter();
    applyFilter();
    if (!mSettings.viewer.cursorAutohide)
        showCursor();
    emit clickZonesChanged();

    // Re-fits the displayed image when a fit-relevant setting changed.
    const bool fitScaleChanged = mTransform.applyConfig(viewTransformConfigFrom(settings.viewer));
    const bool scalingChanged = fitScaleChanged || mScalingFilter != filterBefore ||
                                mSettings.viewer.useUpscayl != useUpscaylBefore;
    if (mImage) {
        if (scalingChanged)
            requestScaling();
    } else {
        setFitMode(static_cast<ImageFitMode>(settings.viewer.fitMode));
    }
}

//------------------------------------------------------------------------------
// Document
//------------------------------------------------------------------------------
void ImageViewportController::showImage(std::shared_ptr<const QImage> image,
                                        const QString &filePath) {
    if (!image || image->isNull()) {
        qWarning() << "ImageViewportController::showImage: null image for" << filePath;
        return;
    }
    syncDevicePixelRatio();
    const QSize oldSize = mImage ? mImage->size() : QSize();
    const QSize newSize = image->size();
    const bool sameFile = !filePath.isEmpty() && filePath == mFilePath;
    const bool rotationOrMirror =
        sameFile && oldSize.isValid() &&
        qint64(oldSize.width()) * oldSize.height() == qint64(newSize.width()) * newSize.height();
    // Scale or fit mode and relative viewport centre of the current image,
    // kept across a rotation or mirror.
    const PreservedView preserved = mTransform.preservedView();

    reset();
    mImage = std::move(image);
    mFilePath = filePath;
    if (mView)
        mView->setImage(mImage, ImageRenderItem::ImageUpdate::NewImage);
    if (rotationOrMirror)
        mTransform.showTransformedImage(newSize, preserved);
    else
        mTransform.showImage(newSize);
    requestScaling();
    updateZoomIndicator();
    hideCursorTimed(false);
    emit imageChanged();
}

void ImageViewportController::showAnimation(const QString &filePath, const QString &format) {
    reset();
    syncDevicePixelRatio();
    mFilePath = filePath;
    // open() publishes the first frame synchronously (onAnimationFrame()).
    mAwaitingFirstFrame = true;
    if (!mPlayer.open(filePath, format.toUtf8())) {
        mAwaitingFirstFrame = false;
        mFilePath.clear();
        return;
    }
    updateZoomIndicator();
    hideCursorTimed(false);
}

void ImageViewportController::onAnimationFrame(std::shared_ptr<const QImage> frame) {
    mImage = std::move(frame);
    if (!mAwaitingFirstFrame) {
        if (mView)
            mView->setImage(mImage, ImageRenderItem::ImageUpdate::AnimationFrame);
        return;
    }
    mAwaitingFirstFrame = false;
    if (mView)
        mView->setImage(mImage, ImageRenderItem::ImageUpdate::NewImage);
    mTransform.showImage(mImage->size());
    requestScaling();
    emit imageChanged();
}

void ImageViewportController::closeImage() {
    const bool hadImage = hasImage();
    reset();
    updateZoomIndicator();
    showCursor();
    if (hadImage)
        emit imageChanged();
}

// Drops the image and stops everything that belongs to it. The panorama
// mode is a viewer mode and survives, as in the widget viewer.
void ImageViewportController::reset() {
    setRenderingSettled(false);
    mPresentation.cancel();
    stopPosAnimation();
    mPlayer.close();
    mAwaitingFirstFrame = false;
    mImage.reset();
    mFilePath.clear();
    if (mView) {
        mView->clearUpscaledCrop();
        mView->setImage(nullptr);
    }
    mTransform.clear();
    applyItemSettled();
}

bool ImageViewportController::hasImage() const {
    return mImage != nullptr;
}

QRectF ImageViewportController::imageArea() const {
    if (!hasImage())
        return {};
    const ViewTransform &transform = mTransform.transform();
    return QRectF(transform.imagePosition(), transform.scaledSizeF());
}

QSize ImageViewportController::imageSize() const {
    return mImage ? mImage->size() : QSize();
}

void ImageViewportController::setUpscaledCrop(const QImage &crop, const QRect &sourceRect) {
    if (mPanorama || !mImage || !mView)
        return;
    if (crop.isNull() || sourceRect.isEmpty()) {
        qWarning() << "ImageViewportController::setUpscaledCrop: empty crop for" << sourceRect;
        return;
    }
    mView->setUpscaledCrop(std::make_shared<const QImage>(crop), sourceRect);
}

void ImageViewportController::hideUpscaledCrop() {
    if (mView)
        mView->clearUpscaledCrop();
}

void ImageViewportController::refreshScaling() {
    requestScaling();
}

QRect ImageViewportController::visibleOriginalImageRect() const {
    if (!mImage)
        return {};
    const ViewTransform &transform = mTransform.transform();
    const qreal scale = transform.scale();
    if (scale <= 0.0)
        return {};
    // Viewport (logical) -> source pixels: (p - imagePosition) * dpr / scale.
    const qreal toSource = transform.devicePixelRatio() / scale;
    const QPointF position = transform.imagePosition();
    const QRectF viewport(QPointF(0.0, 0.0), QSizeF(viewportSize()));
    const QRectF source((viewport.topLeft() - position) * toSource, viewport.size() * toSource);
    return source.toAlignedRect().intersected(mImage->rect());
}

float ImageViewportController::currentScale() const {
    return mTransform.transform().scale();
}

float ImageViewportController::devicePixelRatio() const {
    return static_cast<float>(mTransform.transform().devicePixelRatio());
}

bool ImageViewportController::isBusyInteracting() const {
    return mInteraction.isBusy() || mPinching;
}

bool ImageViewportController::isRenderingSettled() const {
    return mRenderingSettled;
}

bool ImageViewportController::panoramaMode() const {
    return mPanorama;
}

void ImageViewportController::grabVisibleImage() {
    const QRect viewport(QPoint(0, 0), viewportSize());
    // Every pixel the image covers, also partly at a fractional position.
    const QRectF imageArea =
        mImage ? QRectF(mTransform.transform().imagePosition(),
                        QSizeF(mImage->size()) * mTransform.transform().scale())
               : QRectF();
    const QRectF visible = mPanorama ? QRectF(viewport) : imageArea.intersected(QRectF(viewport));
    if (!mImage || !mView || !mView->window() || visible.isEmpty()) {
        emit visibleImageGrabFailed();
        return;
    }
    const qreal dpr = windowDevicePixelRatio();
    const QSize grabSize = (QSizeF(viewport.size()) * dpr).toSize();
    QSharedPointer<QQuickItemGrabResult> grab = mView->grabToImage(grabSize);
    if (!grab) {
        qWarning() << "ImageViewportController: the viewport could not be read back";
        emit visibleImageGrabFailed();
        return;
    }
    // Replacing a pending grab drops its connection with it.
    mPendingGrab = grab;
    mPendingGrabCrop = QRectF(visible.topLeft() * dpr, visible.size() * dpr)
                           .toAlignedRect()
                           .intersected(QRect(QPoint(0, 0), grabSize));
    connect(grab.data(), &QQuickItemGrabResult::ready, this, [this, dpr]() {
        const QSharedPointer<QQuickItemGrabResult> finished = std::exchange(mPendingGrab, {});
        const QImage grabbed = finished ? finished->image() : QImage();
        if (grabbed.isNull()) {
            qWarning() << "ImageViewportController: the viewport readback returned no image";
            emit visibleImageGrabFailed();
            return;
        }
        QImage visibleImage = grabbed.copy(mPendingGrabCrop);
        visibleImage.setDevicePixelRatio(dpr);
        emit visibleImageGrabbed(visibleImage);
    });
}

void ImageViewportController::setColorAdjustments(const ColorAdjustments &adjustments) {
    mColorAdjustments = adjustments;
    if (mView)
        mView->setColorAdjustments(adjustments);
}

//------------------------------------------------------------------------------
// Transform notifications
//------------------------------------------------------------------------------
void ImageViewportController::onTransformChanged() {
    const ViewTransform &transform = mTransform.transform();
    if (mView) {
        if (mView->imageScale() != transform.scale())
            hideUpscaledCrop();
        mView->setPlacement({.position = transform.imagePosition(), .scale = transform.scale()});
    }
    // The settle pass of the previous view no longer matches.
    setRenderingSettled(false);
    mPresentation.cancel();
    applyItemSettled();
    emit imageGeometryChanged();
}

void ImageViewportController::onScaleChanged(qreal scale) {
    updateZoomIndicator();
    emit scaleChanged(scale);
}

void ImageViewportController::onViewPositionChanged() {
    mScaleTimer.start();
    hideUpscaledCrop();
}

void ImageViewportController::onPanoramaChanged() {
    if (!mView)
        return;
    const PanoramaView &panorama = mTransform.panorama();
    mView->setPanoramaCamera(
        {.yaw = panorama.yaw(), .pitch = panorama.pitch(), .fov = panorama.fov()});
}

//------------------------------------------------------------------------------
// Scaling and settling
//------------------------------------------------------------------------------
// The GPU renderer shows every filter itself; Core is only asked about the
// AI upscale of the visible area (Core::onUpscaleRequested()).
void ImageViewportController::requestScaling() {
    mScaleTimer.stop();
    setRenderingSettled(false);
    mPresentation.cancel();
    if (!mImage) {
        presentSettledFrame();
        return;
    }
    // Zooming continues; its end requests scaling again.
    if (isZoomAnimating() || mPinching ||
        mInteraction.mode() == ViewportInteraction::Mode::Zoom ||
        mInteraction.mode() == ViewportInteraction::Mode::WheelZoom)
        return;
    if (wantsUpscale())
        emit upscaleRequested(mTransform.transform().scaledSize() * devicePixelRatio());
    else if (!mSettings.viewer.useUpscayl)
        hideUpscaledCrop();
    presentSettledFrame();
}

bool ImageViewportController::wantsUpscale() const {
    return mSettings.viewer.useUpscayl && !mPanorama && !mPlayer.isOpen() &&
           currentScale() > kOneToOneScale + ViewTransform::kScaleEpsilon;
}

void ImageViewportController::presentSettledFrame() {
    mPresentation.requestPresentation();
    applyItemSettled();
    if (mView)
        mView->update();
}

void ImageViewportController::onFramePresented() {
    setRenderingSettled(true);
    emit renderingSettled();
}

void ImageViewportController::setRenderingSettled(bool settled) {
    if (mRenderingSettled == settled)
        return;
    mRenderingSettled = settled;
    emit renderingSettledChanged();
}

// The item runs its settle pass while a settled frame is being presented or
// shown, except during animation playback, where every frame is a new image.
void ImageViewportController::applyItemSettled() {
    if (!mView)
        return;
    const bool settling = mPresentation.isPending() || mRenderingSettled;
    mView->setSettled(settling && !mPlayer.isPlaying());
}

//------------------------------------------------------------------------------
// Zoom
//------------------------------------------------------------------------------
void ImageViewportController::zoomIn() { zoomBy(true, false); }
void ImageViewportController::zoomOut() { zoomBy(false, false); }
void ImageViewportController::zoomInCursor() { zoomBy(true, true); }
void ImageViewportController::zoomOutCursor() { zoomBy(false, true); }

void ImageViewportController::zoomBy(bool zoomIn, bool atCursor) {
    if (!mInteractionEnabled)
        return;
    mTransform.setZoomAnchor(zoomAnchorPosition(atCursor));
    // Repeated zooms during a smooth zoom continue from its target.
    const float baseScale = isZoomAnimating() ? mZoomTargetScale : currentScale();
    const ViewTransform &transform = mTransform.transform();
    startZoom(zoomIn ? transform.zoomInScale(baseScale) : transform.zoomOutScale(baseScale));
}

QPointF ImageViewportController::zoomAnchorPosition(bool atCursor) const {
    if (atCursor && mPointerInside)
        return mPointerPosition;
    return mTransform.transform().viewportCenter();
}

void ImageViewportController::startZoom(float newScale) {
    if (!useSmoothMotion() || !mSettings.viewer.smoothZoom) {
        mTransform.zoomTo(newScale);
        return;
    }
    mZoomStartScale = currentScale();
    mZoomTargetScale = newScale;
    mZoomAnimation.stop();
    mZoomAnimation.start();
}

void ImageViewportController::onZoomAnimationValue(const QVariant &value) {
    if (mZoomAnimation.state() != QAbstractAnimation::Running)
        return;
    const qreal progress = value.toReal();
    const float scale = progress >= kAnimationEnd
                            ? mZoomTargetScale
                            : mZoomStartScale + (mZoomTargetScale - mZoomStartScale) *
                                                    static_cast<float>(progress);
    mTransform.zoomTo(scale);
}

void ImageViewportController::onZoomAnimationFinished() {
    mTransform.saveViewportPosition();
    requestScaling();
}

bool ImageViewportController::isZoomAnimating() const {
    return mZoomAnimation.state() == QAbstractAnimation::Running;
}

bool ImageViewportController::useSmoothMotion() const {
    return !reducedMotion();
}

bool ImageViewportController::reducedMotion() const {
    const QAccessibilityHints *hints = accessibilityHints();
    return hints && hints->motionPreference() == Qt::MotionPreference::ReducedMotion;
}

//------------------------------------------------------------------------------
// Fit modes and locks
//------------------------------------------------------------------------------
void ImageViewportController::setFitMode(ImageFitMode mode) {
    stopScaleTimerAndAnimations();
    mTransform.setFitMode(mode);
    requestScaling();
}

void ImageViewportController::forceFitMode(ImageFitMode mode) {
    stopScaleTimerAndAnimations();
    mTransform.forceFitMode(mode);
    requestScaling();
}

void ImageViewportController::fitWindow() {
    if (mInteractionEnabled)
        forceFitMode(FIT_WINDOW);
}

void ImageViewportController::fitWidth() {
    if (mInteractionEnabled)
        forceFitMode(FIT_WIDTH);
}

void ImageViewportController::fitHeight() {
    if (mInteractionEnabled)
        forceFitMode(FIT_HEIGHT);
}

void ImageViewportController::fitOriginal() {
    if (mInteractionEnabled)
        setFitMode(FIT_ORIGINAL);
}

void ImageViewportController::switchFitMode() {
    if (mInteractionEnabled)
        setFitMode(mTransform.transform().fitMode() == FIT_WINDOW ? FIT_ORIGINAL : FIT_WINDOW);
}

void ImageViewportController::setInteractionEnabled(bool enabled) {
    if (mInteractionEnabled == enabled)
        return;
    mInteractionEnabled = enabled;
    if (!enabled) {
        stopScaleTimerAndAnimations();
        mPinching = false;
    }
}

bool ImageViewportController::isInteractionEnabled() const {
    return mInteractionEnabled;
}

void ImageViewportController::setExpandSmallImagesInFitMode(bool enabled) {
    mTransform.setExpandSmallImagesInFitMode(enabled);
}

void ImageViewportController::toggleLockZoom() {
    mTransform.toggleLockZoom();
}

void ImageViewportController::toggleLockView() {
    mTransform.toggleLockView();
}

bool ImageViewportController::isZoomLocked() const {
    return mTransform.transform().lock() == ViewLock::Zoom;
}

bool ImageViewportController::isViewLocked() const {
    return mTransform.transform().lock() == ViewLock::All;
}

//------------------------------------------------------------------------------
// Display modes
//------------------------------------------------------------------------------
void ImageViewportController::toggleTransparencyGrid() {
    mTransparencyGrid = !mTransparencyGrid;
    if (mView)
        mView->setTransparencyGrid(mTransparencyGrid);
}

void ImageViewportController::setScalingFilter(ScalingFilter filter) {
    if (mScalingFilter == filter)
        return;
    mScalingFilter = filter;
    applyFilter();
    requestScaling();
}

ScalingFilter ImageViewportController::scalingFilter() const {
    return mScalingFilter;
}

ScalingFilter ImageViewportController::configuredScalingFilter() const {
    return static_cast<ScalingFilter>(mSettings.viewer.scalingFilter);
}

void ImageViewportController::togglePanorama() {
    if (!mImage)
        return;
    mPanorama = !mPanorama;
    if (mPanorama) {
        hideUpscaledCrop();
        onPanoramaChanged();
    } else {
        mTransform.applyFitMode();
    }
    if (mView) {
        mView->setProjection(mPanorama ? RenderEnums::Projection::Equirectangular
                                       : RenderEnums::Projection::Flat);
    }
    updateZoomIndicator();
    emit panoramaModeChanged();
}

//------------------------------------------------------------------------------
// Scrolling
//------------------------------------------------------------------------------
void ImageViewportController::scrollUp() { scroll(0, -kScrollStepPx, true); }
void ImageViewportController::scrollDown() { scroll(0, kScrollStepPx, true); }
void ImageViewportController::scrollLeft() { scroll(-kScrollStepPx, 0, true); }
void ImageViewportController::scrollRight() { scroll(kScrollStepPx, 0, true); }

void ImageViewportController::scroll(int dx, int dy, bool smooth) {
    if (!mInteractionEnabled)
        return;
    if (smooth && useSmoothMotion())
        scrollSmooth(dx, dy);
    else
        scrollPrecise(QPointF(dx, dy));
}

void ImageViewportController::scrollSmooth(int dx, int dy) {
    if (dx)
        scrollSmoothAxis(Qt::Horizontal, dx);
    if (dy)
        scrollSmoothAxis(Qt::Vertical, dy);
    mTransform.saveViewportPosition();
}

// Extends a running scroll in the same direction; a reversal starts from the
// current position.
void ImageViewportController::scrollSmoothAxis(Qt::Orientation axis, int delta) {
    QVariantAnimation &animation = axis == Qt::Horizontal ? mScrollAnimationX : mScrollAnimationY;
    const QPoint scrollPosition = mTransform.transform().scrollPosition();
    const int current = axis == Qt::Horizontal ? scrollPosition.x() : scrollPosition.y();
    const bool running = animation.state() == QAbstractAnimation::Running;
    const int runningEnd = running ? animation.endValue().toInt() : current;
    int end = current + delta;
    const bool reversed = (end < current && current < runningEnd) ||
                          (end > current && current > runningEnd);
    if (running && !reversed)
        end = runningEnd + delta;
    animation.stop();
    animation.setStartValue(current);
    animation.setEndValue(end);
    animation.start();
}

void ImageViewportController::scrollPrecise(QPointF delta) {
    stopPosAnimation();
    mTransform.scrollBy(delta);
    mTransform.saveViewportPosition();
}

void ImageViewportController::stopPosAnimation() {
    mScrollAnimationX.stop();
    mScrollAnimationY.stop();
    mZoomAnimation.stop();
}

void ImageViewportController::stopScaleTimerAndAnimations() {
    mScaleTimer.stop();
    stopPosAnimation();
}

//------------------------------------------------------------------------------
// Pointer input
//------------------------------------------------------------------------------
InteractionContext ImageViewportController::interactionContext() const {
    return {
        .hasImage = hasImage(),
        .panorama = mPanorama,
        .imageFits = mTransform.transform().scaledImageFits(),
        .dragsEnabled = !mClickZonePressed,
    };
}

bool ImageViewportController::pointerPressed(QPointF position, int button, int modifiers) {
    mPointerPosition = position;
    if (!mInteractionEnabled)
        return false;
    const auto pressed = static_cast<Qt::MouseButton>(button);
    if (mClickZonesEnabled) {
        const ClickZone zone = clickZoneAt(position);
        if (pressed == Qt::LeftButton && modifiers == Qt::NoModifier && zone != ClickZone::None) {
            setHighlightedClickZone(zone);
            setClickZonePressed(true);
            if (zone == ClickZone::Left)
                emit prevImageRequested();
            else
                emit nextImageRequested();
            return true;
        }
        setHighlightedClickZone(ClickZone::None);
    }
    if (!mImage)
        return false;
    mInteraction.press(position.toPoint());
    if (pressed == Qt::RightButton)
        mTransform.setZoomAnchor(position);
    // Presses also reach the mouse shortcuts, as in the widget UI.
    return false;
}

void ImageViewportController::pointerMoved(QPointF position, int buttons) {
    mPointerPosition = position;
    const auto held = Qt::MouseButtons::fromInt(buttons);
    if (!held.testAnyFlags(Qt::LeftButton | Qt::RightButton)) {
        updateClickZoneHover(position);
        showCursor();
        hideCursorTimed(true);
        return;
    }
    if (mClickZonePressed)
        return;
    applyInteractionStep(mInteraction.move(position.toPoint(), held, interactionContext()));
    updateCursorShape();
}

bool ImageViewportController::pointerReleased(QPointF position) {
    mPointerPosition = position;
    setClickZonePressed(false);
    const ViewportInteraction::Release release = mInteraction.release(hasImage());
    showCursor();
    hideCursorTimed(false);
    updateCursorShape();
    if (release.rescale)
        requestScaling();
    return release.consumed;
}

bool ImageViewportController::pointerDoubleClicked(QPointF position, int button, int modifiers) {
    mPointerPosition = position;
    if (!mInteractionEnabled)
        return false;
    const auto clicked = static_cast<Qt::MouseButton>(button);
    // The second press of the double click already acted on the click zone.
    if (mClickZonesEnabled && clicked == Qt::LeftButton && modifiers == Qt::NoModifier &&
        clickZoneAt(position) != ClickZone::None)
        return true;
    if (mPanorama && clicked == Qt::LeftButton) {
        mTransform.resetPanorama();
        return true;
    }
    return false;
}

void ImageViewportController::pointerEntered(QPointF position) {
    mPointerPosition = position;
    setPointerInside(true);
    updateClickZoneHover(position);
}

void ImageViewportController::pointerExited() {
    setPointerInside(false);
    setHighlightedClickZone(ClickZone::None);
    updateCursorShape();
}

bool ImageViewportController::wheelTurned(QPointF position, QPoint angleDelta, QPoint pixelDelta,
                                          int buttons, int modifiers) {
    mPointerPosition = position;
    if (!mInteractionEnabled)
        return false;
    if (mPanorama) {
        mTransform.zoomPanoramaByWheel(angleDelta.y());
        return true;
    }
    if (Qt::MouseButtons::fromInt(buttons).testFlag(Qt::RightButton)) {
        mInteraction.beginWheelZoom();
        if (angleDelta.y() > 0)
            zoomInCursor();
        else if (angleDelta.y() < 0)
            zoomOutCursor();
        return true;
    }
    if (modifiers != Qt::NoModifier)
        return false;

    const auto scrolling = static_cast<ImageScrolling>(mSettings.viewer.imageScrolling);
    const bool mouseWheel = mWheelClassifier.isMouseWheel(
        angleDelta, mSettings.viewer.trackpadDetection, mClock.elapsed());
    bool consumed = false;
    if (!mouseWheel) {
        consumed = true;
        if (scrolling != SCROLL_NONE) {
            stopPosAnimation();
            mTransform.scrollBy(trackpadScrollDelta(angleDelta, pixelDelta));
        }
    } else if (scrolling == SCROLL_BY_TRACKPAD_AND_WHEEL &&
               wheelCanScroll(angleDelta.y(), mTransform.transform().scaledRect(),
                              viewportSize().height())) {
        consumed = true;
        scroll(0, wheelScrollDistance(angleDelta.y(), mSettings.viewer.mouseScrollingSpeed), true);
    }
    mTransform.saveViewportPosition();
    return consumed;
}

void ImageViewportController::pinchStarted(QPointF centroid) {
    if (!mImage || mPanorama || !mInteractionEnabled)
        return;
    mPointerPosition = centroid;
    stopScaleTimerAndAnimations();
    mPinching = true;
    mPinchStartScale = currentScale();
    mTransform.setZoomAnchor(centroid);
}

void ImageViewportController::pinchUpdated(qreal scaleFactor) {
    if (!mPinching)
        return;
    mTransform.zoomTo(mPinchStartScale * static_cast<float>(scaleFactor));
}

void ImageViewportController::pinchFinished() {
    if (!mPinching)
        return;
    mPinching = false;
    mTransform.saveViewportPosition();
    requestScaling();
}

void ImageViewportController::applyInteractionStep(const InteractionStep &step) {
    switch (step.kind) {
    case InteractionStep::Kind::None:
        break;
    case InteractionStep::Kind::Pan:
        scrollPrecise(QPointF(step.delta));
        break;
    case InteractionStep::Kind::GestureZoom:
        if (mPanorama)
            mTransform.zoomPanoramaByGesture(step.distance);
        else
            mTransform.zoomTo(mTransform.transform().gestureZoomScale(step.distance));
        break;
    case InteractionStep::Kind::PanoramaDrag:
        mTransform.dragPanorama(step.delta);
        break;
    case InteractionStep::Kind::DragOut:
        emit draggedOut();
        break;
    case InteractionStep::Kind::NextImage:
        emit nextImageRequested();
        break;
    case InteractionStep::Kind::PrevImage:
        emit prevImageRequested();
        break;
    }
}

//------------------------------------------------------------------------------
// Click zones
//------------------------------------------------------------------------------
int ImageViewportController::clickZoneWidth() const {
    return kClickZoneWidth;
}

bool ImageViewportController::clickZonesEnabled() const {
    return mClickZonesEnabled;
}

bool ImageViewportController::clickZonesDrawn() const {
    return mSettings.viewer.clickableEdgesVisible;
}

ImageViewportController::ClickZone ImageViewportController::highlightedClickZone() const {
    return mHighlightedZone;
}

bool ImageViewportController::isClickZonePressed() const {
    return mClickZonePressed;
}

void ImageViewportController::updateClickZonesEnabled() {
    const bool enabled = mSettings.viewer.clickableEdges && !panelOccupiesSide(mSettings.panel) &&
                         viewportSize().width() > kClickZonesMinViewportWidth;
    if (mClickZonesEnabled == enabled)
        return;
    mClickZonesEnabled = enabled;
    if (!enabled) {
        mHighlightedZone = ClickZone::None;
        mClickZonePressed = false;
    }
    updateCursorShape();
    emit clickZonesChanged();
}

ImageViewportController::ClickZone ImageViewportController::clickZoneAt(QPointF position) const {
    const int width = viewportSize().width();
    const int height = viewportSize().height();
    if (position.y() < 0.0 || position.y() >= height)
        return ClickZone::None;
    if (position.x() >= 0.0 && position.x() < kClickZoneWidth)
        return ClickZone::Left;
    if (position.x() >= width - kClickZoneWidth && position.x() < width)
        return ClickZone::Right;
    return ClickZone::None;
}

void ImageViewportController::updateClickZoneHover(QPointF position) {
    if (!mClickZonesEnabled)
        return;
    setClickZonePressed(false);
    setHighlightedClickZone(clickZoneAt(position));
}

void ImageViewportController::setHighlightedClickZone(ClickZone zone) {
    if (mHighlightedZone == zone)
        return;
    mHighlightedZone = zone;
    updateCursorShape();
    emit clickZonesChanged();
}

void ImageViewportController::setClickZonePressed(bool pressed) {
    if (mClickZonePressed == pressed)
        return;
    mClickZonePressed = pressed;
    emit clickZonesChanged();
}

void ImageViewportController::setPointerInside(bool inside) {
    mPointerInside = inside;
}

//------------------------------------------------------------------------------
// Zoom indicator and cursor
//------------------------------------------------------------------------------
int ImageViewportController::zoomPercent() const {
    return qRound(currentScale() * kPercent);
}

bool ImageViewportController::isZoomIndicatorVisible() const {
    return mZoomIndicatorVisible;
}

void ImageViewportController::updateZoomIndicator() {
    mZoomIndicatorTimer.stop();
    if (!mImage || mPanorama) {
        setZoomIndicatorVisible(false);
        return;
    }
    switch (mSettings.overlays.zoomIndicatorMode) {
    case SettingsEnums::ZoomIndicatorMode::Enabled:
        setZoomIndicatorVisible(true);
        break;
    case SettingsEnums::ZoomIndicatorMode::Auto:
        setZoomIndicatorVisible(true);
        mZoomIndicatorTimer.start();
        break;
    case SettingsEnums::ZoomIndicatorMode::Disabled:
        setZoomIndicatorVisible(false);
        break;
    }
}

void ImageViewportController::setZoomIndicatorVisible(bool visible) {
    if (mZoomIndicatorVisible == visible)
        return;
    mZoomIndicatorVisible = visible;
    emit zoomIndicatorVisibleChanged();
}

Qt::CursorShape ImageViewportController::cursorShape() const {
    return mCursorShape;
}

void ImageViewportController::showCursor() {
    mCursorTimer.stop();
    setCursorHidden(false);
}

void ImageViewportController::hideCursorTimed(bool restartTimer) {
    if (restartTimer || !mCursorTimer.isActive())
        mCursorTimer.start();
}

// Hides the pointer over a displayed image in the active window, except over
// a click zone, when the cursor auto-hide setting is on.
void ImageViewportController::hideCursor() {
    mCursorTimer.stop();
    const bool windowActive = mWindow && mWindow->isActive();
    if (!mImage || !windowActive || !mPointerInside || !mSettings.viewer.cursorAutohide)
        return;
    if (mClickZonesEnabled && clickZoneAt(mPointerPosition) != ClickZone::None)
        return;
    setCursorHidden(true);
}

void ImageViewportController::setCursorHidden(bool hidden) {
    if (mCursorHidden == hidden)
        return;
    mCursorHidden = hidden;
    updateCursorShape();
}

void ImageViewportController::updateCursorShape() {
    Qt::CursorShape shape = Qt::ArrowCursor;
    if (mInteraction.mode() == ViewportInteraction::Mode::Pan)
        shape = Qt::ClosedHandCursor;
    else if (mInteraction.mode() == ViewportInteraction::Mode::Zoom)
        shape = Qt::SizeVerCursor;
    else if (mCursorHidden)
        shape = Qt::BlankCursor;
    else if (mHighlightedZone != ClickZone::None)
        shape = Qt::PointingHandCursor;
    if (mCursorShape == shape)
        return;
    mCursorShape = shape;
    emit cursorShapeChanged();
}
