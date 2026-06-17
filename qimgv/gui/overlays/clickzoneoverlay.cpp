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

  fadeEffect = new QGraphicsOpacityEffect(this);
  this->setGraphicsEffect(fadeEffect);
  fadeAnimation = new QPropertyAnimation(fadeEffect, "opacity");
  fadeAnimation->setDuration(200);
  fadeAnimation->setEasingCurve(QEasingCurve::OutCubic);
  connect(fadeAnimation, &QPropertyAnimation::finished, this, &ClickZoneOverlay::onAnimationFinished);

  fadeEffect->setOpacity(0);
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
  // Strip alpha: thumbpanel inherits thumbnailOpacity, buttons must be opaque.
  QColor newButtonColor = settings->colorScheme().thumbpanel;
  newButtonColor.setAlphaF(1.0);

  if (newDrawZones == drawZones && newButtonColor == mButtonColor)
    return;
  drawZones = newDrawZones;
  mButtonColor = newButtonColor;
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
  ImageLib::recolor(*pixmap, QColor(255, 255, 255));
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
    if(activeZone == zone && fadeAnimation->state() != QPropertyAnimation::Running)
        return;

    // If moving from one side to another, we don't restart from 0,
    // we just change the target icon and continue fading in if needed.
    if(zone != HIGHLIGHT_NONE) {
        activeZone = zone;
        update();
        if(fadeEffect->opacity() < 0.99 && fadeAnimation->endValue().toReal() != 1.0) {
            fadeAnimation->stop();
            fadeAnimation->setStartValue(fadeEffect->opacity());
            fadeAnimation->setEndValue(1.0);
            fadeAnimation->start();
        }
    } else {
        // Fade out to NONE
        fadeAnimation->stop();
        fadeAnimation->setStartValue(fadeEffect->opacity());
        fadeAnimation->setEndValue(0.0);
        fadeAnimation->start();
    }
}

void ClickZoneOverlay::onAnimationFinished() {
    if(fadeEffect->opacity() <= 0.01) {
        activeZone = HIGHLIGHT_NONE;
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
  // Full opacity here — the QGraphicsOpacityEffect fade animation handles
  // the smooth appear/disappear on hover. No extra transparency needed.
  p.setBrush(mButtonColor);
  p.setPen(Qt::NoPen);

  if (activeZone == HIGHLIGHT_LEFT) {
    p.drawRoundedRect(mLeftButton, kButtonRadius, kButtonRadius);
    drawPixmap(p, pixmapLeft, mLeftButton);
  }
  if (activeZone == HIGHLIGHT_RIGHT) {
    p.drawRoundedRect(mRightButton, kButtonRadius, kButtonRadius);
    drawPixmap(p, pixmapRight, mRightButton);
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
