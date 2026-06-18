#include "slidepanel.h"

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

  timeline.setDuration(ANIMATION_DURATION);
  timeline.setEasingCurve(QEasingCurve::Linear);
  timeline.setUpdateInterval(16);
  timeline.setStartFrame(0);
  timeline.setEndFrame(ANIMATION_DURATION);
  // For some reason 16 feels janky on windows. Linux is fine.
#ifdef _WIN32
  timeline.setUpdateInterval(8);
#else
  timeline.setUpdateInterval(16);
#endif

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
  QWidget::hide();
}

void SlidePanel::hideAnimated() {
  if (layoutManaged())
    hide();
  else if (!this->isHidden() && timeline.state() != QTimeLine::Running) {
    mHiding = true;
    outCurve.setType(QEasingCurve::InCubic);
    emit animationStarted();
    timeline.start();
  }
}

void SlidePanel::showAnimated() {
  if (hasWidget()) {
    if (layoutManaged()) {
      show();
      return;
    }
    // If already showing and fully visible, do nothing
    if (!mHiding && !this->isHidden() && timeline.state() != QTimeLine::Running)
      return;

    mHiding = false;
    outCurve.setType(QEasingCurve::OutCubic);
    if (this->isHidden()) {
      fadeEffect->setOpacity(0);
      setProperty("pos", endPosition);
      QWidget::show();
      QWidget::raise();
      // Pre-cache: force one render cycle so the opacity effect
      // allocates its compositing pixmap before the animation starts
      repaint();
    }
    if (timeline.state() != QTimeLine::Running) {
      emit animationStarted();
      timeline.start();
    }
  }
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
    fadeEffect->setOpacity(panelVisibleOpacity);
    setProperty("pos", startPosition);
    QWidget::show();
    QWidget::raise();
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
  if (mHiding && triggerRect().contains(adjustedPos, true)) {
    // Smoothly reverse the animation if cursor is back at the panel
    showAnimated();
  } else {
    // Apply the animation frame
    qreal value = outCurve.valueForProgress(static_cast<qreal>(frame) /
                                            ANIMATION_DURATION);

    QPoint posA = mHiding ? startPosition : endPosition;
    QPoint posB = mHiding ? endPosition : startPosition;

    QPoint newPosOffset =
        QPoint(static_cast<int>((posB.x() - posA.x()) * value),
               static_cast<int>((posB.y() - posA.y()) * value));
    setProperty("pos", posA + newPosOffset);
    fadeEffect->setOpacity(mHiding ? (1 - value) : value);
  }
  update();
}

void SlidePanel::setAnimationRange(QPoint start, QPoint end) {
  startPosition = start;
  endPosition = end;
}

void SlidePanel::onAnimationFinish() {
  if (mHiding) {
    QWidget::hide();
    fadeEffect->setOpacity(panelVisibleOpacity);
    setProperty("pos", startPosition);
  }
  emit animationFinished();
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
