#include "fullscreenchromecontroller.h"

#include <QLocale>

namespace {
// The controls stay out of the way of a panel at the top or right.
bool controlsAllowedWith(const PanelSettings &panel) {
    return !panel.enabled || panel.position == SettingsEnums::PanelPosition::Bottom ||
           panel.position == SettingsEnums::PanelPosition::Left;
}
} // namespace

FullscreenChromeController::FullscreenChromeController(QObject *parent) : QObject(parent) {
    mInfoBarTimer.setSingleShot(true);
    mInfoBarTimer.setInterval(kHideTimeoutMs);
    mControlsTimer.setSingleShot(true);
    mControlsTimer.setInterval(kHideTimeoutMs);
    connect(&mInfoBarTimer, &QTimer::timeout, this, [this]() { setInfoBarShown(false); });
    connect(&mControlsTimer, &QTimer::timeout, this, &FullscreenChromeController::onControlsTimeout);
    updateTexts();
}

bool FullscreenChromeController::isInfoBarActive() const {
    return mInfoBarActive;
}

bool FullscreenChromeController::isInfoBarShown() const {
    return mInfoBarShown;
}

bool FullscreenChromeController::areControlsActive() const {
    return mControlsActive;
}

bool FullscreenChromeController::areControlsShown() const {
    return mControlsShown;
}

QString FullscreenChromeController::positionText() const {
    return mText.position;
}

QString FullscreenChromeController::nameText() const {
    return mText.name;
}

QString FullscreenChromeController::detailsText() const {
    return mText.details;
}

void FullscreenChromeController::setFullscreen(bool fullscreen) {
    if (mFullscreen == fullscreen)
        return;
    mFullscreen = fullscreen;
    updateActive();
}

void FullscreenChromeController::applySettings(const UiSettingsSnapshot &settings) {
    const bool infoBarEnabled = settings.overlays.infoBarFullscreen;
    const bool controlsAllowed = controlsAllowedWith(settings.panel);
    if (infoBarEnabled == mInfoBarEnabled && controlsAllowed == mControlsAllowed)
        return;
    mInfoBarEnabled = infoBarEnabled;
    mControlsAllowed = controlsAllowed;
    updateActive();
}

void FullscreenChromeController::setFileInfo(const ShellFileInfo &info) {
    mInfo = info;
    updateTexts();
    if (mInfoBarActive)
        showInfoBar();
}

void FullscreenChromeController::setViewMode(ViewMode mode) {
    if (mViewMode == mode)
        return;
    mViewMode = mode;
    updateTexts();
}

void FullscreenChromeController::toggleInfoBar() {
    if (!mFullscreen)
        return;
    mInfoBarEnabled = !mInfoBarEnabled;
    emit infoBarSettingToggled(mInfoBarEnabled);
    updateActive();
}

void FullscreenChromeController::pointerMoved() {
    if (mInfoBarActive)
        showInfoBar();
    if (mControlsActive) {
        setControlsShown(true);
        if (mControlsHovered)
            mControlsTimer.stop();
        else
            mControlsTimer.start();
    }
}

void FullscreenChromeController::setControlsHovered(bool hovered) {
    mControlsHovered = hovered;
    if (!mControlsActive)
        return;
    if (hovered) {
        mControlsTimer.stop();
        setControlsShown(true);
    } else {
        mControlsTimer.start();
    }
}

void FullscreenChromeController::updateActive() {
    const bool infoBarActive = mFullscreen && mInfoBarEnabled;
    const bool controlsActive = mFullscreen && mControlsAllowed;
    if (infoBarActive != mInfoBarActive) {
        mInfoBarActive = infoBarActive;
        emit infoBarActiveChanged();
        if (mInfoBarActive) {
            showInfoBar();
        } else {
            mInfoBarTimer.stop();
            setInfoBarShown(false);
        }
    }
    if (controlsActive != mControlsActive) {
        mControlsActive = controlsActive;
        emit controlsActiveChanged();
        if (mControlsActive) {
            showControls();
        } else {
            mControlsTimer.stop();
            setControlsShown(false);
        }
    }
}

void FullscreenChromeController::updateTexts() {
    const FullscreenInfoText text = fullscreenInfoFor(mInfo, mViewMode, QLocale());
    if (text == mText)
        return;
    mText = text;
    emit infoTextChanged();
}

void FullscreenChromeController::showInfoBar() {
    setInfoBarShown(true);
    mInfoBarTimer.start();
}

void FullscreenChromeController::showControls() {
    setControlsShown(true);
    mControlsTimer.start();
}

void FullscreenChromeController::setInfoBarShown(bool shown) {
    if (mInfoBarShown == shown)
        return;
    mInfoBarShown = shown;
    emit infoBarShownChanged();
}

void FullscreenChromeController::setControlsShown(bool shown) {
    if (mControlsShown == shown)
        return;
    mControlsShown = shown;
    emit controlsShownChanged();
}

void FullscreenChromeController::onControlsTimeout() {
    if (mControlsHovered)
        return;
    setControlsShown(false);
}
