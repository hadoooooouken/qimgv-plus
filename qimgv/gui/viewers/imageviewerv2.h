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

enum MouseInteractionState {
    MOUSE_NONE,
    MOUSE_DRAG_BEGIN,
    MOUSE_DRAG,
    MOUSE_PAN,
    MOUSE_ZOOM,
    MOUSE_WHEEL_ZOOM,
    MOUSE_GESTURE
};

enum ViewLockMode {
    LOCK_NONE,
    LOCK_ZOOM,
    LOCK_ALL
};

class ImageViewerV2 : public QGraphicsView
{
    Q_OBJECT
public:
    ImageViewerV2(QWidget* parent = nullptr);
    ~ImageViewerV2();
    virtual ImageFitMode fitMode() const;
    virtual QRect scaledRectR() const;
    virtual float currentScale() const;
    virtual QSize sourceSize() const;
    virtual void showImage(std::unique_ptr<QPixmap> _pixmap, QString filePath = "");
    virtual void showAnimation(std::shared_ptr<QMovie> _animation);
    virtual void setScaledPixmap(QPixmap newFrame);
    void setUpscaledCrop(const QImage &cropImg, QRect origCrop);
    void hideUpscaledCrop();
    virtual bool isDisplaying() const;
    bool panoramaMode() const { return mPanoramaMode; }
    bool isBusyInteracting() const;
    void setColorAdjustments(float exposure, float contrast, float brightness, float temperature, float tint, float saturation, float hue);
    void updateCasSettings();

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
    virtual void setFitWindowStretch();
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
    void centerOnPixmap();
    void onScrollTimelineFinished();
    void onZoomTimelineValueChanged(qreal value);

    void onDPRChanged();
private:
    QGraphicsScene *scene;
    std::shared_ptr<QPixmap> pixmap;
    QPixmap pixmapScaled;
    std::shared_ptr<QMovie> movie;
    FilterPixmapItem pixmapItem, pixmapItemScaled, pixmapItemCrop;
    QTimer *animationTimer, *scaleTimer;
    QScrollBar *hs, *vs;
    QPoint mouseMoveStartPos, mousePressPos, drawPos;
    bool transparencyGrid, expandImage, keepFitMode,
         loopPlayback,     mIsFullscreen,  scrollBarWorkaround,
         useFixedZoomLevels, trackpadDetection;
    QList<float> zoomLevels;
    MouseInteractionState mouseInteraction;
    const int SCROLL_UPDATE_RATE = 7;
    const int DEFAULT_SCROLL_DISTANCE = 240;
    const qreal TRACKPAD_SCROLL_MULTIPLIER = 0.7;
    const qreal WHEEL_SCROLL_MULTIPLIER = 2.0f;
    const int ANIMATION_SPEED = 150;
    // how many px you can move while holding RMB until it counts as a zoom attempt
    int zoomThreshold = 4;
    int dragThreshold = 10;
    int gestureThreshold = 40;

    bool dragsEnabled = true;

    float zoomStep = 0.1f, dpr;
    float minScale, maxScale, fitWindowScale, fitWindowStretchScale, expandLimit, lockedScale;
    QPointF savedViewportPos;
    ViewLockMode mViewLock;

    QPair<QPointF, QPoint> zoomAnchor; // [pixmap coords, viewport coords]

    QElapsedTimer lastTouchpadScroll;

    ImageFitMode imageFitMode, imageFitModeDefault;
    ImageFocusPoint focusIn1to1;
    ScalingFilter mScalingFilter;
    bool mApplyFilterAt100 = false;
    bool mUseUpscayl = false;

    QPixmap *checkboard;

    void zoomAnchored(float newScale);
    void fitNormal();
    void fitWidth();
    void fitWindow();
    void fitWindowStretch();

    void scroll(int dx, int dy, bool animated);

    void mousePanWrapping(QMouseEvent *event);
    void mousePan(QMouseEvent *event);
    void mouseMoveZoom(QMouseEvent *event);
    void reset();
    void applyFitMode();

    QTimeLine *scrollTimeLineX, *scrollTimeLineY;
    QTimeLine *zoomTimeLine;
    float zoomStartScale;
    float zoomTargetScale;
    static qreal smootherstepEasing(qreal t);
    void stopPosAnimation();
    QPointF sceneRoundPos(QPointF scenePoint) const;
    QRectF sceneRoundRect(QRectF sceneRect) const;
    void doZoom(float newScale);
    void swapToOriginalPixmap();
    void setZoomAnchor(QPoint viewportPos);
    void updatePixmap(std::unique_ptr<QPixmap> newPixmap);
    Qt::TransformationMode selectTransformationMode();
    void centerIfNecessary();
    void snapToEdges();
    void scrollSmooth(int dx, int dy);
    void scrollPrecise(int dx, int dy);
    void updateFitWindowScale();
    void updateFitWindowStretchScale();
    void updateMinScale();
    void fitFree(float scale);
    void applySavedViewportPos();
    void saveViewportPos();
    void lockZoom();
    void doZoomIn(bool atCursor);
    void doZoomOut(bool atCursor);

private:
    class PanoramaGraphicsItem *panoramaItem = nullptr;
    QGraphicsSvgItem *svgItem = nullptr;
    bool mSvgMode = false;
    bool mPanoramaMode = false;
    float mPanoramaYaw = 0.0f, mPanoramaPitch = 0.0f, mPanoramaFov = 90.0f;
    QString currentFilePath;
};
