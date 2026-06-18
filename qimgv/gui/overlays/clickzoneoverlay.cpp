#include "clickzoneoverlay.h"
#include "settings.h"

ClickZoneOverlay::ClickZoneOverlay(FloatingWidgetContainer *parent)
    : FloatingWidget(parent) {
  // this is just for painting, we are handling mouse events elsewhere
  setAttribute(Qt::WA_TransparentForMouseEvents);
  if (parent)
    setContainerSize(parent->size());

  dpr = this->devicePixelRatioF();
  pixmapLeft = loadPixmap(":/res/icons/common/overlay/arrow_left_50.png");
  pixmapRight = loadPixmap(":/res/icons/common/overlay/arrow_right_50.png");

  connect(settings, &Settings::settingsChanged, this,
          &ClickZoneOverlay::readSettings);
  readSettings();

  // QTimeLine-based animation matching SlidePanel.
  // Replaces QGraphicsOpacityEffect which caused arrow/background desync
  // due to internal pixmap caching on a full-screen widget.
  mEasingCurve.setType(QEasingCurve::OutCubic);

  mTimeline.setDuration(kAnimationDuration);
  mTimeline.setEasingCurve(QEasingCurve::Linear);
  mTimeline.setStartFrame(0);
  mTimeline.setEndFrame(kAnimationDuration);
  mTimeline.setUpdateInterval(8);

  connect(&mTimeline, &QTimeLine::frameChanged, this,
          &ClickZoneOverlay::animationUpdate);
  connect(&mTimeline, &QTimeLine::finished, this,
          &ClickZoneOverlay::onAnimationFinish);

  this->show();
}

ClickZoneOverlay::~ClickZoneOverlay() {
  delete pixmapLeft;
  pixmapLeft = nullptr;
  delete pixmapRight;
  pixmapRight = nullptr;
}

void ClickZoneOverlay::readSettings() {
  bool newDrawZones = settings->clickableEdgesVisible();
  // Nav button color follows the thumbpanel color — the same value that
  // "Use black for background and thumbnail bar" already controls.
  QColor newButtonColor = settings->colorScheme().thumbpanel;

  if (newDrawZones == drawZones && newButtonColor == mButtonColor)
    return;
  drawZones = newDrawZones;
  mButtonColor = newButtonColor;
  recolorIcons();
  update();
}

QPixmap *ClickZoneOverlay::loadPixmap(QString path) {
  QPixmap *pixmap;
  if (dpr >= (1.0 + 0.001)) {
    path.replace(".", "@2x.");
    hiResPixmaps = true;
    pixmap = new QPixmap(path);
    if (dpr >= (2.0 - 0.001))
      pixmapDrawScale = dpr;
    else
      pixmapDrawScale = 2.0;
    pixmap->setDevicePixelRatio(pixmapDrawScale);
  } else {
    hiResPixmaps = false;
    pixmap = new QPixmap(path);
    pixmapDrawScale = dpr;
  }
  if (pixmap->isNull()) {
    delete pixmap;
    pixmap = new QPixmap();
  }
  return pixmap;
}

QRect ClickZoneOverlay::leftZone() { return mLeftZone; }

QRect ClickZoneOverlay::rightZone() { return mRightZone; }

void ClickZoneOverlay::highlightLeft() { setHighlightedZone(HIGHLIGHT_LEFT); }

void ClickZoneOverlay::highlightRight() { setHighlightedZone(HIGHLIGHT_RIGHT); }

void ClickZoneOverlay::disableHighlight() {
  setHighlightedZone(HIGHLIGHT_NONE);
}

bool ClickZoneOverlay::isHighlighted() {
  return (activeZone != HIGHLIGHT_NONE);
}

void ClickZoneOverlay::setPressed(bool mode) {
  if (isPressed == mode)
    return;
  isPressed = mode;
  if (isHighlighted())
    update();
}

void ClickZoneOverlay::setHighlightedZone(ActiveHighlightZone zone) {
    if (activeZone == zone && mTimeline.state() != QTimeLine::Running)
        return;

    if (zone != HIGHLIGHT_NONE) {
        // Switch sides without restarting from zero — just update the
        // active zone and keep fading in if we haven't reached full opacity.
        activeZone = zone;
        update();
        if (mHiding || (mCurrentOpacity < 0.99 && mTimeline.state() != QTimeLine::Running)) {
            mHiding = false;
            mEasingCurve.setType(QEasingCurve::OutCubic);
            if (mTimeline.state() != QTimeLine::Running)
                mTimeline.start();
        }
    } else {
        // Begin fade-out + slide-out
        if (!mHiding) {
            mHiding = true;
            mEasingCurve.setType(QEasingCurve::InCubic);
            if (mTimeline.state() != QTimeLine::Running)
                mTimeline.start();
        }
    }
}

void ClickZoneOverlay::animationUpdate(int frame) {
    qreal progress = static_cast<qreal>(frame) / kAnimationDuration;
    qreal value = mEasingCurve.valueForProgress(progress);

    mCurrentOpacity = mHiding ? (1.0 - value) : value;

    // Slide offset: left button slides from -kSlideAmount, right from +kSlideAmount
    int slideBase = (activeZone == HIGHLIGHT_LEFT) ? -kSlideAmount : kSlideAmount;
    if (mHiding)
        mSlideOffset = static_cast<int>(slideBase * value);
    else
        mSlideOffset = static_cast<int>(slideBase * (1.0 - value));

    update();
}

void ClickZoneOverlay::onAnimationFinish() {
    if (mHiding) {
        activeZone = HIGHLIGHT_NONE;
        mCurrentOpacity = 0.0;
        mSlideOffset = 0;
        update();
    }
}

void ClickZoneOverlay::recalculateGeometry() {
  setGeometry(0, 0, containerSize().width(), containerSize().height());
}

void ClickZoneOverlay::resizeEvent(QResizeEvent *event) {
  // Hit zones: full height strips — functional area is unchanged.
  mLeftZone  = QRect(0,             0, kZoneWidth, height());
  mRightZone = QRect(width() - kZoneWidth, 0, kZoneWidth, height());

  // Visible pill rects: 1/3 of window height (with safe minimum), offset from edges.
  const int buttonHeight = qMax(height() / kButtonHeightDivisor, kButtonMinHeight);
  const int buttonY      = (height() - buttonHeight) / 2;
  mLeftButton  = QRect(kButtonEdgeMargin,
                       buttonY,
                       kButtonWidth,
                       buttonHeight);
  mRightButton = QRect(width() - kButtonEdgeMargin - kButtonWidth,
                       buttonY,
                       kButtonWidth,
                       buttonHeight);
}

void ClickZoneOverlay::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event)
  if (activeZone == HIGHLIGHT_NONE || !drawZones || width() <= 250)
    return;

  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  // Opacity applied directly via QPainter — both the rounded rect and the
  // arrow pixmap are drawn in the same pass with the same opacity value,
  // eliminating the desync caused by QGraphicsOpacityEffect pixmap caching.
  p.setOpacity(mCurrentOpacity);
  p.setBrush(mButtonColor);
  p.setPen(Qt::NoPen);

  if (activeZone == HIGHLIGHT_LEFT) {
    QRect shifted = mLeftButton.translated(mSlideOffset, 0);
    p.drawRoundedRect(shifted, kButtonRadius, kButtonRadius);
    drawPixmap(p, pixmapLeft, shifted);
  }
  if (activeZone == HIGHLIGHT_RIGHT) {
    QRect shifted = mRightButton.translated(mSlideOffset, 0);
    p.drawRoundedRect(shifted, kButtonRadius, kButtonRadius);
    drawPixmap(p, pixmapRight, shifted);
  }
}

// Draws the arrow pixmap centered inside the visible button rect.
void ClickZoneOverlay::drawPixmap(QPainter &p, QPixmap *pixmap, QRect buttonRect) {
  p.setRenderHint(QPainter::SmoothPixmapTransform);
  QPointF pos;
  if (hiResPixmaps) {
    pos = QPointF(buttonRect.left() + buttonRect.width()  / 2 -
                      pixmap->width()  / (2 * pixmapDrawScale),
                  buttonRect.top()  + buttonRect.height() / 2 -
                      pixmap->height() / (2 * pixmapDrawScale));
  } else {
    pos = QPointF(buttonRect.left() + buttonRect.width()  / 2 - pixmap->width()  / 2,
                  buttonRect.top()  + buttonRect.height() / 2 - pixmap->height() / 2);
  }
  p.drawPixmap(pos, *pixmap);
}

void ClickZoneOverlay::recolorIcons() {
    if (!pixmapLeft || !pixmapRight)
        return;

    QColor arrowColor;
    if (mButtonColor.lightnessF() > 0.5) {
        // Light theme
        arrowColor = QColor(100, 100, 100);
    } else {
        // Dark theme
        arrowColor = QColor(164, 164, 164);
    }

    ImageLib::recolor(*pixmapLeft,  arrowColor);
    ImageLib::recolor(*pixmapRight, arrowColor);
}
