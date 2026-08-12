#pragma once

#include "gui/customwidgets/floatingwidgetcontainer.h"
#include <QVBoxLayout>
#include "gui/viewers/imageviewerv2.h"
#include "gui/overlays/zoomindicatoroverlayproxy.h"
#include "gui/overlays/clickzoneoverlay.h"
#include "gui/contextmenu.h"

enum CurrentWidget {
    IMAGEVIEWER,
    UNSET
};

class ViewerWidget : public FloatingWidgetContainer
{
    Q_OBJECT
public:
    explicit ViewerWidget(QWidget *parent = nullptr);
    QRect imageRect();
    float currentScale();
    QRect visibleImageRect() const;
    QRect visibleOriginalImageRect() const;
    QPixmap currentScaledPixmapCopy() const;
    bool copyCurrentViewportToClipboard() const;
    float getDpr() const;
    QSize sourceSize();

    void setInteractionEnabled(bool mode);
    bool interactionEnabled();

    bool showImage(std::shared_ptr<const QImage> image, QString filePath = "");
    bool showAnimation(const QString &filePath, const QString &format);
    void onScalingFinished(QImage scaled);
    void setUpscaledCrop(const QImage &cropImg, QRect origCrop);
    void hideUpscaledCrop();
    bool panoramaMode() const { return imageViewer ? imageViewer->panoramaMode() : false; }
    bool isBusyInteracting() const;
    void refreshScaling();
    bool isDisplaying();
    bool isRenderingSettled() const;
    bool lockZoomEnabled();
    bool lockViewEnabled();
    ScalingFilter scalingFilter();
    void setColorAdjustments(float exposure, float contrast, float brightness, float temperature, float tint, float saturation, float hue);
    void updateCasSettings();
    void onMouseMoveFullscreen();

private:
    QVBoxLayout layout;
    std::unique_ptr<ImageViewerV2> imageViewer;
    std::unique_ptr<ContextMenu> contextMenu;
    ZoomIndicatorOverlayProxy *zoomIndicator;
    ClickZoneOverlay *clickZoneOverlay;

    void enableImageViewer();

    CurrentWidget currentWidget;
    bool mInteractionEnabled;
    QTimer cursorTimer;
    const int CURSOR_HIDE_TIMEOUT_MS = 1000;
    bool mIsFullscreen;

    float mExposure = 0.0f;
    float mContrast = 1.0f;
    float mBrightness = 0.0f;
    float mTemperature = 0.0f;
    float mTint = 0.0f;
    float mSaturation = 1.0f;
    float mHue = 0.0f;

    void disableImageViewer();

    bool eventFilter(QObject *object, QEvent *event);

private slots:
    void onScaleChanged(qreal);
    void onAnimationPlaybackFinished();

signals:
    void scaleChanged(qreal scale);
    void scalingRequested(QSize, ScalingFilter);
    void renderingSettled();
    void zoomIn();
    void zoomOut();
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
    void switchFitMode();
    void toggleTransparencyGrid();
    void draggedOut();
    void setFilterNearest();
    void setFilterBilinear();
    void setScalingFilter(ScalingFilter filter);
    void playbackFinished();
    void toggleLockZoom();
    void toggleLockView();
    void showScriptSettings();
    void nextImageRequested();
    void prevImageRequested();
    void clickableEdgePointerMoved(QPoint globalPosition);

public slots:
    void setFitMode(ImageFitMode mode);
    ImageFitMode fitMode();
    void closeImage();
    void hideCursor();
    void showCursor();
    void hideCursorTimed(bool restartTimer);

    void showContextMenu();
    void hideContextMenu();
    void showContextMenu(QPoint pos);
    void onFullscreenModeChanged(bool);
    void readSettings();
    void togglePanorama();

protected:
    void mouseMoveEvent(QMouseEvent *event);
    void mousePressEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void hideEvent(QHideEvent *event);

    void keyPressEvent(QKeyEvent *event);
    void leaveEvent(QEvent *event);
    bool focusNextPrevChild(bool mode);
};
