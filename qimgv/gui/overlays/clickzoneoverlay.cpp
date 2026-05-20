#include "clickzoneoverlay.h"

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

void ClickZoneOverlay::readSettings() {
  if (settings->clickableEdgesVisible() == drawZones)
    return;
  drawZones = settings->clickableEdgesVisible();
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
  mLeftZone = QRect(0, 0, zoneSize, height());
  mRightZone = QRect(width() - zoneSize, 0, zoneSize, height());
}

void ClickZoneOverlay::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event)
  if (activeZone == HIGHLIGHT_NONE || !drawZones || width() <= 250)
    return;
  QPainter p(this);
  if (isPressed)
    p.setOpacity(0.30f);
  else
    p.setOpacity(0.40f);
  QBrush brush;
  brush.setColor(QColor(0, 0, 0));
  brush.setStyle(Qt::SolidPattern);

  if (activeZone == HIGHLIGHT_LEFT) {
    p.fillRect(mLeftZone, brush);
    drawPixmap(p, pixmapLeft, mLeftZone);
  }
  if (activeZone == HIGHLIGHT_RIGHT) {
    p.fillRect(mRightZone, brush);
    drawPixmap(p, pixmapRight, mRightZone);
  }
}

// draws pixmap centered inside rect
void ClickZoneOverlay::drawPixmap(QPainter &p, QPixmap *pixmap, QRect rect) {
  if (isPressed)
    p.setOpacity(0.37f);
  else
    p.setOpacity(0.5f);
  p.setRenderHint(QPainter::SmoothPixmapTransform);
  QPointF pos;
  if (hiResPixmaps) {
    pos = QPointF(rect.left() + rect.width() / 2 -
                      pixmap->width() / (2 * pixmapDrawScale),
                  rect.top() + rect.height() / 2 -
                      pixmap->height() / (2 * pixmapDrawScale));
  } else {
    pos = QPointF(rect.left() + rect.width() / 2 - pixmap->width() / 2,
                  rect.top() + rect.height() / 2 - pixmap->height() / 2);
  }
  p.drawPixmap(pos, *pixmap);
}
