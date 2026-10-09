#pragma once

#include <QElapsedTimer>
#include <QImage>
#include <QObject>
#include <QPoint>
#include <QPointF>
#include <QPointer>
#include <QRect>
#include <QSharedPointer>
#include <QSize>
#include <QString>
#include <QTimer>
#include <QVariantAnimation>
#include <QtQml/qqmlregistration.h>

#include <memory>

#include "components/animationplayer/animationplayer.h"
#include "components/viewtransform/viewportinteraction.h"
#include "components/viewtransform/viewtransformcontroller.h"
#include "gui/quick/bridges/uisettings.h"
#include "gui/quick/render/imagerenderitem.h"
#include "gui/quick/ui/framepresentationtracker.h"
#include "settings_types.h"
#include "utils/coloradjustments.h"

class QQuickItemGrabResult;
class QQuickWindow;

// Interaction and display state of the Qt Quick image viewer
// (ImageViewport.qml): everything ImageViewerV2 and ViewerWidget decide for
// the widget UI, without the widgets.
//
// It owns the zoom / pan / fit state (ViewTransformController), the mouse
// interaction (ViewportInteraction), animation playback (AnimationPlayer),
// smooth zoom and scroll animations, the settle pass and the viewer chrome
// state (zoom indicator, click zones, cursor), and drives the
// ImageRenderItem it is given (view): image, placement, filter, panorama
// camera and `settled`. ImageViewport.qml only lays out the items, binds the
// chrome properties and forwards pointer input through the invokables below;
// input the viewer does not use is forwarded by QML to the Actions bridge.
//
// Settling: any change of the view marks the rendering unsettled. Once the
// view rests (a short delay after the last change, or at the end of a zoom),
// the item gets `settled` (exact downsample / MKS2021 pass) and
// renderingSettled() is emitted after the frame showing it has been
// presented. While an animation plays the item stays unsettled, so the
// high-quality pass does not run for every frame.
//
// Owned by the Quick UI host (QuickUiHost) and handed to QML as a required
// property; the view and its window are owned by QML. GUI thread only.
class ImageViewportController final : public QObject, private IViewSurface {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by the Quick UI host")
    Q_PROPERTY(ImageRenderItem *view READ view WRITE setView NOTIFY viewChanged FINAL)
    Q_PROPERTY(bool hasImage READ hasImage NOTIFY imageChanged FINAL)
    Q_PROPERTY(qreal scale READ currentScale NOTIFY scaleChanged FINAL)
    Q_PROPERTY(int zoomPercent READ zoomPercent NOTIFY scaleChanged FINAL)
    Q_PROPERTY(bool zoomIndicatorVisible READ isZoomIndicatorVisible NOTIFY zoomIndicatorVisibleChanged FINAL)
    Q_PROPERTY(bool panoramaMode READ panoramaMode NOTIFY panoramaModeChanged FINAL)
    Q_PROPERTY(Qt::CursorShape cursorShape READ cursorShape NOTIFY cursorShapeChanged FINAL)
    Q_PROPERTY(bool clickZonesEnabled READ clickZonesEnabled NOTIFY clickZonesChanged FINAL)
    Q_PROPERTY(bool clickZonesDrawn READ clickZonesDrawn NOTIFY clickZonesChanged FINAL)
    Q_PROPERTY(ClickZone highlightedClickZone READ highlightedClickZone NOTIFY clickZonesChanged FINAL)
    Q_PROPERTY(bool clickZonePressed READ isClickZonePressed NOTIFY clickZonesChanged FINAL)
    Q_PROPERTY(int clickZoneWidth READ clickZoneWidth CONSTANT FINAL)
    Q_PROPERTY(bool reducedMotion READ reducedMotion NOTIFY reducedMotionChanged FINAL)
    Q_PROPERTY(bool renderingSettled READ isRenderingSettled NOTIFY renderingSettledChanged FINAL)

public:
    enum class ClickZone { None, Left, Right };
    Q_ENUM(ClickZone)

    // Width of the left and right click zones, logical pixels.
    static constexpr int kClickZoneWidth = 110;
    // Click zones are disabled in viewports this narrow or narrower.
    static constexpr int kClickZonesMinViewportWidth = 250;

    // settings: the initial UI settings (applySettings()).
    explicit ImageViewportController(const UiSettingsSnapshot &settings,
                                     QObject *parent = nullptr);
    ~ImageViewportController() override;

    // --- view -----------------------------------------------------------
    [[nodiscard]] ImageRenderItem *view() const;
    // The item that shows the image (nullptr detaches). The controller
    // pushes its whole display state into a newly set view.
    void setView(ImageRenderItem *view);

    // Applies changed settings, as the widget viewer does on
    // Settings::settingsChanged; the session overrides (scaling filter,
    // transparency grid) return to the settings when the viewer settings
    // changed.
    void applySettings(const UiSettingsSnapshot &settings);

    // --- document (IViewerPort) ------------------------------------------
    // Shows image; a null image is rejected with a warning. Showing the same
    // file again with the same pixel count (a rotation or mirror) keeps the
    // scale or fit mode and the relative viewport position.
    void showImage(std::shared_ptr<const QImage> image, const QString &filePath);
    // Plays the animation at filePath; decode failures are reported through
    // playbackError().
    void showAnimation(const QString &filePath, const QString &format);
    void closeImage();
    [[nodiscard]] bool hasImage() const;
    void setUpscaledCrop(const QImage &crop, const QRect &sourceRect);
    void hideUpscaledCrop();
    // Re-evaluates scaling and the settle pass at the current view.
    void refreshScaling();
    // Visible part of the image, in source pixels.
    [[nodiscard]] QRect visibleOriginalImageRect() const;
    [[nodiscard]] float currentScale() const;
    [[nodiscard]] float devicePixelRatio() const;
    [[nodiscard]] bool isBusyInteracting() const;
    [[nodiscard]] bool isRenderingSettled() const;
    [[nodiscard]] bool panoramaMode() const;
    void setColorAdjustments(const ColorAdjustments &adjustments);
    // Reads back the visible part of the image as it is shown (filtering,
    // colour adjustments, tone mapping), in device pixels; the whole
    // viewport in panorama mode. Asynchronous: visibleImageGrabbed() or
    // visibleImageGrabFailed() follows. A newer grab replaces a pending one.
    void grabVisibleImage();

    // --- viewer actions -------------------------------------------------
    void zoomIn();
    void zoomOut();
    // Zoom around the pointer when it is over the viewer, otherwise around
    // the viewport centre.
    void zoomInCursor();
    void zoomOutCursor();
    void scrollUp();
    void scrollDown();
    void scrollLeft();
    void scrollRight();
    void fitWindow();
    void fitWidth();
    void fitOriginal();
    void fitHeight();
    // Between fit-to-window and 1:1.
    void switchFitMode();
    void toggleLockZoom();
    void toggleLockView();
    [[nodiscard]] bool isZoomLocked() const;
    [[nodiscard]] bool isViewLocked() const;
    // Session override until the viewer settings change.
    void toggleTransparencyGrid();
    // Session filter; ScalingFilterSelection decides toggles and cycling.
    void setScalingFilter(ScalingFilter filter);
    [[nodiscard]] ScalingFilter scalingFilter() const;
    [[nodiscard]] ScalingFilter configuredScalingFilter() const;
    void togglePanorama();

    // --- chrome state (QML) ---------------------------------------------
    [[nodiscard]] int zoomPercent() const;
    [[nodiscard]] bool isZoomIndicatorVisible() const;
    [[nodiscard]] Qt::CursorShape cursorShape() const;
    [[nodiscard]] bool clickZonesEnabled() const;
    // The highlighted zone is painted (Settings::clickableEdgesVisible()).
    [[nodiscard]] bool clickZonesDrawn() const;
    [[nodiscard]] ClickZone highlightedClickZone() const;
    [[nodiscard]] bool isClickZonePressed() const;
    [[nodiscard]] int clickZoneWidth() const;
    // The platform asks for reduced motion: zoom and scroll jump.
    [[nodiscard]] bool reducedMotion() const;

    // --- pointer input (QML) ----------------------------------------------
    // Positions are in viewport (item) coordinates. buttons and modifiers
    // are Qt::MouseButtons / Qt::KeyboardModifiers values. The bool results
    // tell whether the viewer consumed the event; QML forwards unconsumed
    // events to the action shortcuts.
    Q_INVOKABLE bool pointerPressed(QPointF position, int button, int modifiers);
    Q_INVOKABLE void pointerMoved(QPointF position, int buttons);
    Q_INVOKABLE bool pointerReleased(QPointF position);
    Q_INVOKABLE bool pointerDoubleClicked(QPointF position, int button, int modifiers);
    Q_INVOKABLE void pointerEntered(QPointF position);
    Q_INVOKABLE void pointerExited();
    Q_INVOKABLE bool wheelTurned(QPointF position, QPoint angleDelta, QPoint pixelDelta,
                                 int buttons, int modifiers);
    // Pinch zoom (touch screen, precision touchpad); scaleFactor is relative
    // to the start of the pinch.
    Q_INVOKABLE void pinchStarted(QPointF centroid);
    Q_INVOKABLE void pinchUpdated(qreal scaleFactor);
    Q_INVOKABLE void pinchFinished();

signals:
    void viewChanged();
    void imageChanged();
    void scaleChanged(qreal scale);
    void zoomIndicatorVisibleChanged();
    void panoramaModeChanged();
    void cursorShapeChanged();
    void clickZonesChanged();
    void reducedMotionChanged();
    void renderingSettledChanged();

    // IViewerPort / UiEvents outputs.
    // The visible area may be AI upscaled at the displayed size (device
    // pixels): Upscayl is on and the view is zoomed in above 1:1. The GPU
    // shows every scaling filter itself, so no CPU-scaled copy is requested.
    void upscaleRequested(QSize size);
    void renderingSettled();
    void draggedOut();
    void nextImageRequested();
    void prevImageRequested();
    // Image area on screen after a fit-to-window centring (crop overlay).
    void imageAreaChanged(QRect area);
    void playbackError(const QString &message);
    void visibleImageGrabbed(const QImage &image);
    void visibleImageGrabFailed();

private:
    // IViewSurface
    [[nodiscard]] QSize viewportSize() const override;
    [[nodiscard]] QPointF pointerPosition() const override;

    bool eventFilter(QObject *watched, QEvent *event) override;

    // View and window.
    void attachWindow(QQuickWindow *window);
    void pushDisplayState();
    void applyFilter();
    void applyDisplayColor();
    void onViewportResized();
    void syncDevicePixelRatio();
    void onDevicePixelRatioChanged();
    [[nodiscard]] qreal windowDevicePixelRatio() const;

    // Transform notifications.
    void onTransformChanged();
    void onScaleChanged(qreal scale);
    void onViewPositionChanged();
    void onPanoramaChanged();

    // Document.
    void reset();
    void onAnimationFrame(std::shared_ptr<const QImage> frame);

    // Scaling and settling.
    void requestScaling();
    [[nodiscard]] bool wantsUpscale() const;
    void presentSettledFrame();
    void onFramePresented();
    void setRenderingSettled(bool settled);
    void applyItemSettled();

    // Zoom and scroll.
    void zoomBy(bool zoomIn, bool atCursor);
    [[nodiscard]] QPointF zoomAnchorPosition(bool atCursor) const;
    void startZoom(float newScale);
    void onZoomAnimationValue(const QVariant &value);
    void onZoomAnimationFinished();
    void setFitMode(ImageFitMode mode);
    void forceFitMode(ImageFitMode mode);
    void scroll(int dx, int dy, bool smooth);
    void scrollSmooth(int dx, int dy);
    void scrollSmoothAxis(Qt::Orientation axis, int delta);
    void scrollPrecise(QPointF delta);
    void stopPosAnimation();
    void stopScaleTimerAndAnimations();
    [[nodiscard]] bool isZoomAnimating() const;
    [[nodiscard]] bool useSmoothMotion() const;

    // Pointer.
    void applyInteractionStep(const InteractionStep &step);
    [[nodiscard]] InteractionContext interactionContext() const;
    [[nodiscard]] ClickZone clickZoneAt(QPointF position) const;
    void updateClickZoneHover(QPointF position);
    void setHighlightedClickZone(ClickZone zone);
    void setClickZonePressed(bool pressed);
    void setPointerInside(bool inside);

    // Chrome.
    void updateZoomIndicator();
    void setZoomIndicatorVisible(bool visible);
    void showCursor();
    void hideCursorTimed(bool restartTimer);
    void hideCursor();
    void setCursorHidden(bool hidden);
    void updateCursorShape();
    void updateClickZonesEnabled();

    QPointer<ImageRenderItem> mView;
    QPointer<QQuickWindow> mWindow;
    // Readback in flight (grabVisibleImage()) and the part of it to keep.
    QSharedPointer<QQuickItemGrabResult> mPendingGrab;
    QRect mPendingGrabCrop;
    ViewTransformController mTransform;
    ViewportInteraction mInteraction;
    WheelClassifier mWheelClassifier;
    QElapsedTimer mClock;
    AnimationPlayer mPlayer;
    FramePresentationTracker mPresentation;

    QTimer mScaleTimer;
    QTimer mCursorTimer;
    QTimer mZoomIndicatorTimer;
    QVariantAnimation mZoomAnimation;
    QVariantAnimation mScrollAnimationX;
    QVariantAnimation mScrollAnimationY;
    float mZoomStartScale = 1.0f;
    float mZoomTargetScale = 1.0f;

    UiSettingsSnapshot mSettings;
    bool mSettingsApplied = false;
    ScalingFilter mScalingFilter = QI_FILTER_BILINEAR;
    bool mTransparencyGrid = false;
    ColorAdjustments mColorAdjustments;

    std::shared_ptr<const QImage> mImage;
    QString mFilePath;
    bool mAwaitingFirstFrame = false;
    bool mPanorama = false;

    bool mRenderingSettled = false;
    bool mPinching = false;
    float mPinchStartScale = 1.0f;

    QPointF mPointerPosition;
    bool mPointerInside = false;
    bool mClickZonesEnabled = false;
    ClickZone mHighlightedZone = ClickZone::None;
    bool mClickZonePressed = false;
    bool mZoomIndicatorVisible = false;
    bool mCursorHidden = false;
    Qt::CursorShape mCursorShape = Qt::ArrowCursor;
};
