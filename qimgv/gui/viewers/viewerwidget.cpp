/*
 * This widget displays the ImageViewer.
 */

#include "viewerwidget.h"

ViewerWidget::ViewerWidget(QWidget *parent)
    : FloatingWidgetContainer(parent),
      imageViewer(nullptr),
      contextMenu(nullptr),
      currentWidget(UNSET),
      mInteractionEnabled(false),
      mIsFullscreen(false)
{
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setMouseTracking(true);
    layout.setContentsMargins(0, 0, 0, 0);
    layout.setSpacing(0);
    this->setLayout(&layout);

    imageViewer.reset(new ImageViewerV2(this));
    layout.addWidget(imageViewer.get());
    imageViewer->hide();

    connect(imageViewer.get(), &ImageViewerV2::scalingRequested, this, &ViewerWidget::scalingRequested);
    connect(imageViewer.get(), &ImageViewerV2::scaleChanged, this, &ViewerWidget::onScaleChanged);
    connect(imageViewer.get(), &ImageViewerV2::playbackFinished, this, &ViewerWidget::onAnimationPlaybackFinished);
    connect(this, &ViewerWidget::toggleTransparencyGrid, imageViewer.get(), &ImageViewerV2::toggleTransparencyGrid);
    connect(this, &ViewerWidget::setFilterNearest,       imageViewer.get(), &ImageViewerV2::setFilterNearest);
    connect(this, &ViewerWidget::setFilterBilinear,      imageViewer.get(), &ImageViewerV2::setFilterBilinear);
    connect(this, &ViewerWidget::setScalingFilter,       imageViewer.get(), &ImageViewerV2::setScalingFilter);
    connect(imageViewer.get(), &ImageViewerV2::nextImageRequested, this, &ViewerWidget::nextImageRequested);
    connect(imageViewer.get(), &ImageViewerV2::prevImageRequested, this, &ViewerWidget::prevImageRequested);

    zoomIndicator = new ZoomIndicatorOverlayProxy(this);
    clickZoneOverlay = new ClickZoneOverlay(this);

    enableImageViewer();
    setInteractionEnabled(true);

    connect(&cursorTimer, &QTimer::timeout, this, &ViewerWidget::hideCursor);

    connect(settings, &Settings::settingsChanged, this, &ViewerWidget::readSettings);
    readSettings();
}

QRect ViewerWidget::imageRect() {
    if(imageViewer && currentWidget == IMAGEVIEWER)
        return imageViewer->scaledRectR();
    else
        return QRect(0,0,0,0);
}

float ViewerWidget::currentScale() {
    if(currentWidget == IMAGEVIEWER)
        return imageViewer->currentScale();
    else
        return 1.0f;
}

QSize ViewerWidget::sourceSize() {
    if(currentWidget == IMAGEVIEWER)
        return imageViewer->sourceSize();
    else
        return QSize(0,0);
}

QRect ViewerWidget::visibleImageRect() const {
    if (imageViewer && currentWidget == IMAGEVIEWER) {
        return imageViewer->visibleImageRect();
    }
    return QRect();
}

QRect ViewerWidget::visibleOriginalImageRect() const {
    if (imageViewer && currentWidget == IMAGEVIEWER) {
        return imageViewer->visibleOriginalImageRect();
    }
    return QRect();
}

QPixmap ViewerWidget::currentScaledPixmapCopy() const {
    if (imageViewer && currentWidget == IMAGEVIEWER) {
        return imageViewer->currentScaledPixmapCopy();
    }
    return QPixmap();
}

float ViewerWidget::getDpr() const {
    if (imageViewer && currentWidget == IMAGEVIEWER) {
        return imageViewer->getDpr();
    }
    return 1.0f;
}

// show imageViewer
void ViewerWidget::enableImageViewer() {
    if(currentWidget != IMAGEVIEWER) {
        imageViewer->show();
        currentWidget = IMAGEVIEWER;
    }
}

void ViewerWidget::disableImageViewer() {
    if(currentWidget == IMAGEVIEWER) {
        currentWidget = UNSET;
        imageViewer->closeImage();
        imageViewer->hide();
        zoomIndicator->hide();
    }
}

void ViewerWidget::onScaleChanged(qreal scale) {
    if(!this->isVisible())
        return;
    if(imageViewer && imageViewer->panoramaMode()) {
        zoomIndicator->hide();
        return;
    }
    zoomIndicator->setScale(scale);
    if(settings->zoomIndicatorMode() == ZoomIndicatorMode::INDICATOR_ENABLED) {
        zoomIndicator->show();
    } else if(scale != 1.0f) {
        if(settings->zoomIndicatorMode() == ZoomIndicatorMode::INDICATOR_AUTO)
            zoomIndicator->show(1500);
    } else {
        zoomIndicator->hide();
    }
}

void ViewerWidget::onAnimationPlaybackFinished() {
    if(currentWidget == IMAGEVIEWER)
        emit playbackFinished();
}

void ViewerWidget::setInteractionEnabled(bool mode) {
    if(mInteractionEnabled == mode)
        return;
    mInteractionEnabled = mode;
    if(mInteractionEnabled) {
        connect(this, &ViewerWidget::toggleLockZoom, imageViewer.get(), &ImageViewerV2::toggleLockZoom);
        connect(this, &ViewerWidget::toggleLockView, imageViewer.get(), &ImageViewerV2::toggleLockView);
        connect(this, &ViewerWidget::zoomIn,         imageViewer.get(), &ImageViewerV2::zoomIn);
        connect(this, &ViewerWidget::zoomOut,        imageViewer.get(), &ImageViewerV2::zoomOut);
        connect(this, &ViewerWidget::zoomInCursor,   imageViewer.get(), &ImageViewerV2::zoomInCursor);
        connect(this, &ViewerWidget::zoomOutCursor,  imageViewer.get(), &ImageViewerV2::zoomOutCursor);
        connect(this, &ViewerWidget::scrollUp,       imageViewer.get(), &ImageViewerV2::scrollUp);
        connect(this, &ViewerWidget::scrollDown,     imageViewer.get(), &ImageViewerV2::scrollDown);
        connect(this, &ViewerWidget::scrollLeft,     imageViewer.get(), &ImageViewerV2::scrollLeft);
        connect(this, &ViewerWidget::scrollRight,    imageViewer.get(), &ImageViewerV2::scrollRight);
        connect(this, &ViewerWidget::fitWindow,      imageViewer.get(), &ImageViewerV2::setFitWindow);
        connect(this, &ViewerWidget::fitWidth,       imageViewer.get(), &ImageViewerV2::setFitWidth);
        connect(this, &ViewerWidget::fitOriginal,    imageViewer.get(), &ImageViewerV2::setFitOriginal);
        connect(this, &ViewerWidget::fitWindowStretch, imageViewer.get(), &ImageViewerV2::setFitWindowStretch);
        connect(imageViewer.get(), &ImageViewerV2::draggedOut, this, &ViewerWidget::draggedOut);
        imageViewer->setAttribute(Qt::WA_TransparentForMouseEvents, false);
    } else {
        disconnect(this, &ViewerWidget::zoomIn,        imageViewer.get(), &ImageViewerV2::zoomIn);
        disconnect(this, &ViewerWidget::zoomOut,       imageViewer.get(), &ImageViewerV2::zoomOut);
        disconnect(this, &ViewerWidget::zoomInCursor,  imageViewer.get(), &ImageViewerV2::zoomInCursor);
        disconnect(this, &ViewerWidget::zoomOutCursor, imageViewer.get(), &ImageViewerV2::zoomOutCursor);
        disconnect(this, &ViewerWidget::scrollUp,      imageViewer.get(), &ImageViewerV2::scrollUp);
        disconnect(this, &ViewerWidget::scrollDown,    imageViewer.get(), &ImageViewerV2::scrollDown);
        disconnect(this, &ViewerWidget::scrollLeft,    imageViewer.get(), &ImageViewerV2::scrollLeft);
        disconnect(this, &ViewerWidget::scrollRight,   imageViewer.get(), &ImageViewerV2::scrollRight);
        disconnect(this, &ViewerWidget::fitWindow,     imageViewer.get(), &ImageViewerV2::setFitWindow);
        disconnect(this, &ViewerWidget::fitWidth,      imageViewer.get(), &ImageViewerV2::setFitWidth);
        disconnect(this, &ViewerWidget::fitOriginal,   imageViewer.get(), &ImageViewerV2::setFitOriginal);
        disconnect(this, &ViewerWidget::fitWindowStretch, imageViewer.get(), &ImageViewerV2::setFitWindowStretch);
        disconnect(imageViewer.get(), &ImageViewerV2::draggedOut, this, &ViewerWidget::draggedOut);
        imageViewer->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        hideContextMenu();
    }
}

bool ViewerWidget::interactionEnabled() {
    return mInteractionEnabled;
}

bool ViewerWidget::showImage(std::unique_ptr<QPixmap> pixmap, QString filePath) {
    if(!pixmap)
        return false;
    enableImageViewer();
    imageViewer->showImage(std::move(pixmap), filePath);
    onScaleChanged(imageViewer->currentScale());
    hideCursorTimed(false);
    return true;
}

bool ViewerWidget::showAnimation(std::shared_ptr<QMovie> movie) {
    if(!movie)
        return false;
    enableImageViewer();
    imageViewer->showAnimation(movie);
    onScaleChanged(imageViewer->currentScale());
    hideCursorTimed(false);
    return true;
}

void ViewerWidget::setFitMode(ImageFitMode mode) {
    if(mode == FIT_WINDOW)
        emit fitWindow();
    else if(mode == FIT_WIDTH)
        emit fitWidth();
    else if(mode == FIT_ORIGINAL)
        emit fitOriginal();
}

ImageFitMode ViewerWidget::fitMode() {
    return imageViewer->fitMode();
}

void ViewerWidget::onScalingFinished(std::unique_ptr<QPixmap> scaled) {
    imageViewer->setScaledPixmap(std::move(scaled));
}

void ViewerWidget::setUpscaledCrop(const QImage &cropImg, QRect origCrop) {
    imageViewer->setUpscaledCrop(cropImg, origCrop);
}

void ViewerWidget::hideUpscaledCrop() {
    if (imageViewer)
        imageViewer->hideUpscaledCrop();
}

void ViewerWidget::closeImage() {
    imageViewer->closeImage();
    showCursor();
}

bool ViewerWidget::isDisplaying() {
    if(currentWidget == IMAGEVIEWER && imageViewer->isDisplaying())
        return true;
    else
        return false;
}

bool ViewerWidget::lockZoomEnabled() {
    return imageViewer->lockZoomEnabled();
}

bool ViewerWidget::lockViewEnabled() {
    return imageViewer->lockViewEnabled();
}

ScalingFilter ViewerWidget::scalingFilter() {
    return imageViewer->scalingFilter();
}

void ViewerWidget::mousePressEvent(QMouseEvent *event) {
    hideContextMenu();
    event->ignore();
}

void ViewerWidget::mouseReleaseEvent(QMouseEvent *event) {
    showCursor();
    hideCursorTimed(false);
    event->ignore();
}

void ViewerWidget::mouseMoveEvent(QMouseEvent *event) {
    if(!(event->buttons() & Qt::LeftButton) && !(event->buttons() & Qt::RightButton)) {
        showCursor();
        hideCursorTimed(true);
    }
    event->ignore();
}

void ViewerWidget::hideCursorTimed(bool restartTimer) {
    if(restartTimer || !cursorTimer.isActive())
        cursorTimer.start(CURSOR_HIDE_TIMEOUT_MS);
}

void ViewerWidget::hideCursor() {
    cursorTimer.stop();
    // ignore if we have something else open like settings window
    if(!isDisplaying() || !isActiveWindow())
        return;
    // ignore when menu is up
    if(contextMenu && contextMenu->isVisible())
        return;
    if(settings->cursorAutohide()) {
        QPoint posMapped = mapFromGlobal(QCursor::pos());
        //if(settings->enableClickZoneThing())
        // ignore when we are hovering the click zone
        if(clickZoneOverlay->leftZone().contains(posMapped) ||
            clickZoneOverlay->leftZone().contains(posMapped))
        {
            return;
        }

        // only hide when we are under viewer widget
        QWidget *w = qApp->widgetAt(QCursor::pos());
        if(w && (w == imageViewer.get()->viewport())) {
            setCursor(QCursor(Qt::BlankCursor));
        }
    }
}

// click zone input crutch
// --
// we can't process mouse events in the overlay
// cause they won't propagate to the ImageViewer, only to overlay's container (this widget)
// so we just grab them before they reach ImageViewer and do the needful
bool ViewerWidget::eventFilter(QObject *object, QEvent *event) {
    // catch press and doubleclick
    // force doubleclick to act as press event for click zones
    if(event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonDblClick) {
        // disable feature for very small windows
        if(width() <= 250)
            return false;

        auto mouseEvent = dynamic_cast<QMouseEvent*>(event);
        if(mouseEvent->button() != Qt::LeftButton || mouseEvent->modifiers()) {
            clickZoneOverlay->disableHighlight();
            return false;
        }
        if(clickZoneOverlay->leftZone().contains(mouseEvent->pos())) {
            clickZoneOverlay->setPressed(true);
            clickZoneOverlay->highlightLeft();
            imageViewer.get()->disableDrags();
            actionManager->invokeAction("prevImage");
            return true; // do not pass the event to imageViewer
        }
        if(clickZoneOverlay->rightZone().contains(mouseEvent->pos())) {
            clickZoneOverlay->setPressed(true);
            clickZoneOverlay->highlightRight();
            imageViewer.get()->disableDrags();
            actionManager->invokeAction("nextImage");
            return true;
        }
    }
    // right click produces QEvent::ContextMenu instead of QEvent::MouseButtonPress
    // this is NOT a QMouseEvent
    if(event->type() == QEvent::ContextMenu) {
        clickZoneOverlay->disableHighlight();
        return false;
    }

    if(event->type() == QEvent::MouseButtonRelease) {
        clickZoneOverlay->setPressed(false);
        imageViewer.get()->enableDrags();
    }

    if(event->type() == QEvent::MouseMove || event->type() == QEvent::Enter) {
        QPoint mousePos;
        if(event->type() == QEvent::MouseMove) {
            auto mouseEvent = dynamic_cast<QMouseEvent*>(event);
            mousePos = mouseEvent->pos();
            if(mouseEvent->buttons())
                return false;
        } else {
            auto enterEvent = dynamic_cast<QEnterEvent*>(event);
            mousePos = enterEvent->pos();
        }
        if(clickZoneOverlay->leftZone().contains(mousePos)) {
            clickZoneOverlay->setPressed(false);
            clickZoneOverlay->highlightLeft();
            setCursor(Qt::PointingHandCursor);
            return true;
        } else if(clickZoneOverlay->rightZone().contains(mousePos)) {
            clickZoneOverlay->setPressed(false);
            clickZoneOverlay->highlightRight();
            setCursor(Qt::PointingHandCursor);
            return true;
        } else {
            clickZoneOverlay->disableHighlight();
            setCursor(Qt::ArrowCursor);
        }
    }

    if(event->type() == QEvent::Leave) {
        clickZoneOverlay->disableHighlight();
        setCursor(Qt::ArrowCursor);
    }

    return false; // send event to imageViewer
}

void ViewerWidget::showCursor() {
    cursorTimer.stop();
    if(cursor().shape() == Qt::BlankCursor)
        setCursor(QCursor(Qt::ArrowCursor));
}

void ViewerWidget::showContextMenu() {
    QPoint pos = cursor().pos();
    showContextMenu(pos);
}

void ViewerWidget::showContextMenu(QPoint pos) {
    if(isVisible() && interactionEnabled()) {
        if(!contextMenu) {
            contextMenu.reset(new ContextMenu(this));
            connect(contextMenu.get(), &ContextMenu::showScriptSettings, this, &ViewerWidget::showScriptSettings);
        }
        contextMenu->setImageEntriesEnabled(isDisplaying());
        contextMenu->setCasSettingsVisible(isDisplaying() && (scalingFilter() == QI_FILTER_CAS));
        if(!contextMenu->isVisible()) {
            QPoint localPos = mapFromGlobal(pos);
            contextMenu->showAt(localPos);
        } else {
            contextMenu->hide();
        }
    }
}

void ViewerWidget::onFullscreenModeChanged(bool mode) {
    imageViewer->onFullscreenModeChanged(mode);
    mIsFullscreen = mode;
}

void ViewerWidget::readSettings() {
    imageViewer->readSettings();
    if(settings->clickableEdges()) {
        imageViewer->viewport()->installEventFilter(this);
        clickZoneOverlay->show();
    } else {
        imageViewer->viewport()->removeEventFilter(this);
        imageViewer.get()->enableDrags();
        clickZoneOverlay->hide();
    }
}

void ViewerWidget::hideContextMenu() {
    if(contextMenu)
        contextMenu->hide();
}

void ViewerWidget::hideEvent(QHideEvent *event) {
    QWidget::hideEvent(event);
    hideContextMenu();
}

// block native tab-switching so we can use it in shortcuts
bool ViewerWidget::focusNextPrevChild(bool mode) {
    return false;
}

void ViewerWidget::keyPressEvent(QKeyEvent *event) {
    event->accept();
    actionManager->processEvent(event);
}

void ViewerWidget::leaveEvent(QEvent *event) {
    QWidget::leaveEvent(event);
}

void ViewerWidget::togglePanorama() {
    imageViewer->togglePanorama();
    if(imageViewer && imageViewer->panoramaMode()) {
        zoomIndicator->hide();
    }
}

void ViewerWidget::setColorAdjustments(float brightness, float contrast, float saturation, float hue, float exposure, float temperature, float tint) {
    if(imageViewer)
        imageViewer->setColorAdjustments(brightness, contrast, saturation, hue, exposure, temperature, tint);
}

void ViewerWidget::updateCasSettings() {
    if(imageViewer)
        imageViewer->updateCasSettings();
}

bool ViewerWidget::isBusyInteracting() const {
    return imageViewer ? imageViewer->isBusyInteracting() : false;
}
