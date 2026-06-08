#include "imageviewerv2.h"
#include "panoramagraphicsitem.h"
#include <QKeyEvent>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOpenGLWidget>
#include <QPainter>

ImageViewerV2::ImageViewerV2(QWidget *parent)
    : QGraphicsView(parent), pixmap(nullptr), pixmapScaled(nullptr),
      movie(nullptr), transparencyGrid(false), expandImage(false),
      keepFitMode(false), loopPlayback(true), mIsFullscreen(false),
      scrollBarWorkaround(true), useFixedZoomLevels(false),
      trackpadDetection(true),
      mouseInteraction(MouseInteractionState::MOUSE_NONE), minScale(0.01f),
      maxScale(40.0f), fitWindowScale(0.125f), fitWindowStretchScale(0.125f),
      mViewLock(LOCK_NONE), imageFitMode(FIT_WINDOW),
      mScalingFilter(QI_FILTER_BILINEAR), imageFitModeDefault(FIT_WINDOW),
      scene(nullptr), zoomTimeLine(nullptr), zoomStartScale(1.0f),
      zoomTargetScale(1.0f) {
  setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
  this->viewport()->setAttribute(Qt::WA_OpaquePaintEvent, false);
  setFocusPolicy(Qt::NoFocus);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  setAcceptDrops(false);

  dpr = this->devicePixelRatioF();
  hs = horizontalScrollBar();
  vs = verticalScrollBar();

  scrollTimeLineY = new QTimeLine(ANIMATION_SPEED, this);
  scrollTimeLineY->setEasingCurve(QEasingCurve::OutSine);
  scrollTimeLineY->setUpdateInterval(SCROLL_UPDATE_RATE);
  scrollTimeLineX = new QTimeLine(ANIMATION_SPEED, this);
  scrollTimeLineX->setEasingCurve(QEasingCurve::OutSine);
  scrollTimeLineX->setUpdateInterval(SCROLL_UPDATE_RATE);
  connect(scrollTimeLineX, &QTimeLine::finished, this,
          &ImageViewerV2::onScrollTimelineFinished);
  connect(scrollTimeLineY, &QTimeLine::finished, this,
          &ImageViewerV2::onScrollTimelineFinished);

  zoomTimeLine = new QTimeLine(ANIMATION_SPEED, this);
  QEasingCurve zoomCurve;
  zoomCurve.setCustomType(smootherstepEasing);
  zoomTimeLine->setEasingCurve(zoomCurve);
  zoomTimeLine->setUpdateInterval(SCROLL_UPDATE_RATE);
  connect(zoomTimeLine, &QTimeLine::valueChanged, this,
          &ImageViewerV2::onZoomTimelineValueChanged);
  connect(zoomTimeLine, &QTimeLine::finished, this,
          &ImageViewerV2::saveViewportPos);
  connect(zoomTimeLine, &QTimeLine::finished, this,
          &ImageViewerV2::requestScaling);

  animationTimer = new QTimer(this);
  animationTimer->setSingleShot(true);

  scaleTimer = new QTimer(this);
  scaleTimer->setSingleShot(true);
  scaleTimer->setInterval(80);

  checkboard = new QPixmap(":res/icons/common/other/checkerboard.png");

  lastTouchpadScroll.start();
  zoomThreshold = static_cast<int>(dpr * 4.);
  gestureThreshold = static_cast<int>(dpr * 40.);

  pixmapItem.setTransformationMode(Qt::SmoothTransformation);
  pixmapItem.setScale(1.0f);
  pixmapItem.setOffset(10000, 10000);
  pixmapItem.setTransformOriginPoint(10000, 10000);
  pixmapItemScaled.setScale(1.0f);
  pixmapItemScaled.setOffset(10000, 10000);
  pixmapItemScaled.setTransformOriginPoint(10000, 10000);
  pixmapItemCrop.setScale(1.0f);
  pixmapItemCrop.setOffset(10000, 10000);
  pixmapItemCrop.setTransformOriginPoint(10000, 10000);

  this->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  this->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  scene = new QGraphicsScene(this);
  scene->setSceneRect(0, 0, 200000, 200000);
  scene->setBackgroundBrush(QColor(60, 60, 103));
  scene->addItem(&pixmapItem);
  scene->addItem(&pixmapItemScaled);
  scene->addItem(&pixmapItemCrop);
  panoramaItem = new PanoramaGraphicsItem();
  scene->addItem(panoramaItem);
  panoramaItem->hide();

  pixmapItemScaled.hide();
  pixmapItemCrop.hide();

  this->setFrameShape(QFrame::NoFrame);
  this->setScene(scene);

  QOpenGLWidget *glViewport = new QOpenGLWidget();
  this->setViewport(glViewport);

  connect(scrollTimeLineX, &QTimeLine::frameChanged, this,
          &ImageViewerV2::scrollToX);
  connect(scrollTimeLineY, &QTimeLine::frameChanged, this,
          &ImageViewerV2::scrollToY);

  connect(hs, &QScrollBar::valueChanged, this, [this]() {
    if (scaleTimer->isActive())
      scaleTimer->stop();
    scaleTimer->start();
    hideUpscaledCrop();
  });
  connect(vs, &QScrollBar::valueChanged, this, [this]() {
    if (scaleTimer->isActive())
      scaleTimer->stop();
    scaleTimer->start();
    hideUpscaledCrop();
  });

  connect(animationTimer, &QTimer::timeout, this,
          &ImageViewerV2::onAnimationTimer, Qt::UniqueConnection);

  QObject::connect(scaleTimer, &QTimer::timeout,
                   [this]() { this->requestScaling(); });

  readSettings();
  connect(settings, &Settings::settingsChanged, this,
          &ImageViewerV2::readSettings);
}

ImageViewerV2::~ImageViewerV2() = default;

// devicePixelRatioF() does not provide correct value on wayland until the first
// paint event occurs catch change event & do the needful
bool ImageViewerV2::event(QEvent *ev) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
  if (ev->type() == QEvent::DevicePixelRatioChange) {
    onDPRChanged();
  }
#endif
  return QGraphicsView::event(ev);
}

void ImageViewerV2::onDPRChanged() {
  if (dpr == this->devicePixelRatioF())
    return;
  qDebug() << "DPR CHANGED " << dpr << " >> " << this->devicePixelRatioF();
  dpr = this->devicePixelRatioF();
  zoomThreshold = static_cast<int>(dpr * 4.);
  gestureThreshold = static_cast<int>(dpr * 40.);
  if (pixmap) {
    pixmap->setDevicePixelRatio(dpr);
    pixmapItem.setPixmap(*pixmap);
    if (!mSvgMode) {
      pixmapItem.show();
    }
    pixmapItem.update();
    updateMinScale();
    applyFitMode();
    requestScaling();
    update();
  }
}

void ImageViewerV2::readSettings() {
  transparencyGrid = settings->transparencyGrid();
  expandImage = settings->expandImage();
  expandLimit = static_cast<float>(settings->expandLimit());
  if (expandLimit < 1.0f)
    expandLimit = maxScale;
  keepFitMode = settings->keepFitMode();
  imageFitModeDefault = settings->imageFitMode();
  zoomStep = settings->zoomStep();
  focusIn1to1 = settings->focusPointIn1to1Mode();
  trackpadDetection = settings->trackpadDetection();
  if ((useFixedZoomLevels = settings->useFixedZoomLevels())) {
    // zoomlevels are stored as a string, parse into list
    zoomLevels.clear();
    auto levelsStr = settings->zoomLevels().split(',');
    for (const auto &i : std::as_const(levelsStr))
      zoomLevels.append(i.toFloat());
    std::sort(zoomLevels.begin(), zoomLevels.end());
  }
  // set bg color
  onFullscreenModeChanged(mIsFullscreen);
  updateMinScale();
  setScalingFilter(settings->scalingFilter());
  updateCasSettings();
  if (isDisplaying()) {
    if (imageFitMode == FIT_FREE) {
      if (currentScale() < minScale) {
        doZoom(minScale);
        centerIfNecessary();
        snapToEdges();
      }
      requestScaling();
    } else {
      applyFitMode();
      requestScaling();
    }
  } else {
    setFitMode(imageFitModeDefault);
  }
}

void ImageViewerV2::onFullscreenModeChanged(bool mode) {
  QColor bgColor;
  mIsFullscreen = mode;
  if (mode) {
    bgColor = settings->colorScheme().background_fullscreen;
    bgColor.setAlphaF(1.0);
  } else {
    bgColor = settings->colorScheme().background;
    bgColor.setAlphaF(settings->backgroundOpacity());
  }
  scene->setBackgroundBrush(bgColor);
  this->viewport()->setAttribute(Qt::WA_OpaquePaintEvent,
                                 (bgColor.alpha() == 255));
}

void ImageViewerV2::startAnimation() {
  if (movie && movie->frameCount() > 1) {
    stopAnimation();
    emit animationPaused(false);
    // movie->jumpToFrame(0);
    // emit frameChanged(0);
    animationTimer->start(movie->nextFrameDelay());
  }
}

void ImageViewerV2::stopAnimation() {
  if (movie) {
    emit animationPaused(true);
    animationTimer->stop();
  }
}

void ImageViewerV2::pauseResume() {
  if (movie) {
    if (animationTimer->isActive())
      stopAnimation();
    else
      startAnimation();
  }
}

void ImageViewerV2::enableDrags() { dragsEnabled = true; }

void ImageViewerV2::disableDrags() { dragsEnabled = false; }

void ImageViewerV2::onAnimationTimer() {
  if (!movie)
    return;
  if (movie->currentFrameNumber() == movie->frameCount() - 1) {
    // last frame
    if (!loopPlayback) {
      emit animationPaused(true);
      emit playbackFinished();
      return;
    } else {
      movie->jumpToFrame(0);
    }
  } else {
    if (!movie->jumpToNextFrame()) {
      qDebug() << "[Error] QMovie:" << movie->lastErrorString();
      this->stopAnimation();
      return;
    }
  }
  emit frameChanged(movie->currentFrameNumber());
  std::unique_ptr<QPixmap> newFrame(new QPixmap());
  *newFrame = movie->currentPixmap();
  updatePixmap(std::move(newFrame));
  animationTimer->start(movie->nextFrameDelay());
}

void ImageViewerV2::nextFrame() {
  if (!movie) {
    return;
  } else if (movie->currentFrameNumber() == movie->frameCount() - 1) {
    showAnimationFrame(0);
  } else {
    showAnimationFrame(movie->currentFrameNumber() + 1);
  }
}

void ImageViewerV2::prevFrame() {
  if (!movie) {
    return;
  } else if (movie->currentFrameNumber() == 0) {
    showAnimationFrame(movie->frameCount() - 1);
  } else {
    showAnimationFrame(movie->currentFrameNumber() - 1);
  }
}

bool ImageViewerV2::showAnimationFrame(int frame) {
  if (!movie || frame < 0 || frame >= movie->frameCount())
    return false;
  if (movie->currentFrameNumber() == frame)
    return true;
  // at the first glance this may seem retarded
  // because it is
  // unfortunately i dont see a *better* way to do seeking with QMovie
  // QMovie::CacheAll is buggy and memory inefficient
  if (frame < movie->currentFrameNumber())
    movie->jumpToFrame(0);
  while (frame != movie->currentFrameNumber()) {
    if (!movie->jumpToNextFrame()) {
      qDebug() << "[Error] QMovie:" << movie->lastErrorString();
      break;
    }
  }
  emit frameChanged(movie->currentFrameNumber());
  std::unique_ptr<QPixmap> newFrame(new QPixmap());
  *newFrame = movie->currentPixmap();
  updatePixmap(std::move(newFrame));
  return true;
}

void ImageViewerV2::updatePixmap(std::unique_ptr<QPixmap> newPixmap) {
  pixmap = std::move(newPixmap);
  pixmap->setDevicePixelRatio(dpr);
  pixmapItem.setPixmap(*pixmap);
  pixmapItem.update();
  if (mPanoramaMode) {
    panoramaItem->setPixmap(pixmap);
    panoramaItem->show();
    pixmapItem.hide();
    if (svgItem) {
      svgItem->hide();
    }
  } else if (mSvgMode) {
    panoramaItem->hide();
    pixmapItem.hide();
    pixmapItemScaled.hide();
    if (svgItem) {
      svgItem->show();
      svgItem->setScale(pixmapItem.scale());
    }
  } else {
    panoramaItem->hide();
    if (svgItem) {
      svgItem->hide();
    }
    pixmapItem.show();
  }
}

void ImageViewerV2::showAnimation(std::shared_ptr<QMovie> _movie) {
  if (_movie && _movie->isValid()) {
    // Update DPR just in case an event was missed or not delivered yet
    float newDpr = this->devicePixelRatioF();
    if (dpr != newDpr) {
      dpr = newDpr;
      zoomThreshold = static_cast<int>(dpr * 4.);
      gestureThreshold = static_cast<int>(dpr * 40.);
    }
    reset();
    movie = _movie;
    movie->jumpToFrame(0);
    Qt::TransformationMode mode = selectTransformationMode();
    pixmapItem.setTransformationMode(mode);
    std::unique_ptr<QPixmap> newFrame(new QPixmap());
    *newFrame = movie->currentPixmap();
    updatePixmap(std::move(newFrame));
    emit durationChanged(movie->frameCount());
    emit frameChanged(0);

    updateMinScale();
    if (!keepFitMode || imageFitMode == FIT_FREE)
      imageFitMode = imageFitModeDefault;

    if (mViewLock == LOCK_NONE) {
      applyFitMode();
    } else {
      imageFitMode = FIT_FREE;
      fitFree(lockedScale);
      if (mViewLock == LOCK_ALL)
        applySavedViewportPos();
    }
    startAnimation();
  }
}

void ImageViewerV2::showImage(std::unique_ptr<QPixmap> _pixmap,
                              QString filePath) {
  if (_pixmap && !_pixmap->isNull()) {
    // Update DPR just in case an event was missed or not delivered yet
    float newDpr = this->devicePixelRatioF();
    if (dpr != newDpr) {
      dpr = newDpr;
      zoomThreshold = static_cast<int>(dpr * 4.);
      gestureThreshold = static_cast<int>(dpr * 40.);
    }
    bool isSameFile = (!filePath.isEmpty() && filePath == currentFilePath);
    QSize oldSize = sourceSize();
    QSize newSize = _pixmap->size();
    bool isRotationOrMirror = (isSameFile && oldSize.isValid() &&
                               (oldSize.width() * oldSize.height() ==
                                newSize.width() * newSize.height()));

    float prevScale = currentScale();
    ImageFitMode prevFitMode = imageFitMode;
    QPointF relativePos(0.5, 0.5);

    if (isRotationOrMirror && isDisplaying()) {
      QGraphicsPixmapItem *item = &pixmapItem;
      QPointF sceneCenter =
          mapToScene(viewport()->rect().center()) + QPointF(1, 1);
      auto itemRect = item->sceneBoundingRect();
      if (itemRect.width() > 0 && itemRect.height() > 0) {
        relativePos.setX(qBound(
            qreal(0), (sceneCenter.x() - itemRect.left()) / itemRect.width(),
            qreal(1)));
        relativePos.setY(qBound(
            qreal(0), (sceneCenter.y() - itemRect.top()) / itemRect.height(),
            qreal(1)));
      }
    }

    reset();

    currentFilePath = filePath;

    if (filePath.endsWith(".svg", Qt::CaseInsensitive)) {
      svgItem = new QGraphicsSvgItem(filePath);
      if (svgItem->renderer() && svgItem->renderer()->isValid()) {
        svgItem->setPos(10000, 10000);
        svgItem->setCacheMode(QGraphicsItem::NoCache);
        scene->addItem(svgItem);
        mSvgMode = true;
      } else {
        mSvgMode = false;
        delete svgItem;
        svgItem = nullptr;
      }
    } else {
      mSvgMode = false;
    }

    updatePixmap(std::move(_pixmap));
    updateMinScale();

    if (isRotationOrMirror) {
      imageFitMode = prevFitMode;
      if (imageFitMode == FIT_FREE) {
        doZoom(prevScale);
      } else {
        applyFitMode();
      }

      // Restore relative viewport center
      QGraphicsPixmapItem *item = &pixmapItem;
      auto itemRect = item->sceneBoundingRect();
      QPointF newScenePos;
      newScenePos.setX(itemRect.left() + itemRect.width() * relativePos.x());
      newScenePos.setY(itemRect.top() + itemRect.height() * relativePos.y());
      centerOn(newScenePos);
      centerIfNecessary();
      snapToEdges();
    } else {
      if (!keepFitMode || imageFitMode == FIT_FREE)
        imageFitMode = imageFitModeDefault;

      if (mViewLock == LOCK_NONE) {
        applyFitMode();
      } else {
        imageFitMode = FIT_FREE;
        fitFree(lockedScale);
        if (mViewLock == LOCK_ALL)
          applySavedViewportPos();
      }
    }
    requestScaling();
    update();
  }
}

// reset state, remove image & stop animation
void ImageViewerV2::reset() {
  stopPosAnimation();
  pixmapItemScaled.setPixmap(QPixmap());
  pixmapItemCrop.setPixmap(QPixmap());
  pixmapScaled.reset(nullptr);
  pixmapItem.setPixmap(QPixmap());
  pixmapItem.setScale(1.0f);
  pixmapItem.setOffset(10000, 10000);
  if (svgItem) {
    scene->removeItem(svgItem);
    delete svgItem;
    svgItem = nullptr;
  }
  pixmap.reset();
  stopAnimation();
  movie = nullptr;
  centerOn(10000, 10000);
  // when this view is not in focus this it won't update the background
  // so we force it here
  viewport()->update();
  panoramaItem->setPixmap(nullptr);
  panoramaItem->hide();
  pixmapItem.show();
  pixmapItemScaled.hide();
  pixmapItemCrop.hide();
  currentFilePath = "";
}

void ImageViewerV2::closeImage() { reset(); }

void ImageViewerV2::setScaledPixmap(std::unique_ptr<QPixmap> newFrame) {
  if (!movie && newFrame->size() != scaledSizeR() * dpr)
    return;
  pixmapScaled = std::move(newFrame);
  pixmapScaled->setDevicePixelRatio(dpr);
  pixmapItemScaled.setPixmap(*pixmapScaled);
  pixmapItem.hide();
  pixmapItemScaled.show();
}

bool ImageViewerV2::isDisplaying() const { return (pixmap != nullptr); }

void ImageViewerV2::scrollUp() { scroll(0, -DEFAULT_SCROLL_DISTANCE, true); }

void ImageViewerV2::scrollDown() { scroll(0, DEFAULT_SCROLL_DISTANCE, true); }

void ImageViewerV2::scrollLeft() { scroll(-DEFAULT_SCROLL_DISTANCE, 0, true); }

void ImageViewerV2::scrollRight() { scroll(DEFAULT_SCROLL_DISTANCE, 0, true); }

// temporary override till application restart
void ImageViewerV2::toggleTransparencyGrid() {
  transparencyGrid = !transparencyGrid;
  scene->update();
}

void ImageViewerV2::setScalingFilter(ScalingFilter filter) {
  if (mScalingFilter == filter)
    return;
  mScalingFilter = filter;

  if (!mSvgMode) {
    pixmapItem.show();
  }
  pixmapItem.setTransformationMode(selectTransformationMode());

  if (mScalingFilter == QI_FILTER_NEAREST || mScalingFilter == QI_FILTER_CAS)
    swapToOriginalPixmap();
  requestScaling();
}

void ImageViewerV2::setLoopPlayback(bool mode) {
  if (movie && mode && loopPlayback != mode)
    startAnimation();
  loopPlayback = mode;
}

void ImageViewerV2::setFilterNearest() {
  if (mScalingFilter != QI_FILTER_NEAREST) {
    mScalingFilter = QI_FILTER_NEAREST;
    pixmapItem.setTransformationMode(selectTransformationMode());
    swapToOriginalPixmap();
    requestScaling();
  }
}

void ImageViewerV2::setFilterBilinear() {
  if (mScalingFilter != QI_FILTER_BILINEAR) {
    mScalingFilter = QI_FILTER_BILINEAR;
    pixmapItem.setTransformationMode(selectTransformationMode());
    requestScaling();
  }
}

// returns a mode based on current zoom level and a bunch of toggles
Qt::TransformationMode ImageViewerV2::selectTransformationMode() {
  if (mScalingFilter == QI_FILTER_NEAREST) {
    return Qt::FastTransformation;
  }
  return Qt::SmoothTransformation;
}

void ImageViewerV2::setExpandImage(bool mode) {
  expandImage = mode;
  updateMinScale();
  applyFitMode();
  requestScaling();
}

void ImageViewerV2::show() {
  setMouseTracking(false);
  QGraphicsView::show();
  setMouseTracking(true);
}

void ImageViewerV2::hide() {
  setMouseTracking(false);
  QWidget::hide();
}

void ImageViewerV2::requestScaling() {
  bool isAt100 = std::abs(pixmapItem.scale() - 1.0) < 0.001;
  if (mSvgMode || !pixmap || (isAt100 && !settings->applyFilterAt100()) ||
      (mScalingFilter == QI_FILTER_CAS && !(settings->useUpscayl() && pixmapItem.scale() > 1.0)) ||
      movie || (zoomTimeLine && zoomTimeLine->state() == QTimeLine::Running) ||
      mouseInteraction == MouseInteractionState::MOUSE_ZOOM ||
      mouseInteraction == MouseInteractionState::MOUSE_WHEEL_ZOOM) {
    return;
  }

  QSize targetSize = scaledSizeR() * dpr;

  // Default limits (same as ImageLib::scaled)
  int maxDim = 12288;
  qint64 maxPixels = 100000000; // 100 megapixels
  float maxScale = 4.0f;

#ifdef USE_UPSCAYL
  if (settings->useUpscayl()) {
    maxScale = 40.0f; // allow extreme zoom with Upscayl (up to 4000%)
    if (currentScale() > maxScale) {
      return;
    }
  } else
#endif
  {
    if (currentScale() > maxScale || targetSize.width() > maxDim ||
        targetSize.height() > maxDim ||
        (qint64)targetSize.width() * targetSize.height() > maxPixels) {
      return;
    }
  }

  if (scaleTimer->isActive())
    scaleTimer->stop();

  emit scalingRequested(scaledSizeR() * dpr, mScalingFilter);
}

bool ImageViewerV2::imageFits() const {
  if (!pixmap)
    return true;
  return (pixmap->width() <= (viewport()->width() * dpr) &&
          pixmap->height() <= (viewport()->height() * dpr));
}

bool ImageViewerV2::scaledImageFits() const {
  if (!pixmap)
    return true;
  QSize sz = scaledSizeR();
  return (sz.width() <= viewport()->width() &&
          sz.height() <= viewport()->height());
}

ScalingFilter ImageViewerV2::scalingFilter() const { return mScalingFilter; }

QWidget *ImageViewerV2::widget() { return this; }

bool ImageViewerV2::hasAnimation() const { return (movie != nullptr); }

//  Right button zooming / dragging logic
//  mouseMoveStartPos: stores the previous mouseMoveEvent() position,
//                     used to calculate delta.
//  mousePressPos: used to filter out accidental zoom events
//  mouseInteraction: tracks which action we are performing since the last
//  mousePressEvent()
//
void ImageViewerV2::mousePressEvent(QMouseEvent *event) {
  if (!pixmap) {
    QWidget::mousePressEvent(event);
    return;
  }
  mouseMoveStartPos = event->pos();
  mousePressPos = mouseMoveStartPos;
  if (event->button() & Qt::RightButton) {
    setZoomAnchor(event->pos());
  } else {
    QGraphicsView::mousePressEvent(event);
  }
}

void ImageViewerV2::mouseMoveEvent(QMouseEvent *event) {
  QWidget::mouseMoveEvent(event);
  if (mIsFullscreen) {
    viewport()->update();
  }
  if (mPanoramaMode && (event->buttons() & Qt::LeftButton)) {
    int dx = event->pos().x() - mouseMoveStartPos.x();
    int dy = event->pos().y() - mouseMoveStartPos.y();
    float speedMultiplier = 2.0f;
    mPanoramaYaw -=
        (float)dx * mPanoramaFov / viewport()->width() * speedMultiplier;
    mPanoramaPitch -=
        (float)dy * mPanoramaFov / viewport()->width() * speedMultiplier;

    if (mPanoramaPitch > 89.0f)
      mPanoramaPitch = 89.0f;
    if (mPanoramaPitch < -89.0f)
      mPanoramaPitch = -89.0f;

    panoramaItem->setViewParameters(mPanoramaYaw, mPanoramaPitch, mPanoramaFov);
    mouseMoveStartPos = event->pos();
    return;
  }

  if (!pixmap || mouseInteraction == MouseInteractionState::MOUSE_DRAG ||
      mouseInteraction == MouseInteractionState::MOUSE_WHEEL_ZOOM)
    return;

  if (event->buttons() & Qt::LeftButton) {
    // ---------------- DRAG / PAN -------------------
    // select which action to start
    if (mouseInteraction == MouseInteractionState::MOUSE_NONE) {
      if (scaledImageFits()) {
        if (dragsEnabled)
          mouseInteraction = MouseInteractionState::MOUSE_DRAG_BEGIN;
      } else {
        mouseInteraction = MouseInteractionState::MOUSE_PAN;
        if (cursor().shape() != Qt::ClosedHandCursor)
          setCursor(Qt::ClosedHandCursor);
      }
    }
    // emit a signal to start dnd; set flag to ignore further mouse move events
    if (mouseInteraction == MouseInteractionState::MOUSE_DRAG_BEGIN) {
      if ((abs(mousePressPos.x() - event->pos().x()) > dragThreshold) ||
          abs(mousePressPos.y() - event->pos().y()) > dragThreshold) {
        mouseInteraction = MouseInteractionState::MOUSE_NONE;
        emit draggedOut();
      }
    }
    // panning
    if (mouseInteraction == MouseInteractionState::MOUSE_PAN) {
      mousePan(event);
    }
    return;
  } else if (event->buttons() & Qt::RightButton) {
    // ------------------- ZOOM / GESTURE ----------------------
    if (mouseInteraction == MouseInteractionState::MOUSE_NONE) {
      int dx = event->pos().x() - mousePressPos.x();
      int dy = event->pos().y() - mousePressPos.y();

      // wait for some movement to decide direction
      if (abs(dx) > zoomThreshold || abs(dy) > zoomThreshold) {
        if (abs(dx) > abs(dy) * 2) {
          // horizontal movement is dominant: prioritize gesture
          if (abs(dx) > gestureThreshold) {
            mouseInteraction = MouseInteractionState::MOUSE_GESTURE;
            if (dx < 0)
              emit nextImageRequested();
            else
              emit prevImageRequested();
            return;
          }
        } else if (abs(dy) > zoomThreshold) {
          // vertical movement is dominant: prioritize zoom
          if (cursor().shape() != Qt::SizeVerCursor) {
            setCursor(Qt::SizeVerCursor);
          }
          mouseInteraction = MouseInteractionState::MOUSE_ZOOM;
        }
      }
      return;
    }

    if (mouseInteraction == MouseInteractionState::MOUSE_ZOOM) {
      mouseMoveZoom(event);
    }
    return;
  } else {
    event->ignore();
  }
}

void ImageViewerV2::mouseReleaseEvent(QMouseEvent *event) {
  unsetCursor();
  bool needScale =
      (mouseInteraction == MouseInteractionState::MOUSE_ZOOM ||
       mouseInteraction == MouseInteractionState::MOUSE_WHEEL_ZOOM ||
       mouseInteraction == MouseInteractionState::MOUSE_PAN);

  if (!pixmap || mouseInteraction == MouseInteractionState::MOUSE_NONE) {
    QGraphicsView::mouseReleaseEvent(event);
    event->ignore();
  }
  mouseInteraction = MouseInteractionState::MOUSE_NONE;
  if (needScale) {
    requestScaling();
  }
}

// warning for future me:
// for some reason in qgraphicsview wheelEvent is followed by moveEvent (wtf?)
void ImageViewerV2::wheelEvent(QWheelEvent *event) {
  if (mPanoramaMode) {
    int delta = event->angleDelta().y();
    mPanoramaFov *= (delta > 0) ? 0.9f : 1.1f;
    if (mPanoramaFov < 10.0f)
      mPanoramaFov = 10.0f;
    if (mPanoramaFov > 140.0f)
      mPanoramaFov = 140.0f;
    panoramaItem->setViewParameters(mPanoramaYaw, mPanoramaPitch, mPanoramaFov);
    event->accept();
    return;
  }

  if (event->buttons() & Qt::RightButton) {
    event->accept();
    mouseInteraction = MOUSE_WHEEL_ZOOM;
    int angleDelta = event->angleDelta().ry();
    if (angleDelta > 0)
      zoomInCursor();
    else if (angleDelta < 0)
      zoomOutCursor();
  } else if (event->modifiers() == Qt::NoModifier) {
    QPoint pixelDelta = event->pixelDelta();
    QPoint angleDelta = event->angleDelta();
    /* for reference
     * linux
     *   trackpad/xorg:
     *     pixelDelta = (x,y) OR (0,0)
     *     angleDelta = (x*scale,y*scale) OR (x,y)
     *   trackpad/wayland:
     *     pixelDelta = (x,y)
     *     angleDelta = (x*scale,y*scale)
     *   wheel:
     *     pixelDelta = (0,0)     - libinput <= 1.18
     *     pixelDelta = (0,120*m) - libinput 1.19
     *     angleDelta = (0,120*m)
     * -----------------------------------------
     * macOS
     *   trackpad:
     *     pixelDelta = (x,y)
     *     angleDelta = (x*scale,y*scale)
     *   wheel:
     *     pixelDelta = (0,y*scrollAccel)
     *     angleDelta = (0,120*m)
     * -----------------------------------------
     * windows
     *   trackpad:
     *     pixelDelta = (0,0)
     *     angleDelta = (x,y)
     *   wheel:
     *     pixelDelta = (0,0)
     *     AngleDelta = (0,120*m)
     */

    bool isWheel = true;
    if (trackpadDetection) {
      // fallback to guesswork
      isWheel = angleDelta.y() &&
                (abs(angleDelta.y()) >= 120 && !(angleDelta.y() % 60)) &&
                lastTouchpadScroll.elapsed() > 250;
    }

    if (!isWheel) {
      lastTouchpadScroll.restart();
      event->accept();
      if (settings->imageScrolling() != ImageScrolling::SCROLL_NONE) {
        // scroll (high precision)
        stopPosAnimation();
        // one of these (pixel/angleDelta) may be multiplied by some scale value
        // we'll use whichever is larger
        int dx = abs(angleDelta.x()) > abs(pixelDelta.x()) ? angleDelta.x()
                                                           : pixelDelta.x();
        int dy = abs(angleDelta.y()) > abs(pixelDelta.y()) ? angleDelta.y()
                                                           : pixelDelta.y();
        hs->setValue(hs->value() - dx * TRACKPAD_SCROLL_MULTIPLIER);
        vs->setValue(vs->value() - dy * TRACKPAD_SCROLL_MULTIPLIER);
        centerIfNecessary();
        snapToEdges();
      }
    } else if (isWheel &&
               settings->imageScrolling() == SCROLL_BY_TRACKPAD_AND_WHEEL) {
      // scroll by interval
      bool scrollable = false;
      QRect imgRect = scaledRectR();
      // shift by 2px in case of img edge misalignment
      // todo: maybe even increase it to skip small distance scrolls?
      if ((event->angleDelta().y() < 0 && imgRect.bottom() > height() + 2) ||
          (event->angleDelta().y() > 0 && imgRect.top() < -2)) {
        event->accept();
        scroll(0,
               -angleDelta.y() * WHEEL_SCROLL_MULTIPLIER *
                   settings->mouseScrollingSpeed(),
               true);
      } else {
        event->ignore(); // not scrollable; passthrough event
      }
    } else {
      event->ignore();
      QWidget::wheelEvent(event);
    }
    saveViewportPos();
  } else {
    event->ignore();
    QWidget::wheelEvent(event);
  }
}

void ImageViewerV2::showEvent(QShowEvent *event) {
  QGraphicsView::showEvent(event);
  // ensure we are properly resized
  qApp->processEvents();
  // reapply fitmode to fix viewport position
  if (imageFitMode == FIT_ORIGINAL)
    applyFitMode();
}

void ImageViewerV2::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Left || event->key() == Qt::Key_Right ||
      event->key() == Qt::Key_Up || event->key() == Qt::Key_Down) {
    event->ignore();
    return;
  }
  QGraphicsView::keyPressEvent(event);
}

void ImageViewerV2::drawBackground(QPainter *painter, const QRectF &rect) {
  if (QOpenGLWidget *glWidget = qobject_cast<QOpenGLWidget *>(viewport())) {
    painter->beginNativePainting();
    if (QOpenGLContext *ctx = QOpenGLContext::currentContext()) {
      if (QOpenGLFunctions *f = ctx->functions()) {
        f->glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        f->glClear(GL_COLOR_BUFFER_BIT);
      }
    }
    painter->endNativePainting();
  }

  QGraphicsView::drawBackground(painter, rect);
  if (!isDisplaying() || !transparencyGrid || !pixmap->hasAlphaChannel())
    return;
  painter->drawTiledPixmap(pixmapItem.sceneBoundingRect(), *checkboard);
}

// simple pan behavior (cursor stops at the screen edges)
inline void ImageViewerV2::mousePan(QMouseEvent *event) {
  if (scaledImageFits())
    return;
  mouseMoveStartPos -= event->pos();
  scroll(mouseMoveStartPos.x(), mouseMoveStartPos.y(), false);
  mouseMoveStartPos = event->pos();
  saveViewportPos();
}

//  zooming while the right button is pressed
//  note: on reaching min zoom level the fitMode is set to FIT_WINDOW;
//        mid-zoom it is set to FIT_FREE.
//        FIT_FREE mode does not persist when changing images.
inline void ImageViewerV2::mouseMoveZoom(QMouseEvent *event) {
  float stepMultiplier = 0.003f; // this one feels ok
  int currentPos = event->pos().y();
  int moveDistance = mouseMoveStartPos.y() - currentPos;

  if (mPanoramaMode) {
    // In panorama mode, we decrease FOV to zoom in (moving mouse up decreases
    // currentPos, increases moveDistance)
    mPanoramaFov *= (1.0f - stepMultiplier * moveDistance * dpr);
    if (mPanoramaFov < 10.0f)
      mPanoramaFov = 10.0f;
    if (mPanoramaFov > 140.0f)
      mPanoramaFov = 140.0f;
    panoramaItem->setViewParameters(mPanoramaYaw, mPanoramaPitch, mPanoramaFov);
    mouseMoveStartPos = event->pos();
    return;
  }

  float newScale =
      currentScale() * (1.0f + stepMultiplier * moveDistance * dpr);
  mouseMoveStartPos = event->pos();
  imageFitMode = FIT_FREE;

  zoomAnchored(newScale);
  centerIfNecessary();
  snapToEdges();
  if (pixmapItem.scale() == fitWindowScale)
    imageFitMode = FIT_WINDOW;
  else if (pixmapItem.scale() == fitWindowStretchScale)
    imageFitMode = FIT_WINDOW_STRETCH;
}

// scale at which current image fills the window
void ImageViewerV2::updateFitWindowScale() {
  float scaleFitX = (float)viewport()->width() * dpr / pixmap->width();
  float scaleFitY = (float)viewport()->height() * dpr / pixmap->height();
  if (scaleFitX < scaleFitY) {
    fitWindowScale = scaleFitX;
  } else {
    fitWindowScale = scaleFitY;
  }
  if (expandImage && fitWindowScale > expandLimit)
    fitWindowScale = expandLimit;
}

void ImageViewerV2::updateFitWindowStretchScale() {
  if (!pixmap)
    return;

  float scaleFitY = (float)viewport()->height() * dpr / pixmap->height();

  // For "Fit in window (stretch)", we always use height-based scaling
  // This ensures the full image is always visible while stretching to fill the
  // window height
  fitWindowStretchScale = scaleFitY;

  if (expandImage && fitWindowStretchScale > expandLimit)
    fitWindowStretchScale = expandLimit;
}

void ImageViewerV2::updateMinScale() {
  if (!pixmap)
    return;
  updateFitWindowScale();
  updateFitWindowStretchScale();
  if (settings->unlockMinZoom()) {
    if (!pixmap->isNull())
      minScale = qMax(10. / pixmap->width(), 10. / pixmap->height());
    else
      minScale = 1.0f;
  } else {
    if (imageFits())
      minScale = 1.0f;
    else
      minScale = fitWindowScale;
  }
  if (mViewLock != LOCK_NONE && lockedScale < minScale)
    minScale = lockedScale;
}

void ImageViewerV2::fitWidth() {
  if (!pixmap)
    return;
  float scaleX = (float)viewport()->width() * dpr / pixmap->width();
  if (!expandImage && scaleX > 1.0f)
    scaleX = 1.0f;
  if (scaleX > expandLimit)
    scaleX = expandLimit;
  if (currentScale() != scaleX) {
    swapToOriginalPixmap();
    doZoom(scaleX);
  }
  centerIfNecessary();
  // just center somewhere at the top then do snap
  if (scaledSizeR().height() > viewport()->height()) {
    QPointF centerTarget =
        mapToScene(viewport()->rect()).boundingRect().center();
    centerTarget.setY(0);
    centerOn(centerTarget);
  }
  snapToEdges();
}

void ImageViewerV2::fitWindow() {
  if (!pixmap)
    return;
  if (imageFits() && !expandImage) {
    fitNormal();
  } else {
    if (currentScale() != fitWindowScale) {
      swapToOriginalPixmap();
      doZoom(fitWindowScale);
    }
    // There's either a qt bug or I am misusing something.
    // First call to scrollbar->setValue() produces wrong results
    // - unless when called from eventloop
    if (scrollBarWorkaround) {
      scrollBarWorkaround = false;
      QTimer::singleShot(0, this, SLOT(centerOnPixmap()));
    } else {
      centerOnPixmap();
    }
  }
}

void ImageViewerV2::fitNormal() { fitFree(1.0f); }

void ImageViewerV2::fitWindowStretch() {
  if (!pixmap)
    return;

  // Update the stretch scale calculation
  updateFitWindowStretchScale();

  if (currentScale() != fitWindowStretchScale) {
    swapToOriginalPixmap();
    doZoom(fitWindowStretchScale);
  }

  // Handle scrollbar workaround similar to fitWindow()
  if (scrollBarWorkaround) {
    scrollBarWorkaround = false;
    QTimer::singleShot(0, this, SLOT(centerOnPixmap()));
  } else {
    centerOnPixmap();
  }
}

void ImageViewerV2::fitFree(float scale) {
  if (!pixmap)
    return;
  if (focusIn1to1 == FOCUS_TOP) {
    doZoom(scale);
    centerIfNecessary();
    if (scaledSizeR().height() > viewport()->height()) {
      QPointF centerTarget = pixmapItem.sceneBoundingRect().center();
      centerTarget.setY(0);
      centerOn(centerTarget);
    }
    snapToEdges();
  } else {
    if (focusIn1to1 == FOCUS_CENTER)
      setZoomAnchor(viewport()->rect().center());
    else
      setZoomAnchor(mapFromGlobal(cursor().pos()));
    zoomAnchored(scale);
    centerIfNecessary();
    snapToEdges();
  }
}

void ImageViewerV2::applyFitMode() {
  switch (imageFitMode) {
  case FIT_ORIGINAL:
    fitNormal();
    break;
  case FIT_WIDTH:
    fitWidth();
    break;
  case FIT_WINDOW:
    fitWindow();
    break;
  case FIT_WINDOW_STRETCH:
    fitWindowStretch();
    break;
  default:
    break;
  }
}

// public, sends scale request
void ImageViewerV2::setFitMode(ImageFitMode newMode) {
  if (scaleTimer->isActive())
    scaleTimer->stop();
  stopPosAnimation();
  imageFitMode = newMode;
  applyFitMode();
  requestScaling();
}

// public, sends scale request
void ImageViewerV2::setFitOriginal() { setFitMode(FIT_ORIGINAL); }

// public, sends scale request
void ImageViewerV2::setFitWidth() {
  setFitMode(FIT_WIDTH);
  requestScaling();
}

// public, sends scale request
void ImageViewerV2::setFitWindow() {
  setFitMode(FIT_WINDOW);
  requestScaling();
}

// public, sends scale request
void ImageViewerV2::setFitWindowStretch() {
  setFitMode(FIT_WINDOW_STRETCH);
  requestScaling();
}

void ImageViewerV2::resizeEvent(QResizeEvent *event) {
  QGraphicsView::resizeEvent(event);
  // reset this so we won't generate unnecessary drag'n'drop event
  mousePressPos = mapFromGlobal(cursor().pos());
  // Qt emits some unnecessary resizeEvents on startup
  // so we try to ignore them
  if (parentWidget()->isVisible()) {
    stopPosAnimation();
    updateMinScale();
    if (imageFitMode == FIT_FREE || imageFitMode == FIT_ORIGINAL) {
      centerIfNecessary();
      snapToEdges();
    } else {
      applyFitMode();
    }
    update();
    if (scaleTimer->isActive())
      scaleTimer->stop();
    scaleTimer->start();
    saveViewportPos();
  }
}

void ImageViewerV2::centerOnPixmap() {
  auto imgRect = pixmapItem.sceneBoundingRect();
  auto vport = mapToScene(viewport()->geometry()).boundingRect();
  hs->setValue(pixmapItem.offset().x() -
               (int)(vport.width() - imgRect.width()) / 2);
  vs->setValue(pixmapItem.offset().y() -
               (int)(vport.height() - imgRect.height()) / 2);
}

void ImageViewerV2::stopPosAnimation() {
  if (scrollTimeLineX->state() == QTimeLine::Running)
    scrollTimeLineX->stop();
  if (scrollTimeLineY->state() == QTimeLine::Running)
    scrollTimeLineY->stop();
  if (zoomTimeLine->state() == QTimeLine::Running)
    zoomTimeLine->stop();
}

inline void ImageViewerV2::scroll(int dx, int dy, bool smooth) {
  if (smooth) {
    scrollSmooth(dx, dy);
  } else {
    scrollPrecise(dx, dy);
  }
}

void ImageViewerV2::scrollSmooth(int dx, int dy) {
  if (dx) {
    bool redirect = false;
    int currentXPos = hs->value();
    int newEndFrame = currentXPos + static_cast<int>(dx);
    if ((newEndFrame < currentXPos &&
         currentXPos < scrollTimeLineX->endFrame()) ||
        (newEndFrame > currentXPos &&
         currentXPos > scrollTimeLineX->endFrame())) {
      redirect = true;
    }
    if (scrollTimeLineX->state() == QTimeLine::Running) {
      int oldEndFrame = scrollTimeLineX->endFrame();
      // if(oldEndFrame == currentYPos)
      //     createScrollTimeLine();
      if (!redirect)
        newEndFrame = oldEndFrame + static_cast<int>(dx);
    }
    scrollTimeLineX->stop();
    scrollTimeLineX->setFrameRange(currentXPos, newEndFrame);
    scrollTimeLineX->start();
  }
  if (dy) {
    bool redirect = false;
    int currentYPos = vs->value();
    int newEndFrame = currentYPos + static_cast<int>(dy);
    if ((newEndFrame < currentYPos &&
         currentYPos < scrollTimeLineY->endFrame()) ||
        (newEndFrame > currentYPos &&
         currentYPos > scrollTimeLineY->endFrame())) {
      redirect = true;
    }
    if (scrollTimeLineY->state() == QTimeLine::Running) {
      int oldEndFrame = scrollTimeLineY->endFrame();
      // if(oldEndFrame == currentYPos)
      //     createScrollTimeLine();
      if (!redirect)
        newEndFrame = oldEndFrame + static_cast<int>(dy);
    }
    scrollTimeLineY->stop();
    scrollTimeLineY->setFrameRange(currentYPos, newEndFrame);
    scrollTimeLineY->start();
  }
  saveViewportPos();
}

void ImageViewerV2::scrollPrecise(int dx, int dy) {
  stopPosAnimation();
  hs->setValue(hs->value() + dx);
  vs->setValue(vs->value() + dy);
  centerIfNecessary();
  snapToEdges();
  saveViewportPos();
}

// used by scrollTimeLine
void ImageViewerV2::scrollToX(int x) {
  hs->setValue(x);
  centerIfNecessary();
  snapToEdges();
  update();
  qApp->processEvents();
}

// used by scrollTimeLine
void ImageViewerV2::scrollToY(int y) {
  vs->setValue(y);
  centerIfNecessary();
  snapToEdges();
  update();
  qApp->processEvents();
}

void ImageViewerV2::onScrollTimelineFinished() { saveViewportPos(); }

void ImageViewerV2::swapToOriginalPixmap() {
  hideUpscaledCrop();
  if (!pixmap || !pixmapItemScaled.isVisible())
    return;
  pixmapItemScaled.hide();
  pixmapItemScaled.setPixmap(QPixmap());
  pixmapScaled.reset(nullptr);
  if (!mSvgMode) {
    pixmapItem.show();
  }
}

void ImageViewerV2::updateCasSettings() {
  if (mScalingFilter == QI_FILTER_CAS) {
    float sharpening = settings->casSharpening();
    float contrast = settings->casContrast();
    pixmapItem.setCasSettings(sharpening, contrast);
    pixmapItemScaled.setCasSettings(sharpening, contrast);
    pixmapItemCrop.setCasSettings(sharpening, contrast);
  } else {
    pixmapItem.setCasSettings(0.0f, 0.0f);
    pixmapItemScaled.setCasSettings(0.0f, 0.0f);
    pixmapItemCrop.setCasSettings(0.0f, 0.0f);
  }
}

void ImageViewerV2::setZoomAnchor(QPoint viewportPos) {
  zoomAnchor = QPair<QPointF, QPoint>(
      pixmapItem.mapFromScene(mapToScene(viewportPos)), viewportPos);
}

void ImageViewerV2::zoomAnchored(float newScale) {
  if (currentScale() != newScale) {
    QPointF vportCenter =
        mapToScene(viewport()->geometry()).boundingRect().center();
    doZoom(newScale);
    // calculate shift to adjust viewport center
    // we do this in viewport coordinates to avoid any rounding errors
    QPointF diff = zoomAnchor.second -
                   mapFromScene(pixmapItem.mapToScene(zoomAnchor.first));
    centerOn(vportCenter - diff);
    requestScaling();
  }
}

// zoom in around viewport center
void ImageViewerV2::zoomIn() { doZoomIn(false); }

// zoom in around cursor if its inside window
void ImageViewerV2::zoomInCursor() { doZoomIn(true); }

void ImageViewerV2::doZoomIn(bool atCursor) {
  if (atCursor && underMouse())
    setZoomAnchor(mapFromGlobal(cursor().pos()));
  else
    setZoomAnchor(viewport()->rect().center());

  float baseScale = currentScale();
  if (settings->enableSmoothZoom() &&
      zoomTimeLine->state() == QTimeLine::Running) {
    baseScale = zoomTargetScale;
  }

  float newScale = baseScale * (1.0f + zoomStep);
  if (useFixedZoomLevels && zoomLevels.count()) {
    if (baseScale < zoomLevels.first()) {
      newScale = qMin(baseScale * (1.0f + zoomStep), zoomLevels.first());
    } else if (baseScale >= zoomLevels.last()) {
      newScale = baseScale * (1.0f + zoomStep);
    } else {
      for (int i = 0; i < zoomLevels.count(); i++) {
        float level = zoomLevels.at(i);
        if (baseScale < level) {
          newScale = level;
          break;
        }
      }
    }
  }

  newScale = qBound(minScale, newScale, maxScale);

  if (settings->enableSmoothZoom()) {
    zoomStartScale = currentScale();
    zoomTargetScale = newScale;
    zoomTimeLine->stop();
    zoomTimeLine->start();
  } else {
    zoomAnchored(newScale);
    centerIfNecessary();
    snapToEdges();
    imageFitMode = FIT_FREE;
    if (pixmapItem.scale() == fitWindowScale)
      imageFitMode = FIT_WINDOW;
    else if (pixmapItem.scale() == fitWindowStretchScale)
      imageFitMode = FIT_WINDOW_STRETCH;
  }
}

// zoom out around viewport center
void ImageViewerV2::zoomOut() { doZoomOut(false); }

// zoom out around cursor if its inside window
void ImageViewerV2::zoomOutCursor() { doZoomOut(true); }

void ImageViewerV2::doZoomOut(bool atCursor) {
  if (atCursor && underMouse())
    setZoomAnchor(mapFromGlobal(cursor().pos()));
  else
    setZoomAnchor(viewport()->rect().center());

  float baseScale = currentScale();
  if (settings->enableSmoothZoom() &&
      zoomTimeLine->state() == QTimeLine::Running) {
    baseScale = zoomTargetScale;
  }

  float newScale = baseScale / (1.0f + zoomStep);
  if (useFixedZoomLevels && zoomLevels.count()) {
    if (baseScale > zoomLevels.last()) {
      newScale = qMax(zoomLevels.last(), baseScale / (1.0f + zoomStep));
    } else if (baseScale <= zoomLevels.first()) {
      newScale = baseScale / (1.0f + zoomStep);
    } else {
      for (int i = zoomLevels.count() - 1; i >= 0; i--) {
        float level = zoomLevels.at(i);
        if (baseScale > level) {
          newScale = level;
          break;
        }
      }
    }
  }

  newScale = qBound(minScale, newScale, maxScale);

  if (settings->enableSmoothZoom()) {
    zoomStartScale = currentScale();
    zoomTargetScale = newScale;
    zoomTimeLine->stop();
    zoomTimeLine->start();
  } else {
    zoomAnchored(newScale);
    centerIfNecessary();
    snapToEdges();
    imageFitMode = FIT_FREE;
    if (pixmapItem.scale() == fitWindowScale)
      imageFitMode = FIT_WINDOW;
    else if (pixmapItem.scale() == fitWindowStretchScale)
      imageFitMode = FIT_WINDOW_STRETCH;
  }
}

void ImageViewerV2::toggleLockZoom() {
  if (!isDisplaying())
    return;
  if (mViewLock != LOCK_ZOOM) {
    mViewLock = LOCK_ZOOM;
    lockZoom();
  } else {
    mViewLock = LOCK_NONE;
  }
}

bool ImageViewerV2::lockZoomEnabled() { return (mViewLock == LOCK_ZOOM); }

void ImageViewerV2::lockZoom() {
  lockedScale = pixmapItem.scale();
  imageFitMode = FIT_FREE;
  saveViewportPos();
}

void ImageViewerV2::toggleLockView() {
  if (!isDisplaying())
    return;
  if (mViewLock != LOCK_ALL) {
    mViewLock = LOCK_ALL;
    lockZoom();
    saveViewportPos();
  } else {
    mViewLock = LOCK_NONE;
  }
}

bool ImageViewerV2::lockViewEnabled() { return (mViewLock == LOCK_ALL); }

// savedViewportPos is [0...1][0...1]
// values are where viewport center is on the image
void ImageViewerV2::saveViewportPos() {
  if (mViewLock != LOCK_ALL)
    return;
  QGraphicsPixmapItem *item = &pixmapItem;
  QPointF sceneCenter = mapToScene(viewport()->rect().center()) + QPointF(1, 1);
  auto itemRect = item->sceneBoundingRect();
  savedViewportPos.setX(
      qBound(qreal(0), (sceneCenter.x() - itemRect.left()) / itemRect.width(),
             qreal(1)));
  savedViewportPos.setY(
      qBound(qreal(0), (sceneCenter.y() - itemRect.top()) / itemRect.height(),
             qreal(1)));
}

void ImageViewerV2::applySavedViewportPos() {
  QGraphicsPixmapItem *item = &pixmapItem;
  auto itemRect = item->sceneBoundingRect();
  QPointF newScenePos;
  newScenePos.setX(itemRect.left() + itemRect.width() * savedViewportPos.x());
  newScenePos.setY(itemRect.top() + itemRect.height() * savedViewportPos.y());
  centerOn(newScenePos);
  centerIfNecessary();
  snapToEdges();
}

void ImageViewerV2::centerIfNecessary() {
  if (!pixmap)
    return;
  QSize sz = scaledSizeR();
  auto imgRect = pixmapItem.sceneBoundingRect();
  auto vport = mapToScene(viewport()->geometry()).boundingRect();
  if (sz.width() <= viewport()->width())
    hs->setValue(pixmapItem.offset().x() -
                 (int)(vport.width() - imgRect.width()) / 2);
  if (sz.height() <= viewport()->height())
    vs->setValue(pixmapItem.offset().y() -
                 (int)(vport.height() - imgRect.height()) / 2);
}

void ImageViewerV2::snapToEdges() {
  QRect imgRect = scaledRectR();
  // current vport center
  QPointF centerTarget = mapToScene(viewport()->rect()).boundingRect().center();
  qreal xShift = 0;
  qreal yShift = 0;
  if (imgRect.width() > width()) {
    if (imgRect.left() > 0)
      xShift = imgRect.left();
    else if (imgRect.right() < width())
      xShift = imgRect.right() - width();
  }
  if (imgRect.height() > height()) {
    if (imgRect.top() > 0)
      yShift = imgRect.top();
    else if (imgRect.bottom() < height())
      yShift = imgRect.bottom() - height();
  }
  centerOn(centerTarget + QPointF(xShift, yShift));
}

void ImageViewerV2::doZoom(float newScale) {
  if (!pixmap)
    return;
  newScale = qBound(minScale, newScale, maxScale);
  // fix scene position to integer values
  auto tl = pixmapItem.sceneBoundingRect().topLeft().toPoint();
  pixmapItem.setOffset(tl);
  pixmapItem.setScale(newScale);
  if (mSvgMode && svgItem) {
    svgItem->setScale(newScale);
  }

  pixmapItem.setTransformationMode(selectTransformationMode());
  swapToOriginalPixmap();
  emit scaleChanged(newScale);
}

ImageFitMode ImageViewerV2::fitMode() const { return imageFitMode; }

// rounds a point in scene coordinates so it stays on the same spot on viewport
QPointF ImageViewerV2::sceneRoundPos(QPointF scenePoint) const {
  return mapToScene(mapFromScene(scenePoint));
}

// rounds a rect in scene coordinates so it stays on the same spot on viewport
// the result is what's actually drawn on screen (incl. size)
QRectF ImageViewerV2::sceneRoundRect(QRectF sceneRect) const {
  QRectF rounded = QRectF(sceneRoundPos(sceneRect.topLeft()), sceneRect.size());
  return QRectF(sceneRoundPos(sceneRect.topLeft()), sceneRect.size());
}

// size as it appears on screen (rounded)
QSize ImageViewerV2::scaledSizeR() const {
  if (!pixmap)
    return QSize(0, 0);
  QRectF pixmapSceneRect = pixmapItem.mapRectToScene(pixmapItem.boundingRect());
  return sceneRoundRect(pixmapSceneRect).size().toSize();
}

// in viewport coords (rounded up)
QRect ImageViewerV2::scaledRectR() const {
  QRectF pixmapSceneRect = pixmapItem.mapRectToScene(pixmapItem.boundingRect());
  return QRect(mapFromScene(pixmapSceneRect.topLeft()),
               mapFromScene(pixmapSceneRect.bottomRight()));
}

float ImageViewerV2::currentScale() const { return pixmapItem.scale(); }

QSize ImageViewerV2::sourceSize() const {
  if (!pixmap)
    return QSize(0, 0);
  return pixmap->size();
}

QRect ImageViewerV2::visibleImageRect() const {
  if (!pixmap || pixmap->isNull())
    return QRect();

  QRectF sceneRect = mapToScene(viewport()->rect()).boundingRect();
  QRectF imageRectF = pixmapItem.mapRectFromScene(sceneRect);

  // Correct for the 10000, 10000 offset in pixmapItem
  imageRectF.translate(-pixmapItem.offset());

  QRect imgBounds(0, 0, pixmap->width(), pixmap->height());
  QRect intersected = imageRectF.toAlignedRect().intersected(imgBounds);

  QPixmap scaled = currentScaledPixmapCopy();
  if (scaled.isNull())
    return QRect();

  double scaleX = (double)scaled.width() / pixmap->width();
  double scaleY = (double)scaled.height() / pixmap->height();

  QRect scaledVisibleRect(qRound(intersected.x() * scaleX),
                          qRound(intersected.y() * scaleY),
                          qRound(intersected.width() * scaleX),
                          qRound(intersected.height() * scaleY));

  QRect scaledBounds(0, 0, scaled.width(), scaled.height());
  return scaledVisibleRect.intersected(scaledBounds);
}

QPixmap ImageViewerV2::currentScaledPixmapCopy() const {
  if (!pixmap || pixmap->isNull())
    return QPixmap();

  if (pixmapScaled && !pixmapScaled->isNull()) {
    return *pixmapScaled;
  }

  QSize tSize = scaledSizeR() * dpr;
  if (tSize.isEmpty())
    return QPixmap();

  // Qt::TransformationMode
  Qt::TransformationMode mode = Qt::SmoothTransformation;
  if (mScalingFilter == QI_FILTER_NEAREST) {
    mode = Qt::FastTransformation;
  }
  return pixmap->scaled(tSize, Qt::KeepAspectRatio, mode);
}

QRect ImageViewerV2::visibleImageViewportRect() const {
  if (!pixmap || pixmap->isNull())
    return QRect();

  QRectF imageSceneRect = pixmapItem.mapRectToScene(pixmapItem.boundingRect());
  QPolygonF poly = mapFromScene(imageSceneRect);
  QRect rect = poly.boundingRect().toAlignedRect();
  return rect.intersected(viewport()->rect());
}

QImage ImageViewerV2::grabViewportImage() const {
  QWidget *view = viewport();
  if (!view || view->size().isEmpty())
    return QImage();

  QImage image(view->size() * devicePixelRatioF(), QImage::Format_ARGB32_Premultiplied);
  image.setDevicePixelRatio(devicePixelRatioF());
  image.fill(Qt::transparent);

  QPainter painter(&image);
  const_cast<ImageViewerV2 *>(this)->render(&painter);
  painter.end();

  return image;
}

float ImageViewerV2::getDpr() const { return dpr; }

void ImageViewerV2::togglePanorama() {
  if (!isDisplaying())
    return;
  mPanoramaMode = !mPanoramaMode;
  if (mPanoramaMode) {
    hideUpscaledCrop();
    pixmapItem.hide();
    pixmapItemScaled.hide();
    panoramaItem->setPixmap(pixmap);
    panoramaItem->setViewParameters(mPanoramaYaw, mPanoramaPitch, mPanoramaFov);
    panoramaItem->show();
  } else {
    panoramaItem->hide();
    if (mSvgMode) {
      svgItem->show();
    } else {
      pixmapItem.show();
    }
    applyFitMode();
  }
  update();
}

void ImageViewerV2::setColorAdjustments(float brightness, float contrast,
                                        float saturation, float hue,
                                        float exposure, float temperature,
                                        float tint) {
  pixmapItem.setColorAdjustments(brightness, contrast, saturation, hue,
                                 exposure, temperature, tint);
  pixmapItemScaled.setColorAdjustments(brightness, contrast, saturation, hue,
                                       exposure, temperature, tint);
  pixmapItemCrop.setColorAdjustments(brightness, contrast, saturation, hue,
                                     exposure, temperature, tint);
  if (panoramaItem) {
    panoramaItem->setColorAdjustments(brightness, contrast, saturation, hue,
                                      exposure, temperature, tint);
  }
  updateCasSettings();
}

qreal ImageViewerV2::smootherstepEasing(qreal t) {
  return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

void ImageViewerV2::onZoomTimelineValueChanged(qreal value) {
  float currentAnimScale =
      (value >= 1.0)
          ? zoomTargetScale
          : zoomStartScale + (zoomTargetScale - zoomStartScale) * value;
  zoomAnchored(currentAnimScale);
  centerIfNecessary();
  snapToEdges();

  imageFitMode = FIT_FREE;
  if (std::abs(pixmapItem.scale() - fitWindowScale) < 0.001)
    imageFitMode = FIT_WINDOW;
  else if (std::abs(pixmapItem.scale() - fitWindowStretchScale) < 0.001)
    imageFitMode = FIT_WINDOW_STRETCH;
}

QRect ImageViewerV2::visibleOriginalImageRect() const {
  if (!pixmap || pixmap->isNull())
    return QRect();

  QRectF sceneRect = mapToScene(viewport()->rect()).boundingRect();
  QRectF imageRectF = pixmapItem.mapRectFromScene(sceneRect);

  imageRectF.translate(-pixmapItem.offset());

  QRect imgBounds(0, 0, pixmap->width(), pixmap->height());
  return imageRectF.toAlignedRect().intersected(imgBounds);
}

void ImageViewerV2::setUpscaledCrop(const QImage &cropImg, QRect origCrop) {
  if (mPanoramaMode)
    return;
  if (!pixmap || pixmap->isNull() || origCrop.isEmpty())
    return;

  QPixmap cropPixmap = QPixmap::fromImage(cropImg);
  pixmapItemCrop.setPixmap(cropPixmap);

  // Position at scene coordinates corresponding to the original crop
  QPointF scenePos =
      pixmapItem.mapToScene(pixmapItem.offset() + origCrop.topLeft());
  scenePos = sceneRoundPos(scenePos);

  // Calculate the net upscale factor of this crop relative to its original crop
  // size
  double upscaleFactor = (double)cropImg.width() / origCrop.width();
  double cropScale = pixmapItem.scale() / upscaleFactor;

  pixmapItemCrop.setTransformationMode(pixmapItem.transformationMode());
  pixmapItemCrop.setScale(cropScale);
  pixmapItemCrop.setTransformOriginPoint(0, 0);
  pixmapItemCrop.setOffset(0, 0);
  pixmapItemCrop.setPos(scenePos);
  pixmapItemCrop.show();
  viewport()->update();
}

void ImageViewerV2::hideUpscaledCrop() {
  pixmapItemCrop.hide();
  pixmapItemCrop.setPixmap(QPixmap());
  viewport()->update();
}

bool ImageViewerV2::isBusyInteracting() const {
  return mouseInteraction != MouseInteractionState::MOUSE_NONE;
}
