#pragma once

#include <QGraphicsView>
#include <QGraphicsScene>
#include "filterpixmapitem.h"
#include <QGraphicsSvgItem>
#include <QSvgRenderer>
#include <QElapsedTimer>
#include <QWheelEvent>
#include <QTimeLine>
#include <QScrollBar>
#include <QMovie>
#include <QColor>
#include <QTimer>
#include <QDebug>
#include <memory>
#include <cmath>
#include "settings_types.h"
#include "components/viewtransform/viewtransformcontroller.h"

enum MouseInteractionState {
    MOUSE_NONE,
    MOUSE_DRAG_BEGIN,
    MOUSE_DRAG,
    MOUSE_PAN,
    MOUSE_ZOOM,
    MOUSE_WHEEL_ZOOM,
    MOUSE_GESTURE
};

// Widget image viewer. Zoom, pan, fit and panorama camera state live in
// ViewTransformController; this class renders that state with QGraphicsView
// items and translates mouse/keyboard input into controller intents.
class ImageViewerV2 : public QGraphicsView, private IViewSurface
{
    Q_OBJECT
public:
    ImageViewerV2(QWidget* parent = nullptr);
    ~ImageViewerV2();
    virtual ImageFitMode fitMode() const;
    virtual QRect scaledRectR() const;
    virtual float currentScale() const;
    virtual QSize sourceSize() const;
    virtual void showImage(std::shared_ptr<const QImage> _image, QString filePath = "");
    virtual void showAnimation(const QString &filePath, const QString &format);
    virtual void setScaledImage(QImage newFrame);
    bool isRenderingSettled() const;
    void setUpscaledCrop(const QImage &cropImg, QRect origCrop);
    void hideUpscaledCrop();
    virtual bool isDisplaying() const;
    bool panoramaMode() const { return mPanoramaMode; }
    bool isBusyInteracting() const;
    void setColorAdjustments(const ColorAdjustments &adjustments);
    void updateCasSettings();
    void onMouseMoveFullscreen();
    void refreshScaling();

    virtual bool imageFits() const;
    bool scaledImageFits() const;
    virtual ScalingFilter scalingFilter() const;
    virtual QWidget *widget();
    bool hasAnimation() const;

    QSize scaledSizeR() const;

    virtual QRect visibleImageRect() const;
    virtual QRect visibleOriginalImageRect() const;
    QRect visibleImageViewportRect() const;
    virtual QPixmap currentScaledPixmapCopy() const;
    QImage grabViewportImage() const;
    float getDpr() const;

    void pauseResume();
    void enableDrags();
    void disableDrags();

signals:
    void scalingRequested(QSize, ScalingFilter);
    void renderingSettled();
    void scaleChanged(qreal);
    void sourceSizeChanged(QSize);
    void imageAreaChanged(QRect);
    void draggedOut();
    void playbackFinished();
    void animationPaused(bool);
    void frameChanged(int);
    void durationChanged(int);
    void nextImageRequested();
    void prevImageRequested();

public slots:
    virtual void setFitMode(ImageFitMode mode);
    virtual void setFitOriginal();
    virtual void setFitWidth();
    virtual void setFitWindow();
    virtual void setFitHeight();
    void setExpandSmallImagesInFitMode(bool enabled);
    void switchFitMode();
    virtual void zoomIn();
    virtual void zoomOut();
    virtual void zoomInCursor();
    virtual void zoomOutCursor();
    virtual void readSettings();
    virtual void scrollUp();
    virtual void scrollDown();
    virtual void scrollLeft();
    virtual void scrollRight();
    virtual void startAnimation();
    virtual void stopAnimation();
    virtual void closeImage();
    virtual void setExpandImage(bool mode);
    virtual void show();
    virtual void hide();
    virtual void setFilterNearest();
    virtual void setFilterBilinear();
    virtual void setScalingFilter(ScalingFilter filter);
    void setLoopPlayback(bool mode);
    void toggleTransparencyGrid();
    void togglePanorama();

    void nextFrame();
    void prevFrame();

    bool showAnimationFrame(int frame);
    void onFullscreenModeChanged(bool mode);
    void toggleLockZoom();
    bool lockZoomEnabled();
    void toggleLockView();
    bool lockViewEnabled();

protected:
    virtual void mousePressEvent(QMouseEvent *event);
    virtual void mouseMoveEvent(QMouseEvent* event);
    virtual void mouseReleaseEvent(QMouseEvent *event);
    virtual void mouseDoubleClickEvent(QMouseEvent *event) override;
    virtual void resizeEvent(QResizeEvent* event);
    void keyPressEvent(QKeyEvent *event);
    void wheelEvent(QWheelEvent *event);
    void showEvent(QShowEvent *event);
    void drawBackground(QPainter *painter, const QRectF &rect);

    bool event(QEvent *ev) override;
protected slots:
    void onAnimationTimer();

private slots:
    void requestScaling();
    void scrollToX(int x);
    void scrollToY(int y);
    void onScrollTimelineFinished();
    void onZoomTimelineValueChanged(qreal value);

    void onDPRChanged();
private:
    // IViewSurface
    QSize viewportSize() const override;
    QPointF pointerPosition() const override;

    QGraphicsScene *scene;
    std::shared_ptr<const QImage> image;
    QImage imageScaled;
    std::shared_ptr<QMovie> movie;
    FilterPixmapItem pixmapItem, pixmapItemScaled, pixmapItemCrop;
    QTimer *animationTimer, *scaleTimer;
    QPoint mouseMoveStartPos, mousePressPos, drawPos;
    bool transparencyGrid, loopPlayback, mIsFullscreen,
         trackpadDetection, mAnimationActive;
    MouseInteractionState mouseInteraction;
    const int DEFAULT_SCROLL_DISTANCE = 240;
    const qreal TRACKPAD_SCROLL_MULTIPLIER = 0.7;
    const qreal WHEEL_SCROLL_MULTIPLIER = 2.0f;
    const int ANIMATION_SPEED = 150;
    static constexpr float kScaleEpsilon = ViewTransform::kScaleEpsilon;
    // how many px you can move while holding RMB until it counts as a zoom attempt
    int zoomThreshold = 4;
    int dragThreshold = 10;
    int gestureThreshold = 40;

    bool dragsEnabled = true;
    bool mRenderingSettled = false;
    bool mFramePresentationPending = false;

    std::unique_ptr<ViewTransformController> viewTransform;

    QElapsedTimer lastTouchpadScroll;

    ScalingFilter mScalingFilter;
    bool mUseUpscayl = false;

    QPixmap checkerboard;

    void scroll(int dx, int dy, bool animated);

    void mousePan(QMouseEvent *event);
    void mouseMoveZoom(QMouseEvent *event);
    void reset();
    void requestSettledFramePresentation();
    void onViewportFrameSwapped();
    void setRenderingSettled(bool settled);
    void onMovieFrameChanged(int frameNumber);
    void onTransformChanged();
    void onViewPositionChanged();
    void applyPanoramaView();
    void syncDevicePixelRatio();
    void updateInputThresholds();

    QTimeLine *scrollTimeLineX, *scrollTimeLineY;
    QTimeLine *zoomTimeLine;
    float zoomStartScale;
    float zoomTargetScale;
    static qreal smootherstepEasing(qreal t);
    void stopPosAnimation();
    QPointF sceneRoundPos(QPointF scenePoint) const;
    void swapToOriginalImage();
    void updateImage(std::shared_ptr<const QImage> newImage);
    Qt::TransformationMode selectTransformationMode();
    void scrollSmooth(int dx, int dy);
    void scrollPrecise(int dx, int dy);
    void stopScaleTimerAndAnimations();
    void doZoomIn(bool atCursor);
    void doZoomOut(bool atCursor);
    void startZoom(float newScale);
    QPointF zoomAnchorPosition(bool atCursor) const;

private:
    class PanoramaGraphicsItem *panoramaItem = nullptr;
    QGraphicsSvgItem *svgItem = nullptr;
    bool mSvgMode = false;
    bool mPanoramaMode = false;
    QString currentFilePath;
    QElapsedTimer lastFullscreenUpdate;
    QTimer *fullscreenUpdateTimer = nullptr;
};
