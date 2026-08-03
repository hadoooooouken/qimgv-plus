#include "clickzoneoverlay.h"
#include "settings.h"
#include "utils/displayutils.h"

#include <QImage>

namespace {

QRectF visiblePixmapBounds(const QPixmap &pixmap) {
  if (pixmap.isNull())
    return {};

  const QImage image = pixmap.toImage();
  QRect physicalBounds;
  bool hasVisiblePixel = false;

  for (int y = 0; y < image.height(); ++y) {
    for (int x = 0; x < image.width(); ++x) {
      if (qAlpha(image.pixel(x, y)) == 0)
        continue;

      const QRect pixelRect(QPoint(x, y), QPoint(x, y));
      physicalBounds = hasVisiblePixel
                           ? physicalBounds.united(pixelRect)
                           : pixelRect;
      hasVisiblePixel = true;
    }
  }

  if (!hasVisiblePixel) {
    qWarning() << "ClickZoneOverlay received a fully transparent navigation icon";
    return QRectF(QPointF(), pixmap.deviceIndependentSize());
  }

  const qreal dpr = pixmap.devicePixelRatioF();
  return QRectF(
      QPointF(physicalBounds.x() / dpr, physicalBounds.y() / dpr),
      QSizeF(physicalBounds.width() / dpr, physicalBounds.height() / dpr));
}

} // namespace

ClickZoneOverlay::ClickZoneOverlay(FloatingWidgetContainer *parent)
    : FloatingWidget(parent) {
  // this is just for painting, we are handling mouse events elsewhere
  setAttribute(Qt::WA_TransparentForMouseEvents);
  if (parent)
    setContainerSize(parent->size());

  dpr = this->devicePixelRatioF();

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
  mTimeline.setUpdateInterval(DisplayUtils::animationTimerIntervalMs(this));

  connect(&mTimeline, &QTimeLine::frameChanged, this,
          &ClickZoneOverlay::animationUpdate);
  connect(&mTimeline, &QTimeLine::finished, this,
          &ClickZoneOverlay::onAnimationFinish);

  this->show();
}

ClickZoneOverlay::~ClickZoneOverlay() = default;

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

    mTimeline.setUpdateInterval(DisplayUtils::animationTimerIntervalMs(this));

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
    drawPixmap(p, mLeftIcon, shifted);
  }
  if (activeZone == HIGHLIGHT_RIGHT) {
    QRect shifted = mRightButton.translated(mSlideOffset, 0);
    p.drawRoundedRect(shifted, kButtonRadius, kButtonRadius);
    drawPixmap(p, mRightIcon, shifted);
  }
}

// Centers the arrow's visible pixels rather than its transparent canvas.
void ClickZoneOverlay::drawPixmap(QPainter &p, const NavigationIcon &icon,
                                  const QRect &buttonRect) {
  if (icon.pixmap.isNull())
    return;

  const QPointF pos =
      QRectF(buttonRect).center() - icon.visibleBounds.center();
  p.drawPixmap(pos, icon.pixmap);
}

void ClickZoneOverlay::recolorIcons() {
    QColor arrowColor;
    if (mButtonColor.lightnessF() > 0.5) {
        // Light theme
        arrowColor = QColor(100, 100, 100);
    } else {
        // Dark theme
        arrowColor = QColor(164, 164, 164);
    }

    mLeftIcon.pixmap = IconFontManager::pixmap(
        FluentIcon::ChevronLeft48, kArrowIconSizePx, arrowColor, dpr);
    mLeftIcon.visibleBounds = visiblePixmapBounds(mLeftIcon.pixmap);

    mRightIcon.pixmap = IconFontManager::pixmap(
        FluentIcon::ChevronRight48, kArrowIconSizePx, arrowColor, dpr);
    mRightIcon.visibleBounds = visiblePixmapBounds(mRightIcon.pixmap);
}
