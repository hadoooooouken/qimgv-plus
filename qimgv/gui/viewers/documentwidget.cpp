#include "documentwidget.h"
#include "settings.h"

namespace {
constexpr int MIN_WINDOW_WIDTH_FOR_PANEL = 800;
constexpr int MIN_WINDOW_HEIGHT_FOR_PANEL = 500;
constexpr int kPanelHoverMarginPx = 8;
// Minimum grace period for a window/taskbar exit (leaveEvent /
// WindowDeactivate), independent of the user's configured hover-hide
// delay. Protects against an incidental cursor dip onto the Windows
// taskbar instantly collapsing the panel when the user has set the
// hover-hide delay to 0 ms; that setting only expresses "hide
// immediately once the cursor leaves the panel's hover area", not
// "skip the taskbar exit grace period too".
constexpr int kWindowExitMinHideDelayMs = 600;
}


DocumentWidget::DocumentWidget(std::shared_ptr<ViewerWidget> viewWidget,
                               QWidget *parent)
    : FloatingWidgetContainer(parent), mainPanel(nullptr), mPanelPinned(false),
      mPanelEnabled(false), mPanelFullscreenOnly(false), avoidPanelFlag(false),
      mIsFullscreen(false), mInteractionEnabled(false), mAllowPanelInit(false) {
  hideTimer.setSingleShot(true);
  connect(&hideTimer, &QTimer::timeout, this,
          &DocumentWidget::hideFloatingPanelDelayed);

  layout = new QBoxLayout(QBoxLayout::LeftToRight);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);
  setLayout(layout);
  setAttribute(Qt::WA_TranslucentBackground, true);
  setAttribute(Qt::WA_NoSystemBackground, true);
  setMouseTracking(true);
  mViewWidget = viewWidget;
  mViewWidget->setParent(this);
  layout->addWidget(mViewWidget.get());
  mViewWidget.get()->show();
  setFocusProxy(mViewWidget.get());

  setInteractionEnabled(true);

  mainPanel.reset(new MainPanel(this));
  connect(mainPanel.get(), &MainPanel::pinned, this,
          &DocumentWidget::setPanelPinned);
  connect(mViewWidget.get(), &ViewerWidget::clickableEdgePointerMoved, this,
          [this](const QPoint &globalPosition) {
            handlePointerMove(mapFromGlobal(globalPosition));
          });

  connect(settings, &Settings::settingsChanged, this,
          &DocumentWidget::readSettings);
  readSettings();
}

std::shared_ptr<ViewerWidget> DocumentWidget::viewWidget() {
  return mViewWidget;
}

void DocumentWidget::readSettings() {
  setPanelEnabled(settings->panelEnabled());
  mPanelFullscreenOnly = settings->panelFullscreenOnly();
  setPanelPinned(settings->panelPinned());
  mainPanel->readSettings();
}

void DocumentWidget::onFullscreenModeChanged(bool mode) {
  if (settings->panelPosition() == PANEL_TOP ||
      settings->panelPosition() == PANEL_RIGHT)
    mainPanel->setExitButtonEnabled(mode);
  else
    mainPanel->setExitButtonEnabled(false);
  mIsFullscreen = mode;
}

void DocumentWidget::setPanelPinned(bool mode) {
  if (!mPanelEnabled)
    return;
  if (mode)
    cancelFloatingPanelHide();
  if (!mode) { // unpin
    if (mPanelPinned)
      layout->removeWidget(mainPanel.get());
    mainPanel->setLayoutManaged(false);
  } else { // pin
    layout->insertWidget(1, mainPanel.get());
    switch (settings->panelPosition()) {
    case PANEL_TOP:
      layout->setDirection(QBoxLayout::BottomToTop);
      break;
    case PANEL_BOTTOM:
      layout->setDirection(QBoxLayout::TopToBottom);
      break;
    case PANEL_LEFT:
      layout->setDirection(QBoxLayout::RightToLeft);
      break;
    case PANEL_RIGHT:
      layout->setDirection(QBoxLayout::LeftToRight);
      break;
    }
    mainPanel->setLayoutManaged(true);
    if (isSizeAllowed()) {
      mainPanel->show();
    } else {
      mainPanel->hide();
    }
  }
  mPanelPinned = mode;
}

bool DocumentWidget::panelPinned() { return mPanelPinned; }

std::shared_ptr<ThumbnailStripProxy> DocumentWidget::thumbPanel() {
  return mainPanel->getThumbnailStrip();
}

void DocumentWidget::hideFloatingPanel() { hideFloatingPanel(false); }

void DocumentWidget::hideFloatingPanel(bool animated) {
  if (!mPanelPinned) {
    if (animated)
      scheduleFloatingPanelHide(PanelHideSource::WindowExit);
    else {
      cancelFloatingPanelHide();
      mainPanel->hide();
    }
  }
}

void DocumentWidget::scheduleFloatingPanelHide(PanelHideSource source) {
  if (mPanelPinned || mainPanel->isHidden())
    return;

  // Give a real window/taskbar exit one full delay, but do not let the
  // subsequent Leave/WindowDeactivate pair extend that deadline again.
  const bool windowExitNeedsFullDelay =
      source == PanelHideSource::WindowExit &&
      mHideTimerSource != PanelHideSource::WindowExit;
  if (!hideTimer.isActive() || windowExitNeedsFullDelay) {
    mHideTimerSource = source;
    const int delayMs = source == PanelHideSource::WindowExit
                             ? qMax(settings->panelHideDelayMs(),
                                    kWindowExitMinHideDelayMs)
                             : settings->panelHideDelayMs();
    hideTimer.start(delayMs);
  }
}

void DocumentWidget::cancelFloatingPanelHide() {
  hideTimer.stop();
  mHideTimerSource = PanelHideSource::None;
}

void DocumentWidget::hideFloatingPanelDelayed() {
  mHideTimerSource = PanelHideSource::None;
  if (!mPanelPinned) {
    QWidget *widgetUnderMouse = QApplication::widgetAt(QCursor::pos());
    if (widgetUnderMouse &&
        (widgetUnderMouse == mainPanel.get() ||
         mainPanel->isAncestorOf(widgetUnderMouse))) {
      return;
    }
    mainPanel->hideAnimated();
  }
}

void DocumentWidget::setPanelEnabled(bool mode) {
  mPanelEnabled = mode;
  if (!mode) {
    cancelFloatingPanelHide();
    mainPanel->hide();
  } else {
    setupMainPanel();
    updatePanelVisibility();
  }
}

bool DocumentWidget::panelEnabled() { return mPanelEnabled; }

void DocumentWidget::allowPanelInit() { mAllowPanelInit = true; }

void DocumentWidget::setupMainPanel() {
  if (mPanelEnabled && mAllowPanelInit)
    mainPanel->setupThumbnailStrip();
}

void DocumentWidget::setInteractionEnabled(bool mode) {
  mInteractionEnabled = mode;
  if (!mode && !mPanelPinned) {
    cancelFloatingPanelHide();
    mainPanel->hide();
  }
}

void DocumentWidget::mouseMoveEvent(QMouseEvent *event) {
  event->ignore();
  if (mPanelPinned)
    return;
  // ignore if we are doing something with the mouse (zoom / drag)
  if (event->buttons() != Qt::NoButton) {
    if (mainPanel->triggerRect().contains(event->pos()))
      avoidPanelFlag = true;
    return;
  }

  handlePointerMove(event->pos());
}

void DocumentWidget::handlePointerMove(const QPoint &position) {
  if (!mInteractionEnabled || mPanelPinned)
    return;

  const QRect triggerRect = mainPanel->triggerRect();
  const QRect hoverRect = triggerRect.adjusted(
      -kPanelHoverMarginPx, -kPanelHoverMarginPx,
      kPanelHoverMarginPx, kPanelHoverMarginPx);

  if (hoverRect.contains(position))
    cancelFloatingPanelHide();

  // show on hover event
  if (mPanelEnabled && (mIsFullscreen || !mPanelFullscreenOnly) &&
      isSizeAllowed()) {
    if (triggerRect.contains(position) && !avoidPanelFlag) {
      mainPanel->showAnimated();
    }
  }
  // Schedule fade-out after the pointer leaves the panel.
  if (!mainPanel->isHidden()) {
    // leaveEvent which misfires on HiDPI (rounding error somewhere?)
    // add a few px of buffer area to avoid bugs
    // it still fcks up Fitts law as the buttons are not receiving hover on
    // screen border

    // alright this also only works when in root window. sad.
    if (!hoverRect.contains(position))
      scheduleFloatingPanelHide(PanelHideSource::PointerExit);
  }
  if (!triggerRect.contains(position))
    avoidPanelFlag = false;
}

void DocumentWidget::enterEvent(QEnterEvent *event) {
  QWidget::enterEvent(event);
  // we can't track move events outside the window (without additional timer),
  // so just hook the panel event here
  handlePointerMove(mapFromGlobal(QCursor::pos()));
}

void DocumentWidget::leaveEvent(QEvent *event) {
  QWidget::leaveEvent(event);
  // qDebug() << cursor().pos() << this->rect();
  //  this misfires on hidpi.
  // instead do the panel hiding in MW::leaveEvent  (it works properly in root
  // window) mainPanel->hide();
  avoidPanelFlag = false;
}

bool DocumentWidget::isSizeAllowed() const {
  const QWidget *w = window();
  if (w) {
    return w->width() >= MIN_WINDOW_WIDTH_FOR_PANEL &&
           w->height() >= MIN_WINDOW_HEIGHT_FOR_PANEL;
  }
  return true;
}

void DocumentWidget::updatePanelVisibility() {
  if (!isSizeAllowed()) {
    if (mainPanel && !mainPanel->isHidden()) {
      cancelFloatingPanelHide();
      mainPanel->hide();
    }
  } else {
    if (mPanelEnabled && mPanelPinned && mainPanel && mainPanel->isHidden()) {
      mainPanel->show();
    }
  }
}

void DocumentWidget::resizeEvent(QResizeEvent *event) {
  FloatingWidgetContainer::resizeEvent(event);
  updatePanelVisibility();
}
