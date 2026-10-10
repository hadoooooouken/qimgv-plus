#include "thumbnailpanelcontroller.h"

#include <QFontMetrics>
#include <QtGlobal>

namespace Enums = SettingsEnums;

ThumbnailPanelController::ThumbnailPanelController(ThumbnailListModel &model,
                                                   const UiSettingsSnapshot &settings,
                                                   QObject *parent)
    : QObject(parent), mModel(model) {
    mHideTimer.setSingleShot(true);
    connect(&mHideTimer, &QTimer::timeout, this, &ThumbnailPanelController::onHideTimeout);
    applySettings(settings);
}

ThumbnailListModel *ThumbnailPanelController::model() {
    return &mModel;
}

bool ThumbnailPanelController::isEnabled() const {
    return mPanel.enabled;
}

bool ThumbnailPanelController::isCreationAllowed() const {
    return mCreationAllowed;
}

bool ThumbnailPanelController::isShown() const {
    return mShown;
}

bool ThumbnailPanelController::isAnimated() const {
    return mAnimated;
}

bool ThumbnailPanelController::isPinned() const {
    return mPinned;
}

bool ThumbnailPanelController::isDocked() const {
    return mDocked;
}

Enums::PanelPosition ThumbnailPanelController::position() const {
    return mPanel.position;
}

ThumbnailStripLayout ThumbnailPanelController::layout() const {
    return mLayout;
}

bool ThumbnailPanelController::isExitButtonVisible() const {
    return mExitButtonVisible;
}

int ThumbnailPanelController::slideDistance() const {
    return kSlideDistancePx;
}

int ThumbnailPanelController::animationDuration() const {
    return kAnimationDurationMs;
}

QRectF ThumbnailPanelController::panelRect() const {
    const qreal extent = mLayout.panelExtent;
    const qreal width = mWindowSize.width();
    const qreal height = mWindowSize.height();
    switch (mPanel.position) {
    case Enums::PanelPosition::Top:
        return {0.0, 0.0, width, extent};
    case Enums::PanelPosition::Bottom:
        return {0.0, height - extent, width, extent};
    case Enums::PanelPosition::Left:
        return {0.0, 0.0, extent, height};
    case Enums::PanelPosition::Right:
        return {width - extent, 0.0, extent, height};
    }
    return {};
}

//------------------------------------------------------------------------------
// Application state

void ThumbnailPanelController::applySettings(const UiSettingsSnapshot &settings) {
    const bool wasEnabled = mPanel.enabled;
    mPanel = settings.panel;
    mFolderView = settings.folderView;
    mViewer = settings.viewer;
    if (wasEnabled != mPanel.enabled) {
        emit enabledChanged();
        if (!mPanel.enabled) {
            cancelHide();
            setShown(false, false);
        }
    }
    updateLayout();
    configureModel();
    updateExitButton();
    setPinned(mPanel.pinned);
}

void ThumbnailPanelController::setLabelFont(const QFont &font) {
    if (mLabelFont == font)
        return;
    mLabelFont = font;
    updateLayout();
    configureModel();
}

void ThumbnailPanelController::setDevicePixelRatio(qreal ratio) {
    if (qFuzzyCompare(mDevicePixelRatio, ratio))
        return;
    mDevicePixelRatio = ratio;
    configureModel();
}

void ThumbnailPanelController::allowCreation() {
    if (mCreationAllowed)
        return;
    mCreationAllowed = true;
    emit creationAllowedChanged();
}

// Switching between window and fullscreen hides a floating panel.
void ThumbnailPanelController::setFullscreen(bool fullscreen) {
    if (mFullscreen == fullscreen)
        return;
    mFullscreen = fullscreen;
    hideNow();
    updateExitButton();
}

void ThumbnailPanelController::setFolderViewActive(bool active) {
    if (mFolderViewActive == active)
        return;
    mFolderViewActive = active;
    if (active)
        hideNow();
}

void ThumbnailPanelController::setWindowSize(QSizeF size) {
    if (mWindowSize == size)
        return;
    mWindowSize = size;
    updatePinnedVisibility();
}

void ThumbnailPanelController::pointerMoved(QPointF position, Qt::MouseButtons buttons) {
    mPointerPosition = position;
    mPointerInWindow = true;
    if (mPinned || !mPanel.enabled || mFolderViewActive)
        return;
    const QRectF trigger = panelRect();
    // A press there (a drag of the image, a zoom) does not bring the panel.
    if (buttons != Qt::NoButton) {
        if (trigger.contains(position))
            mAvoidShow = true;
        return;
    }
    if (!mInteractionEnabled)
        return;

    const QRectF hoverArea =
        trigger.adjusted(-kHoverMarginPx, -kHoverMarginPx, kHoverMarginPx, kHoverMarginPx);
    if (hoverArea.contains(position))
        cancelHide();
    if (hoverShowAllowed() && trigger.contains(position) && !mAvoidShow)
        setShown(true, true);
    if (mShown && !hoverArea.contains(position))
        scheduleHide(HideSource::PointerExit);
    if (!trigger.contains(position))
        mAvoidShow = false;
}

void ThumbnailPanelController::pointerLeftWindow() {
    mPointerInWindow = false;
    mAvoidShow = false;
    scheduleHide(HideSource::WindowExit);
}

void ThumbnailPanelController::hideNow() {
    if (mPinned)
        return;
    cancelHide();
    setShown(false, false);
}

void ThumbnailPanelController::setInteractionEnabled(bool enabled) {
    mInteractionEnabled = enabled;
    if (!enabled)
        hideNow();
}

//------------------------------------------------------------------------------
// QML

void ThumbnailPanelController::togglePinned() {
    const bool pinned = !mPinned;
    setPinned(pinned);
    emit pinRequested(pinned);
}

void ThumbnailPanelController::setAnimationRunning(bool running) {
    mModel.setLoadingBlocked(running);
}

QColor ThumbnailPanelController::labelTextColor(const QColor &background) const {
    return thumbnailLabelColor(background);
}

//------------------------------------------------------------------------------

bool ThumbnailPanelController::isSizeAllowed() const {
    return mWindowSize.width() >= kMinimumWindowWidth &&
           mWindowSize.height() >= kMinimumWindowHeight;
}

bool ThumbnailPanelController::hoverShowAllowed() const {
    return mPanel.enabled && (mFullscreen || !mPanel.fullscreenOnly) && isSizeAllowed();
}

void ThumbnailPanelController::setShown(bool shown, bool animated) {
    if (mShown == shown)
        return;
    mShown = shown;
    mAnimated = animated;
    emit shownChanged();
    updateDocked();
}

void ThumbnailPanelController::setPinned(bool pinned) {
    if (mPinned != pinned) {
        mPinned = pinned;
        if (pinned)
            cancelHide();
        emit pinnedChanged();
    }
    updatePinnedVisibility();
    updateDocked();
}

// DocumentWidget::updatePanelVisibility(): a small window hides the panel, a
// pinned panel shows when the window is large enough again.
void ThumbnailPanelController::updatePinnedVisibility() {
    if (!mPanel.enabled)
        return;
    if (!isSizeAllowed()) {
        cancelHide();
        setShown(false, false);
    } else if (mPinned) {
        setShown(true, false);
    }
}

void ThumbnailPanelController::updateDocked() {
    const bool docked = mPanel.enabled && mPinned && mShown;
    if (mDocked == docked)
        return;
    mDocked = docked;
    emit dockedChanged();
}

void ThumbnailPanelController::updateExitButton() {
    const bool visible = mFullscreen && (mPanel.position == Enums::PanelPosition::Top ||
                                         mPanel.position == Enums::PanelPosition::Right);
    if (mExitButtonVisible == visible)
        return;
    mExitButtonVisible = visible;
    emit exitButtonVisibleChanged();
}

void ThumbnailPanelController::updateLayout() {
    const ThumbnailStripLayout layout = thumbnailStripLayoutFor({
        .previewsSize = mPanel.previewsSize,
        .style = mPanel.style,
        .position = mPanel.position,
        .textHeight = QFontMetrics(mLabelFont).height(),
    });
    if (layout == mLayout)
        return;
    mLayout = layout;
    emit layoutChanged();
}

// ThumbnailStrip::readSettings() and ThumbnailView: thumbnails are requested
// at the logical size times the device pixel ratio.
void ThumbnailPanelController::configureModel() {
    mModel.configure({
        .pixelSize = static_cast<int>(mDevicePixelRatio * mLayout.thumbnailSize),
        .crop = mFolderView.squareThumbnails,
        .unloadOffscreen = mPanel.unloadThumbnails,
        .cacheResolution = mPanel.thumbnailResolution,
    });
    mModel.setScrollConfig({
        .horizontal = mLayout.horizontal,
        .itemExtent = mLayout.itemExtent(),
        .thumbnailSize = mLayout.thumbnailSize,
        .centerSelection = mPanel.centerSelection,
        .smoothScroll = mViewer.smoothScroll,
        .wheelSpeed = mViewer.mouseScrollingSpeed,
        .trackpadDetection = mViewer.trackpadDetection,
    });
}

void ThumbnailPanelController::scheduleHide(HideSource source) {
    if (mPinned || !mShown)
        return;
    // A window exit gets one full delay; the leave and deactivation that
    // follow each other do not extend it again.
    const bool windowExitNeedsFullDelay =
        source == HideSource::WindowExit && mHideSource != HideSource::WindowExit;
    if (mHideTimer.isActive() && !windowExitNeedsFullDelay)
        return;
    mHideSource = source;
    const int delayMs = source == HideSource::WindowExit
                            ? qMax(mPanel.hideDelayMs, kWindowExitMinimumHideDelayMs)
                            : mPanel.hideDelayMs;
    mHideTimer.start(delayMs);
}

void ThumbnailPanelController::cancelHide() {
    mHideTimer.stop();
    mHideSource = HideSource::None;
}

void ThumbnailPanelController::onHideTimeout() {
    mHideSource = HideSource::None;
    if (mPinned)
        return;
    // The pointer rests on the panel.
    if (mPointerInWindow && panelRect().contains(mPointerPosition))
        return;
    setShown(false, true);
}
