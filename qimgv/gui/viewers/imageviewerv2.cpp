#include "imageviewerv2.h"
#include "settings.h"
#include "panoramagraphicsitem.h"
#include "utils/displayutils.h"
#include <QKeyEvent>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOpenGLWidget>
#include <QPainter>
#include <QScreen>
#include <QSurfaceFormat>
#include <QCoreApplication>

namespace {
constexpr int kCheckerboardTileSizePx = 16;
constexpr int kCheckerboardCellsPerAxis = 2;
constexpr int kViewerMultisampleCount = 4;
constexpr qreal kMinimumDevicePixelRatio = 1.0;
constexpr QRgb kCheckerboardLightRgb = 0xFF999999;
constexpr QRgb kCheckerboardDarkRgb = 0xFF666666;
// Delay before a high-quality rescale after the view stops moving.
constexpr int kScaleRequestDelayMs = 80;
// Scene rect; PanoramaGraphicsItem reports it as its bounding rect, so it
// must cover the viewport.
constexpr qreal kPanoramaSceneExtent = 200000;
// The view's own scene rect: smaller than any viewport and top-left aligned,
// so scene coordinates equal viewport coordinates and the view never scrolls.
// The image items are positioned where ViewTransform places the image.
constexpr QRectF kViewSceneRect(0.0, 0.0, 1.0, 1.0);
// Right-button movement (logical px, scaled by DPR) before it counts as a
// zoom, and before a horizontal stroke counts as a next/prev gesture.
constexpr qreal kZoomThresholdPx = 4.0;
constexpr qreal kGestureThresholdPx = 40.0;
// Trackpad detection heuristics: wheel notches arrive as multiples of half a
// notch, and a trackpad stream suppresses wheel detection for a short while.
constexpr int kWheelNotchAngleDelta = 120;
constexpr int kWheelHalfNotchAngleDelta = 60;
constexpr qint64 kTrackpadScrollCooldownMs = 250;
// Image edge misalignment tolerated by the wheel "is scrollable" check.
constexpr int kScrollEdgeTolerancePx = 2;

QPixmap createTransparencyCheckerboard(qreal dpr) {
  const qreal effectiveDpr = qMax(dpr, kMinimumDevicePixelRatio);
  const int physicalTileSize =
      qRound(kCheckerboardTileSizePx * effectiveDpr);
  const int firstCellSize = physicalTileSize / kCheckerboardCellsPerAxis;
  const int secondCellSize = physicalTileSize - firstCellSize;

  QPixmap checkerboard(physicalTileSize, physicalTileSize);
  checkerboard.fill(QColor::fromRgba(kCheckerboardLightRgb));

  QPainter painter(&checkerboard);
  painter.fillRect(firstCellSize, 0, secondCellSize, firstCellSize,
                   QColor::fromRgba(kCheckerboardDarkRgb));
  painter.fillRect(0, firstCellSize, firstCellSize, secondCellSize,
                   QColor::fromRgba(kCheckerboardDarkRgb));
  painter.end();

  checkerboard.setDevicePixelRatio(effectiveDpr);
  return checkerboard;
}
} // namespace

ImageViewerV2::ImageViewerV2(QWidget *parent)
    : QGraphicsView(parent), image(nullptr),
      movie(nullptr), transparencyGrid(false),
      loopPlayback(true), mIsFullscreen(false),
      trackpadDetection(true),
      mouseInteraction(MouseInteractionState::MOUSE_NONE),
      mScalingFilter(QI_FILTER_BILINEAR),
      scene(nullptr), zoomTimeLine(nullptr), zoomStartScale(1.0f),
      zoomTargetScale(1.0f), mAnimationActive(false) {
  setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
  this->viewport()->setAttribute(Qt::WA_OpaquePaintEvent, false);
  setFocusPolicy(Qt::NoFocus);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  setAcceptDrops(false);

  // The private IViewSurface base is only reachable from inside the class.
  viewTransform = std::make_unique<ViewTransformController>(
      static_cast<const IViewSurface &>(*this));
  viewTransform->setDevicePixelRatio(this->devicePixelRatioF());

  scrollTimeLineY = new QTimeLine(ANIMATION_SPEED, this);
  scrollTimeLineY->setEasingCurve(QEasingCurve::OutSine);
  scrollTimeLineY->setUpdateInterval(DisplayUtils::animationTimerIntervalMs(this));
  scrollTimeLineX = new QTimeLine(ANIMATION_SPEED, this);
  scrollTimeLineX->setEasingCurve(QEasingCurve::OutSine);
  scrollTimeLineX->setUpdateInterval(DisplayUtils::animationTimerIntervalMs(this));
  connect(scrollTimeLineX, &QTimeLine::finished, this,
          &ImageViewerV2::onScrollTimelineFinished);
  connect(scrollTimeLineY, &QTimeLine::finished, this,
          &ImageViewerV2::onScrollTimelineFinished);

  zoomTimeLine = new QTimeLine(ANIMATION_SPEED, this);
  QEasingCurve zoomCurve;
  zoomCurve.setCustomType(smootherstepEasing);
  zoomTimeLine->setEasingCurve(zoomCurve);
  zoomTimeLine->setUpdateInterval(DisplayUtils::animationTimerIntervalMs(this));
  connect(zoomTimeLine, &QTimeLine::valueChanged, this,
          &ImageViewerV2::onZoomTimelineValueChanged);
  connect(zoomTimeLine, &QTimeLine::finished, this,
          [this]() { viewTransform->saveViewportPosition(); });
  connect(zoomTimeLine, &QTimeLine::finished, this,
          &ImageViewerV2::requestScaling);

  animationTimer = new QTimer(this);
  animationTimer->setSingleShot(true);

  scaleTimer = new QTimer(this);
  scaleTimer->setSingleShot(true);
  scaleTimer->setInterval(kScaleRequestDelayMs);

  checkerboard = createTransparencyCheckerboard(getDpr());

  lastTouchpadScroll.start();
  updateInputThresholds();

  pixmapItem.setTransformationMode(Qt::SmoothTransformation);

  this->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  this->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  scene = new QGraphicsScene(this);
  // The image items move on every pan and zoom; a spatial index only costs.
  scene->setItemIndexMethod(QGraphicsScene::NoIndex);
  scene->setSceneRect(0, 0, kPanoramaSceneExtent, kPanoramaSceneExtent);
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
  this->setAlignment(Qt::AlignLeft | Qt::AlignTop);
  this->setSceneRect(kViewSceneRect);

  QOpenGLWidget *glViewport = new QOpenGLWidget();
  QSurfaceFormat glFormat = glViewport->format();
  glFormat.setSamples(kViewerMultisampleCount);
  glViewport->setFormat(glFormat);
  this->setViewport(glViewport);
  connect(glViewport, &QOpenGLWidget::frameSwapped,
          this, &ImageViewerV2::onViewportFrameSwapped);

  connect(scrollTimeLineX, &QTimeLine::frameChanged, this,
          &ImageViewerV2::scrollToX);
  connect(scrollTimeLineY, &QTimeLine::frameChanged, this,
          &ImageViewerV2::scrollToY);

  connect(viewTransform.get(), &ViewTransformController::transformChanged,
          this, &ImageViewerV2::onTransformChanged);
  connect(viewTransform.get(), &ViewTransformController::scaleChanged,
          this, &ImageViewerV2::scaleChanged);
  connect(viewTransform.get(), &ViewTransformController::positionChanged,
          this, &ImageViewerV2::onViewPositionChanged);
  connect(viewTransform.get(), &ViewTransformController::imageCentered,
          this, [this]() { emit imageAreaChanged(scaledRectR()); });
  connect(viewTransform.get(), &ViewTransformController::anchoredZoomApplied,
          this, &ImageViewerV2::requestScaling);
  connect(viewTransform.get(), &ViewTransformController::panoramaChanged,
          this, &ImageViewerV2::applyPanoramaView);

  connect(animationTimer, &QTimer::timeout, this,
          &ImageViewerV2::onAnimationTimer, Qt::UniqueConnection);

  QObject::connect(scaleTimer, &QTimer::timeout,
                   [this]() { this->requestScaling(); });

  fullscreenUpdateTimer = new QTimer(this);
  fullscreenUpdateTimer->setSingleShot(true);
  connect(fullscreenUpdateTimer, &QTimer::timeout, this, [this]() {
    if (mIsFullscreen) {
      viewport()->update();
      lastFullscreenUpdate.restart();
    }
  });

  readSettings();
  connect(settings, &Settings::settingsChanged, this,
          &ImageViewerV2::readSettings);
}

ImageViewerV2::~ImageViewerV2() {
  // Ensure the OpenGL context is current when destroying items so they can release GL resources
  if (auto *glWidget = qobject_cast<QOpenGLWidget*>(viewport())) {
    glWidget->makeCurrent();
  }
  // Delete panoramaItem explicitly while context is current
  delete panoramaItem;
}

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
  const qreal newDpr = this->devicePixelRatioF();
  if (viewTransform->transform().devicePixelRatio() == newDpr)
    return;
  qDebug() << "DPR CHANGED " << getDpr() << " >> " << newDpr;
  viewTransform->setDevicePixelRatio(newDpr);
  checkerboard = createTransparencyCheckerboard(newDpr);
  updateInputThresholds();
  if (image) {
    const_cast<QImage*>(image.get())->setDevicePixelRatio(newDpr);
    pixmapItem.setImage(*image);
    if (!mSvgMode) {
      pixmapItem.show();
    }
    pixmapItem.update();
    viewTransform->changeDevicePixelRatio(newDpr);
    requestScaling();
    update();
  }
}

void ImageViewerV2::syncDevicePixelRatio() {
  // Update DPR just in case an event was missed or not delivered yet
  const qreal newDpr = this->devicePixelRatioF();
  if (viewTransform->transform().devicePixelRatio() != newDpr) {
    viewTransform->setDevicePixelRatio(newDpr);
    updateInputThresholds();
  }
}

void ImageViewerV2::updateInputThresholds() {
  const qreal dpr = getDpr();
  zoomThreshold = static_cast<int>(dpr * kZoomThresholdPx);
  gestureThreshold = static_cast<int>(dpr * kGestureThresholdPx);
}

QSize ImageViewerV2::viewportSize() const {
  return viewport()->size();
}

QPointF ImageViewerV2::pointerPosition() const {
  return mapFromGlobal(QCursor::pos());
}

// Places the image items where the view transform puts the image.
void ImageViewerV2::onTransformChanged() {
  const ViewTransform &transform = viewTransform->transform();
  const qreal scale = transform.scale();
  const QPointF imagePos = transform.imagePosition();
  const bool scaleDiffers = pixmapItem.scale() != scale;
  pixmapItem.setPos(imagePos);
  pixmapItem.setScale(scale);
  pixmapItemScaled.setPos(imagePos);
  if (svgItem) {
    svgItem->setPos(imagePos);
    svgItem->setScale(scale);
  }
  if (scaleDiffers) {
    pixmapItem.setTransformationMode(selectTransformationMode());
    swapToOriginalImage();
  }
}

// The view moved: the scaled image and upscaled crop no longer match it.
void ImageViewerV2::onViewPositionChanged() {
  if (scaleTimer->isActive())
    scaleTimer->stop();
  scaleTimer->start();
  hideUpscaledCrop();
}

void ImageViewerV2::applyPanoramaView() {
  const PanoramaView &panorama = viewTransform->panorama();
  panoramaItem->setViewParameters(panorama.yaw(), panorama.pitch(), panorama.fov());
}

void ImageViewerV2::readSettings() {
  transparencyGrid = settings->transparencyGrid();
  ViewTransformConfig config;
  config.expandImage = settings->expandImage();
  config.expandLimit = static_cast<float>(settings->expandLimit());
  config.keepFitMode = settings->keepFitMode();
  config.defaultFitMode = settings->imageFitMode();
  config.zoomStep = settings->zoomStep();
  config.focusPoint = settings->focusPointIn1to1Mode();
  config.useFixedZoomLevels = settings->useFixedZoomLevels();
  if (config.useFixedZoomLevels)
    config.zoomLevels = parseZoomLevels(settings->zoomLevels());
  config.unlockMinZoom = settings->unlockMinZoom();
  trackpadDetection = settings->trackpadDetection();
  // set bg color
  onFullscreenModeChanged(mIsFullscreen);
  ScalingFilter prevScalingFilter = mScalingFilter;
  setScalingFilter(settings->scalingFilter());
  bool prevUseUpscayl = mUseUpscayl;
  mUseUpscayl = settings->useUpscayl();
  updateCasSettings();
  // Re-fits the displayed image when a fit-relevant setting changed.
  bool fitScaleSettingsChanged = viewTransform->applyConfig(config);
  // Whether anything that affects the *currently displayed* scale/crop
  // changed. Settings notifications fire for a lot of unrelated changes
  // (theme, accent color, panel sizes, etc); without this check, any of
  // those would re-trigger a rescale (and, while zoomed past 100% with AI
  // upscaling on, a full "AI Upscaling..." re-run) even though nothing
  // about the image display actually changed.
  bool scalingRelevantChanged = fitScaleSettingsChanged ||
                                mScalingFilter != prevScalingFilter ||
                                mUseUpscayl != prevUseUpscayl;
  if (isDisplaying()) {
    if (scalingRelevantChanged) {
      requestScaling();
    }
  } else {
    setFitMode(config.defaultFitMode);
  }
}

void ImageViewerV2::onFullscreenModeChanged(bool mode) {
  QColor bgColor;
  mIsFullscreen = mode;
  if (mode) {
    bgColor = settings->colorScheme().background_fullscreen;
    bgColor.setAlphaF(1.0);
  } else {
    fullscreenUpdateTimer->stop();
    lastFullscreenUpdate.invalidate();
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
    mAnimationActive = true;
    emit animationPaused(false);
    animationTimer->start(movie->nextFrameDelay());
  }
}

void ImageViewerV2::stopAnimation() {
  if (movie) {
    mAnimationActive = false;
    emit animationPaused(true);
    animationTimer->stop();
  }
}

void ImageViewerV2::pauseResume() {
  if (movie) {
    if (mAnimationActive)
      stopAnimation();
    else
      startAnimation();
  }
}

void ImageViewerV2::enableDrags() { dragsEnabled = true; }

void ImageViewerV2::disableDrags() { dragsEnabled = false; }

void ImageViewerV2::onAnimationTimer() {
  if (!movie || !mAnimationActive)
    return;
  if (movie->currentFrameNumber() == movie->frameCount() - 1) {
    // last frame
    if (!loopPlayback) {
      stopAnimation();
      emit playbackFinished();
      return;
    } else {
      movie->jumpToFrame(0);
    }
  } else {
    if (!movie->jumpToNextFrame()) {
      qWarning() << "[Error] QMovie:" << movie->lastErrorString();
      this->stopAnimation();
      return;
    }
  }
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

  bool blocked = movie->blockSignals(true);

  if (frame < movie->currentFrameNumber())
    movie->jumpToFrame(0);
  while (frame != movie->currentFrameNumber()) {
    if (!movie->jumpToNextFrame()) {
      qWarning() << "[Error] QMovie:" << movie->lastErrorString();
      break;
    }
  }

  movie->blockSignals(blocked);

  onMovieFrameChanged(movie->currentFrameNumber());
  return true;
}

void ImageViewerV2::onMovieFrameChanged(int frameNumber) {
  if (!movie)
    return;

  QImage frameImg = movie->currentImage();
  if (frameImg.isNull())
    return;

  bool isFirstFrame = (image == nullptr || image->isNull());

  updateImage(std::make_shared<const QImage>(frameImg));
  emit frameChanged(frameNumber);

  if (isFirstFrame) {
    emit durationChanged(movie->frameCount());
    viewTransform->showImage(image->size());
  }

  if (mAnimationActive && movie->frameCount() > 1) {
    animationTimer->start(movie->nextFrameDelay());
  }
}

void ImageViewerV2::updateImage(std::shared_ptr<const QImage> newImage) {
  image = std::move(newImage);
  const_cast<QImage*>(image.get())->setDevicePixelRatio(getDpr());
  pixmapItem.setImage(*image);
  pixmapItem.update();
  if (mPanoramaMode) {
    panoramaItem->setImage(image);
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
      svgItem->setScale(currentScale());
    }
  } else {
    panoramaItem->hide();
    if (svgItem) {
      svgItem->hide();
    }
    pixmapItem.show();
  }
}

void ImageViewerV2::showAnimation(const QString &filePath, const QString &format) {
  reset();
  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

  auto newMovie = std::make_shared<QMovie>(filePath, format.toUtf8());
  if (newMovie && newMovie->isValid()) {
    syncDevicePixelRatio();
    movie = newMovie;
    connect(movie.get(), &QMovie::frameChanged, this, &ImageViewerV2::onMovieFrameChanged);

    Qt::TransformationMode mode = selectTransformationMode();
    pixmapItem.setTransformationMode(mode);

    movie->jumpToFrame(0);
    startAnimation();
  }
}

void ImageViewerV2::showImage(std::shared_ptr<const QImage> _image,
                              QString filePath) {
  if (_image && !_image->isNull()) {
    syncDevicePixelRatio();
    bool isSameFile = (!filePath.isEmpty() && filePath == currentFilePath);
    QSize oldSize = sourceSize();
    QSize newSize = _image->size();
    bool isRotationOrMirror = (isSameFile && oldSize.isValid() &&
                               (oldSize.width() * oldSize.height() ==
                                newSize.width() * newSize.height()));
    // Scale or fit mode and relative viewport centre of the current image,
    // kept across a rotation or mirror.
    const PreservedView preservedView = viewTransform->preservedView();

    reset();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

    currentFilePath = filePath;

    if (filePath.endsWith(".svg", Qt::CaseInsensitive)) {
      svgItem = new QGraphicsSvgItem(filePath);
      if (svgItem->renderer() && svgItem->renderer()->isValid()) {
        svgItem->setPos(viewTransform->transform().imagePosition());
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

    updateImage(_image);

    if (isRotationOrMirror)
      viewTransform->showTransformedImage(newSize, preservedView);
    else
      viewTransform->showImage(newSize);
    requestScaling();
    update();
  }
}

// reset state, remove image & stop animation
void ImageViewerV2::reset() {
  setRenderingSettled(false);
  mFramePresentationPending = false;
  stopPosAnimation();
  pixmapItemScaled.setImage(QImage());
  pixmapItemCrop.setImage(QImage());
  imageScaled = QImage();
  pixmapItem.setImage(QImage());
  if (svgItem) {
    scene->removeItem(svgItem);
    delete svgItem;
    svgItem = nullptr;
  }
  image.reset();
  stopAnimation();
  mAnimationActive = false;
  mSvgMode = false;
  if (movie) {
    disconnect(movie.get(), &QMovie::frameChanged, this, &ImageViewerV2::onMovieFrameChanged);
    movie = nullptr;
  }
  viewTransform->clear();
  // when this view is not in focus this it won't update the background
  // so we force it here
  viewport()->update();
  panoramaItem->setImage(nullptr);
  panoramaItem->hide();
  pixmapItem.show();
  pixmapItemScaled.hide();
  pixmapItemCrop.hide();
  currentFilePath = "";
}

void ImageViewerV2::closeImage() { reset(); }

void ImageViewerV2::setScaledImage(QImage newFrame) {
  if (!movie && newFrame.size() != scaledSizeR() * getDpr())
    return;
  imageScaled = newFrame;
  imageScaled.setDevicePixelRatio(getDpr());
  pixmapItemScaled.setImage(imageScaled);
  pixmapItem.hide();
  pixmapItemScaled.show();
  requestSettledFramePresentation();
}

bool ImageViewerV2::isDisplaying() const { return (image != nullptr); }

bool ImageViewerV2::isRenderingSettled() const {
  return mRenderingSettled;
}

void ImageViewerV2::requestSettledFramePresentation() {
  mFramePresentationPending = true;
  viewport()->update();
}

// Single point of truth for mRenderingSettled: also forwards the state to
// pixmapItem, which only spends the extra one-shot precise-downsample GPU
// pass (FilterPixmapItem::buildPreciseDownsample) while settled, keeping
// interactive pan/zoom performance identical to before that feature existed.
//
// mRenderingSettled itself (and the renderingSettled() signal, emitted
// separately by onViewportFrameSwapped()) keep their existing meaning and
// still report settled == true during animation playback -- other
// consumers of isRenderingSettled()/renderingSettled() are unrelated to
// this GPU pass and shouldn't change behavior. Only what's forwarded to
// pixmapItem is gated here, and specifically on mAnimationActive (frames
// are actually being advanced by animationTimer right now) rather than on
// movie being non-null (this file happens to be an animated container):
// while frames are actively advancing, onMovieFrameChanged() hands
// pixmapItem a brand-new QImage (with a new cacheKey()) on every decoded
// frame, so if pixmapItem were told "settled" during that it would rebuild
// the precise downsample on every single animation frame -- exactly the
// per-frame GPU cost this settle-gate exists to avoid. Once playback is
// paused (or the container only has one frame, so it never started),
// mImage stops changing, so pixmapItem can still get the precise pass like
// any other static image.
void ImageViewerV2::setRenderingSettled(bool settled) {
  mRenderingSettled = settled;
  pixmapItem.setSettled(settled && !mAnimationActive);
}

void ImageViewerV2::onViewportFrameSwapped() {
  if (!mFramePresentationPending)
    return;

  mFramePresentationPending = false;
  setRenderingSettled(true);
  emit renderingSettled();
}

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

  if (mScalingFilter == QI_FILTER_NEAREST || mScalingFilter == QI_FILTER_CAS || mScalingFilter == QI_FILTER_SMART_GPU)
    swapToOriginalImage();
  updateCasSettings();
  requestScaling();
}

void ImageViewerV2::setLoopPlayback(bool mode) {
  if (movie && mode && loopPlayback != mode)
    startAnimation();
  loopPlayback = mode;
}

void ImageViewerV2::setFilterNearest() {
  setScalingFilter(QI_FILTER_NEAREST);
}

void ImageViewerV2::setFilterBilinear() {
  setScalingFilter(QI_FILTER_BILINEAR);
}

// returns a mode based on current zoom level and a bunch of toggles
Qt::TransformationMode ImageViewerV2::selectTransformationMode() {
  if (mScalingFilter == QI_FILTER_NEAREST) {
    return Qt::FastTransformation;
  }
  return Qt::SmoothTransformation;
}

void ImageViewerV2::setExpandImage(bool mode) {
  viewTransform->setExpandImage(mode);
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
  setRenderingSettled(false);
  mFramePresentationPending = false;
  bool isAt100 = std::abs(currentScale() - 1.0f) < kScaleEpsilon;
  if (mSvgMode || !image || isAt100 ||
      (mScalingFilter == QI_FILTER_CAS && !(mUseUpscayl && currentScale() > 1.0f)) ||
      (mScalingFilter == QI_FILTER_SMART_GPU && !(mUseUpscayl && currentScale() > 1.0f)) ||
      movie) {
    requestSettledFramePresentation();
    return;
  }

  if ((zoomTimeLine && zoomTimeLine->state() == QTimeLine::Running) ||
      mouseInteraction == MouseInteractionState::MOUSE_ZOOM ||
      mouseInteraction == MouseInteractionState::MOUSE_WHEEL_ZOOM) {
    return;
  }

  QSize targetSize = scaledSizeR() * getDpr();

  // Output buffer limits (same as ImageLib::scaled defaults)
  constexpr int    kMaxScaledDimension = 12288;
  constexpr qint64 kMaxScaledPixels    = 100'000'000; // 100 megapixels

  // Upscayl path: output buffer is guarded downstream by ImageLib::scaled()
  // with enhanced limits (16384 dim / 256 MP).
  // Non-Upscayl path: cap the CPU-scaled output size here.
  if (!mUseUpscayl &&
      (targetSize.width() > kMaxScaledDimension ||
       targetSize.height() > kMaxScaledDimension ||
       static_cast<qint64>(targetSize.width()) * targetSize.height() > kMaxScaledPixels)) {
    requestSettledFramePresentation();
    return;
  }

  if (scaleTimer->isActive())
    scaleTimer->stop();

  // The GPU port of MKS2021 lives in the Qt Quick renderer; this viewer
  // shows the identical kernel from the CPU scaler.
  const ScalingFilter requestFilter =
      mScalingFilter == QI_FILTER_MKS2021_GPU ? QI_FILTER_MKS2021 : mScalingFilter;
  emit scalingRequested(targetSize, requestFilter);
}

void ImageViewerV2::refreshScaling() {
  requestScaling();
}

bool ImageViewerV2::imageFits() const {
  return viewTransform->transform().imageFits();
}

bool ImageViewerV2::scaledImageFits() const {
  return viewTransform->transform().scaledImageFits();
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
  if (!image) {
    QWidget::mousePressEvent(event);
    return;
  }
  mouseMoveStartPos = event->pos();
  mousePressPos = mouseMoveStartPos;
  if (event->button() & Qt::RightButton) {
    viewTransform->setZoomAnchor(event->pos());
  } else {
    QGraphicsView::mousePressEvent(event);
  }
}

void ImageViewerV2::onMouseMoveFullscreen() {
  if (mIsFullscreen) {
    constexpr int defaultFullscreenIntervalMs = 16;
    constexpr double msPerSecond = 1000.0;
    int interval = defaultFullscreenIntervalMs;
    if (QScreen *scr = this->screen()) {
      double scrRate = scr->refreshRate();
      if (scrRate > 0.0) {
        interval = qMax(1, qRound(msPerSecond / scrRate));
      }
    }

    auto *vp = viewport();
    if (vp) {
      if (!lastFullscreenUpdate.isValid()) {
        lastFullscreenUpdate.start();
        vp->update();
      } else {
        qint64 elapsed = lastFullscreenUpdate.elapsed();
        if (elapsed >= interval) {
          vp->update();
          lastFullscreenUpdate.restart();
        } else {
          if (fullscreenUpdateTimer && !fullscreenUpdateTimer->isActive()) {
            fullscreenUpdateTimer->start(interval - elapsed);
          }
        }
      }
    }
  }
}

void ImageViewerV2::mouseMoveEvent(QMouseEvent *event) {
  QWidget::mouseMoveEvent(event);
  onMouseMoveFullscreen();
  if (mPanoramaMode && (event->buttons() & Qt::LeftButton)) {
    viewTransform->dragPanorama(event->pos() - mouseMoveStartPos);
    mouseMoveStartPos = event->pos();
    return;
  }

  if (!image || mouseInteraction == MouseInteractionState::MOUSE_DRAG ||
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

  if (!image || mouseInteraction == MouseInteractionState::MOUSE_NONE) {
    QGraphicsView::mouseReleaseEvent(event);
    event->ignore();
  }
  mouseInteraction = MouseInteractionState::MOUSE_NONE;
  if (needScale) {
    requestScaling();
  }
}

void ImageViewerV2::mouseDoubleClickEvent(QMouseEvent *event) {
  if (mPanoramaMode && (event->button() == Qt::LeftButton)) {
    viewTransform->resetPanorama();
    event->accept();
    return;
  }
  QGraphicsView::mouseDoubleClickEvent(event);
}

// warning for future me:
// for some reason in qgraphicsview wheelEvent is followed by moveEvent (wtf?)
void ImageViewerV2::wheelEvent(QWheelEvent *event) {
  if (mPanoramaMode) {
    viewTransform->zoomPanoramaByWheel(event->angleDelta().y());
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
                (abs(angleDelta.y()) >= kWheelNotchAngleDelta &&
                 !(angleDelta.y() % kWheelHalfNotchAngleDelta)) &&
                lastTouchpadScroll.elapsed() > kTrackpadScrollCooldownMs;
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
        viewTransform->scrollBy(QPointF(-dx * TRACKPAD_SCROLL_MULTIPLIER,
                                        -dy * TRACKPAD_SCROLL_MULTIPLIER));
      }
    } else if (isWheel &&
               settings->imageScrolling() == SCROLL_BY_TRACKPAD_AND_WHEEL) {
      // scroll by interval
      bool scrollable = false;
      QRect imgRect = scaledRectR();
      // shift by 2px in case of img edge misalignment
      if ((event->angleDelta().y() < 0 &&
           imgRect.bottom() > height() + kScrollEdgeTolerancePx) ||
          (event->angleDelta().y() > 0 &&
           imgRect.top() < -kScrollEdgeTolerancePx)) {
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
    viewTransform->saveViewportPosition();
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
  if (fitMode() == FIT_ORIGINAL)
    viewTransform->applyFitMode();
  setRenderingSettled(false);
  mFramePresentationPending = false;
  scaleTimer->start();
}

void ImageViewerV2::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Left || event->key() == Qt::Key_Right ||
      event->key() == Qt::Key_Up || event->key() == Qt::Key_Down) {
    event->ignore();
    return;
  }
  if (event->key() == Qt::Key_PageUp) {
    int scrollDistance = viewport() ? viewport()->height() : DEFAULT_SCROLL_DISTANCE;
    scroll(0, -scrollDistance, true);
    event->accept();
    return;
  }
  if (event->key() == Qt::Key_PageDown) {
    int scrollDistance = viewport() ? viewport()->height() : DEFAULT_SCROLL_DISTANCE;
    scroll(0, scrollDistance, true);
    event->accept();
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
  if (!isDisplaying() || !transparencyGrid || !image->hasAlphaChannel())
    return;
  painter->drawTiledPixmap(pixmapItem.sceneBoundingRect(), checkerboard);
}

// simple pan behavior (cursor stops at the screen edges)
inline void ImageViewerV2::mousePan(QMouseEvent *event) {
  if (scaledImageFits())
    return;
  mouseMoveStartPos -= event->pos();
  scroll(mouseMoveStartPos.x(), mouseMoveStartPos.y(), false);
  mouseMoveStartPos = event->pos();
  viewTransform->saveViewportPosition();
}

//  zooming while the right button is pressed
//  note: on reaching min zoom level the fitMode is set to FIT_WINDOW;
//        mid-zoom it is set to FIT_FREE.
//        FIT_FREE mode does not persist when changing images.
inline void ImageViewerV2::mouseMoveZoom(QMouseEvent *event) {
  int currentPos = event->pos().y();
  int moveDistance = mouseMoveStartPos.y() - currentPos;
  mouseMoveStartPos = event->pos();

  if (mPanoramaMode) {
    // Moving the mouse up narrows the field of view (zooms in).
    viewTransform->zoomPanoramaByGesture(moveDistance);
    return;
  }
  viewTransform->zoomTo(viewTransform->transform().gestureZoomScale(moveDistance));
}

// public, sends scale request
void ImageViewerV2::setFitMode(ImageFitMode newMode) {
  stopScaleTimerAndAnimations();
  viewTransform->setFitMode(newMode);
  requestScaling();
}

// public, sends scale request
void ImageViewerV2::setFitOriginal() { setFitMode(FIT_ORIGINAL); }

// public, sends scale request
void ImageViewerV2::setFitWidth() {
  stopScaleTimerAndAnimations();
  viewTransform->forceFitMode(FIT_WIDTH);
  requestScaling();
}

// public, sends scale request
void ImageViewerV2::setFitWindow() {
  stopScaleTimerAndAnimations();
  viewTransform->forceFitMode(FIT_WINDOW);
  requestScaling();
}

void ImageViewerV2::setExpandSmallImagesInFitMode(bool enabled) {
  viewTransform->setExpandSmallImagesInFitMode(enabled);
}

// public, sends scale request
void ImageViewerV2::setFitHeight() {
  stopScaleTimerAndAnimations();
  viewTransform->forceFitMode(FIT_HEIGHT);
  requestScaling();
}

void ImageViewerV2::stopScaleTimerAndAnimations() {
  if (scaleTimer->isActive())
    scaleTimer->stop();
  stopPosAnimation();
}

void ImageViewerV2::switchFitMode() {
  if (fitMode() == FIT_WINDOW) {
    setFitMode(FIT_ORIGINAL);
  } else {
    setFitMode(FIT_WINDOW);
  }
}


void ImageViewerV2::resizeEvent(QResizeEvent *event) {
  QGraphicsView::resizeEvent(event);
  // reset this so we won't generate unnecessary drag'n'drop event
  mousePressPos = mapFromGlobal(cursor().pos());
  // Qt emits some unnecessary resizeEvents on startup
  // so we try to ignore them
  if (parentWidget()->isVisible()) {
    setRenderingSettled(false);
    mFramePresentationPending = false;
    stopPosAnimation();
    viewTransform->refitToViewport();
    update();
    if (scaleTimer->isActive())
      scaleTimer->stop();
    scaleTimer->start();
  } else {
    viewTransform->syncViewport();
  }
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
  const int refreshIntervalMs = DisplayUtils::animationTimerIntervalMs(this);
  if (dx) {
    bool redirect = false;
    int currentXPos = viewTransform->transform().scrollPosition().x();
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
    scrollTimeLineX->setUpdateInterval(refreshIntervalMs);
    scrollTimeLineX->start();
  }
  if (dy) {
    bool redirect = false;
    int currentYPos = viewTransform->transform().scrollPosition().y();
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
    scrollTimeLineY->setUpdateInterval(refreshIntervalMs);
    scrollTimeLineY->start();
  }
  viewTransform->saveViewportPosition();
}

void ImageViewerV2::scrollPrecise(int dx, int dy) {
  stopPosAnimation();
  viewTransform->scrollBy(QPointF(dx, dy));
  viewTransform->saveViewportPosition();
}

// used by scrollTimeLine
void ImageViewerV2::scrollToX(int x) {
  viewTransform->scrollTo(Qt::Horizontal, x);
  update();
  qApp->processEvents();
}

// used by scrollTimeLine
void ImageViewerV2::scrollToY(int y) {
  viewTransform->scrollTo(Qt::Vertical, y);
  update();
  qApp->processEvents();
}

void ImageViewerV2::onScrollTimelineFinished() {
  viewTransform->saveViewportPosition();
}

void ImageViewerV2::swapToOriginalImage() {
  hideUpscaledCrop();
  if (!image || !pixmapItemScaled.isVisible())
    return;
  pixmapItemScaled.hide();
  pixmapItemScaled.setImage(QImage());
  imageScaled = QImage();
  if (!mSvgMode) {
    pixmapItem.show();
  }
}

void ImageViewerV2::updateCasSettings() {
  pixmapItem.setScalingFilter(mScalingFilter);
  pixmapItemScaled.setScalingFilter(mScalingFilter);
  pixmapItemCrop.setScalingFilter(mScalingFilter);

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
  if (viewport()) {
    viewport()->update();
  }
}

// zoom in around viewport center
void ImageViewerV2::zoomIn() { doZoomIn(false); }

// zoom in around cursor if its inside window
void ImageViewerV2::zoomInCursor() { doZoomIn(true); }

void ImageViewerV2::doZoomIn(bool atCursor) {
  viewTransform->setZoomAnchor(zoomAnchorPosition(atCursor));

  float baseScale = currentScale();
  if (settings->enableSmoothZoom() &&
      zoomTimeLine->state() == QTimeLine::Running) {
    baseScale = zoomTargetScale;
  }
  startZoom(viewTransform->transform().zoomInScale(baseScale));
}

// zoom out around viewport center
void ImageViewerV2::zoomOut() { doZoomOut(false); }

// zoom out around cursor if its inside window
void ImageViewerV2::zoomOutCursor() { doZoomOut(true); }

void ImageViewerV2::doZoomOut(bool atCursor) {
  viewTransform->setZoomAnchor(zoomAnchorPosition(atCursor));

  float baseScale = currentScale();
  if (settings->enableSmoothZoom() &&
      zoomTimeLine->state() == QTimeLine::Running) {
    baseScale = zoomTargetScale;
  }
  startZoom(viewTransform->transform().zoomOutScale(baseScale));
}

// Zooms around the cursor when requested and the cursor is over the view,
// otherwise around the viewport centre.
QPointF ImageViewerV2::zoomAnchorPosition(bool atCursor) const {
  if (atCursor && underMouse())
    return pointerPosition();
  return viewport()->rect().center();
}

void ImageViewerV2::startZoom(float newScale) {
  if (settings->enableSmoothZoom()) {
    zoomStartScale = currentScale();
    zoomTargetScale = newScale;
    zoomTimeLine->stop();
    zoomTimeLine->setUpdateInterval(DisplayUtils::animationTimerIntervalMs(this));
    zoomTimeLine->start();
  } else {
    viewTransform->zoomTo(newScale);
  }
}

void ImageViewerV2::toggleLockZoom() {
  viewTransform->toggleLockZoom();
}

bool ImageViewerV2::lockZoomEnabled() {
  return viewTransform->transform().lock() == ViewLock::Zoom;
}

void ImageViewerV2::toggleLockView() {
  viewTransform->toggleLockView();
}

bool ImageViewerV2::lockViewEnabled() {
  return viewTransform->transform().lock() == ViewLock::All;
}

ImageFitMode ImageViewerV2::fitMode() const {
  return viewTransform->transform().fitMode();
}

// rounds a point in scene coordinates so it stays on the same spot on viewport
QPointF ImageViewerV2::sceneRoundPos(QPointF scenePoint) const {
  return mapToScene(mapFromScene(scenePoint));
}

// size as it appears on screen (rounded)
QSize ImageViewerV2::scaledSizeR() const {
  if (!image)
    return QSize(0, 0);
  return viewTransform->transform().scaledSize();
}

// in viewport coords (rounded up)
QRect ImageViewerV2::scaledRectR() const {
  return viewTransform->transform().scaledRect();
}

float ImageViewerV2::currentScale() const {
  return viewTransform->transform().scale();
}

QSize ImageViewerV2::sourceSize() const {
  if (!image)
    return QSize(0, 0);
  return image->size();
}

QRect ImageViewerV2::visibleImageRect() const {
  if (!image || image->isNull())
    return QRect();

  QRectF sceneRect = mapToScene(viewport()->rect()).boundingRect();
  QRectF imageRectF = pixmapItem.mapRectFromScene(sceneRect);

  // Item coordinates include the pixmap offset
  imageRectF.translate(-pixmapItem.offset());

  QRect imgBounds(0, 0, image->width(), image->height());
  QRect intersected = imageRectF.toAlignedRect().intersected(imgBounds);

  QSize scaledSize;
  if (!imageScaled.isNull()) {
    scaledSize = imageScaled.size();
  } else {
    QSize tSize = scaledSizeR() * getDpr();
    if (tSize.isEmpty())
      return QRect();
    scaledSize = image->size().scaled(tSize, Qt::KeepAspectRatio);
  }

  if (scaledSize.isEmpty())
    return QRect();

  double scaleX = (double)scaledSize.width() / image->width();
  double scaleY = (double)scaledSize.height() / image->height();

  QRect scaledVisibleRect(qRound(intersected.x() * scaleX),
                          qRound(intersected.y() * scaleY),
                          qRound(intersected.width() * scaleX),
                          qRound(intersected.height() * scaleY));

  QRect scaledBounds(0, 0, scaledSize.width(), scaledSize.height());
  return scaledVisibleRect.intersected(scaledBounds);
}

QPixmap ImageViewerV2::currentScaledPixmapCopy() const {
  if (!image || image->isNull())
    return QPixmap();

  if (!imageScaled.isNull()) {
    return QPixmap::fromImage(imageScaled);
  }

  QSize tSize = scaledSizeR() * getDpr();
  if (tSize.isEmpty())
    return QPixmap();

  // Qt::TransformationMode
  Qt::TransformationMode mode = Qt::SmoothTransformation;
  if (mScalingFilter == QI_FILTER_NEAREST) {
    mode = Qt::FastTransformation;
  }
  return QPixmap::fromImage(image->scaled(tSize, Qt::KeepAspectRatio, mode));
}

QRect ImageViewerV2::visibleImageViewportRect() const {
  if (!image || image->isNull())
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

float ImageViewerV2::getDpr() const {
  return static_cast<float>(viewTransform->transform().devicePixelRatio());
}

void ImageViewerV2::togglePanorama() {
  if (!isDisplaying())
    return;
  mPanoramaMode = !mPanoramaMode;
  if (mPanoramaMode) {
    hideUpscaledCrop();
    pixmapItem.hide();
    pixmapItemScaled.hide();
    panoramaItem->setImage(image);
    applyPanoramaView();
    panoramaItem->show();
  } else {
    panoramaItem->hide();
    if (mSvgMode) {
      svgItem->show();
    } else {
      pixmapItem.show();
    }
    viewTransform->applyFitMode();
  }
  update();
}

void ImageViewerV2::setColorAdjustments(const ColorAdjustments &adjustments) {
  pixmapItem.setColorAdjustments(adjustments);
  pixmapItemScaled.setColorAdjustments(adjustments);
  pixmapItemCrop.setColorAdjustments(adjustments);
  if (panoramaItem) {
    panoramaItem->setColorAdjustments(adjustments);
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
  viewTransform->zoomTo(currentAnimScale);
}

QRect ImageViewerV2::visibleOriginalImageRect() const {
  if (!image || image->isNull())
    return QRect();

  QRectF sceneRect = mapToScene(viewport()->rect()).boundingRect();
  QRectF imageRectF = pixmapItem.mapRectFromScene(sceneRect);

  imageRectF.translate(-pixmapItem.offset());

  // FilterPixmapItem geometry is logical, while QImage operations address
  // physical source pixels.
  const qreal imageDpr =
      qMax(image->devicePixelRatio(), kMinimumDevicePixelRatio);
  imageRectF = QTransform::fromScale(imageDpr, imageDpr).mapRect(imageRectF);

  return imageRectF.toAlignedRect().intersected(image->rect());
}

void ImageViewerV2::setUpscaledCrop(const QImage &cropImg, QRect origCrop) {
  if (mPanoramaMode)
    return;
  if (!image || image->isNull() || cropImg.isNull() || origCrop.isEmpty())
    return;

  pixmapItemCrop.setImage(cropImg);

  const qreal sourceDpr =
      qMax(image->devicePixelRatio(), kMinimumDevicePixelRatio);
  const qreal cropDpr =
      qMax(cropImg.devicePixelRatio(), kMinimumDevicePixelRatio);
  const QPointF cropTopLeftInItem =
      QPointF(origCrop.topLeft()) / sourceDpr;

  // Position at scene coordinates corresponding to the original crop
  QPointF scenePos =
      pixmapItem.mapToScene(pixmapItem.offset() + cropTopLeftInItem);
  scenePos = sceneRoundPos(scenePos);

  // Calculate the net upscale factor of this crop relative to its original crop
  // size
  const double upscaleFactor =
      static_cast<double>(cropImg.width()) / origCrop.width();
  const double cropScale =
      pixmapItem.scale() * cropDpr / (upscaleFactor * sourceDpr);

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
  pixmapItemCrop.setImage(QImage());
  viewport()->update();
}

bool ImageViewerV2::isBusyInteracting() const {
  return mouseInteraction != MouseInteractionState::MOUSE_NONE;
}
