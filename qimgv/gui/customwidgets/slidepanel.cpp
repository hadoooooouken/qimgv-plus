#include "slidepanel.h"
#include "utils/displayutils.h"

SlidePanel::SlidePanel(FloatingWidgetContainer *parent)
    : FloatingWidget(parent), panelSize(50), slideAmount(40), mWidget(nullptr) {
  mLayout.setSpacing(0);
  mLayout.setContentsMargins(0, 0, 0, 0);
  this->setLayout(&mLayout);

  // fade effect
  fadeEffect = new QGraphicsOpacityEffect(this);
  this->setGraphicsEffect(fadeEffect);

  startPosition = geometry().topLeft();

  outCurve.setType(QEasingCurve::OutCubic);

  timeline.setDuration(kAnimationDurationMs);
  timeline.setEasingCurve(QEasingCurve::Linear);
  timeline.setStartFrame(0);
  timeline.setEndFrame(kAnimationDurationMs);
  timeline.setUpdateInterval(DisplayUtils::animationTimerIntervalMs(this));

  connect(&timeline, &QTimeLine::frameChanged, this,
          &SlidePanel::animationUpdate);
  connect(&timeline, &QTimeLine::finished, this,
          &SlidePanel::onAnimationFinish);

  this->setAttribute(Qt::WA_NoMousePropagation, true);
  this->setFocusPolicy(Qt::NoFocus);

  setPosition(PANEL_TOP);

  QWidget::hide();
}

SlidePanel::~SlidePanel() {}

void SlidePanel::hide() {
  timeline.stop();
  mVisibilityTarget = VisibilityTarget::Hidden;
  QWidget::hide();
  fadeEffect->setOpacity(panelVisibleOpacity);
  setProperty("pos", startPosition);
  finishActiveAnimation();
}

void SlidePanel::hideAnimated() {
  if (layoutManaged())
    hide();
  else if (!this->isHidden())
    animateTo(VisibilityTarget::Hidden);
}

void SlidePanel::showAnimated() {
  if (hasWidget()) {
    if (layoutManaged()) {
      show();
      return;
    }
    // If already showing and fully visible, do nothing
    if (mVisibilityTarget == VisibilityTarget::Visible &&
        !this->isHidden() && timeline.state() != QTimeLine::Running)
      return;

    if (this->isHidden()) {
      fadeEffect->setOpacity(0);
      setProperty("pos", endPosition);
      QWidget::show();
      QWidget::raise();
      // Pre-cache: force one render cycle so the opacity effect
      // allocates its compositing pixmap before the animation starts
      repaint();
    }
    animateTo(VisibilityTarget::Visible);
  }
}

void SlidePanel::animateTo(VisibilityTarget target) {
  mVisibilityTarget = target;
  const QTimeLine::Direction direction =
      target == VisibilityTarget::Visible ? QTimeLine::Forward
                                          : QTimeLine::Backward;
  timeline.setDirection(direction);

  if (timeline.state() == QTimeLine::Running)
    return;

  // Only pick the curve shape when starting a fresh run from rest.
  // Changing it mid-flight (i.e. on a live reversal, handled above)
  // would recompute valueForProgress() under a different curve at the
  // same progress ratio and produce a visible position/opacity jump.
  outCurve.setType(target == VisibilityTarget::Visible
                        ? QEasingCurve::OutCubic
                        : QEasingCurve::InCubic);

  if (!mAnimationActive) {
    mAnimationActive = true;
    emit animationStarted();
  }
  timeline.setUpdateInterval(DisplayUtils::animationTimerIntervalMs(this));
  timeline.start();
}

void SlidePanel::finishActiveAnimation() {
  if (!mAnimationActive)
    return;

  mAnimationActive = false;
  emit animationFinished();
}

bool SlidePanel::layoutManaged() { return mLayoutManaged; }

void SlidePanel::setLayoutManaged(bool mode) {
  mLayoutManaged = mode;
  if (!mode)
    recalculateGeometry();
}

void SlidePanel::setWidget(std::shared_ptr<QWidget> w) {
  if (!w)
    return;
  if (hasWidget())
    layout()->removeWidget(mWidget.get());
  mWidget = w;
  mWidget->setParent(this);
  mLayout.insertWidget(0, mWidget.get());
}

bool SlidePanel::hasWidget() { return (mWidget != nullptr); }

void SlidePanel::show() {
  if (hasWidget()) {
    timeline.stop();
    mVisibilityTarget = VisibilityTarget::Visible;
    fadeEffect->setOpacity(panelVisibleOpacity);
    setProperty("pos", startPosition);
    QWidget::show();
    QWidget::raise();
    finishActiveAnimation();
  } else {
    qWarning() << "Warning: Trying to show panel containing no widget!";
  }
}

// save current geometry so it is accessible during "pos" animation
void SlidePanel::saveStaticGeometry(QRect geometry) {
  mStaticGeometry = geometry;
}

QRect SlidePanel::staticGeometry() { return mStaticGeometry; }

void SlidePanel::animationUpdate(int frame) {
  // Calculate local cursor position; correct for the current pos() animation
  QPoint adjustedPos = mapFromGlobal(QCursor::pos()) + this->pos();
  if (mVisibilityTarget == VisibilityTarget::Hidden &&
      triggerRect().contains(adjustedPos, true)) {
    // Smoothly reverse the animation if cursor is back at the panel
    showAnimated();
  } else {
    // Apply the animation frame
    const qreal visibleProgress = outCurve.valueForProgress(
        static_cast<qreal>(frame) / kAnimationDurationMs);

    QPoint newPosOffset =
        QPoint(static_cast<int>((startPosition.x() - endPosition.x()) *
                                visibleProgress),
               static_cast<int>((startPosition.y() - endPosition.y()) *
                                visibleProgress));
    setProperty("pos", endPosition + newPosOffset);
    fadeEffect->setOpacity(panelVisibleOpacity * visibleProgress);
  }
  update();
}

void SlidePanel::setAnimationRange(QPoint start, QPoint end) {
  startPosition = start;
  endPosition = end;
}

void SlidePanel::onAnimationFinish() {
  const int targetTime = mVisibilityTarget == VisibilityTarget::Visible
                             ? timeline.duration()
                             : 0;
  if (timeline.currentTime() != targetTime) {
    const QTimeLine::Direction direction =
        mVisibilityTarget == VisibilityTarget::Visible ? QTimeLine::Forward
                                                       : QTimeLine::Backward;
    timeline.setDirection(direction);
    timeline.start();
    return;
  }

  if (mVisibilityTarget == VisibilityTarget::Hidden) {
    QWidget::hide();
    fadeEffect->setOpacity(panelVisibleOpacity);
    setProperty("pos", startPosition);
  }
  finishActiveAnimation();
}

QRect SlidePanel::triggerRect() { return mTriggerRect; }

void SlidePanel::setPosition(PanelPosition p) {
  if (p == PANEL_TOP || p == PANEL_BOTTOM)
    mLayout.setDirection(QBoxLayout::LeftToRight);
  else
    mLayout.setDirection(QBoxLayout::BottomToTop);
  mPosition = p;
  recalculateGeometry();
}

PanelPosition SlidePanel::position() { return mPosition; }

void SlidePanel::recalculateGeometry() {
  if (layoutManaged())
    return;
  if (mPosition == PANEL_TOP) {
    setAnimationRange(QPoint(0, 0), QPoint(0, 0) - QPoint(0, slideAmount));
    saveStaticGeometry(
        QRect(QPoint(0, 0), QPoint(containerSize().width() - 1, height() - 1)));
  } else if (mPosition == PANEL_BOTTOM) {
    setAnimationRange(
        QPoint(0, containerSize().height() - height()),
        QPoint(0, containerSize().height() - height() + slideAmount));
    saveStaticGeometry(
        QRect(QPoint(0, containerSize().height() - height()),
              QPoint(containerSize().width() - 1, containerSize().height())));
  } else if (mPosition == PANEL_LEFT) {
    setAnimationRange(QPoint(0, 0), QPoint(0, 0) - QPoint(slideAmount, 0));
    saveStaticGeometry(QRect(0, 0, width(), containerSize().height()));

  } else { // right
    setAnimationRange(QPoint(containerSize().width() - width(), 0),
                      QPoint(containerSize().width() - width(), 0) +
                          QPoint(slideAmount, 0));
    saveStaticGeometry(QRect(containerSize().width() - width(), 0,
                             containerSize().width(),
                             containerSize().height()));
  }
  this->setGeometry(staticGeometry());
  updateTriggerRect();
}

void SlidePanel::updateTriggerRect() { mTriggerRect = staticGeometry(); }

void SlidePanel::setOrientation() {}
