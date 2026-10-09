#include "overlaycoordinator.h"

namespace {
using SettingsEnums::PanelPosition;

// The save confirmation sits at the bottom unless a panel is there or at a
// side, where it would compete with it.
bool saveConfirmAtTopWith(const PanelSettings &panel) {
    return panel.enabled &&
           (panel.position == PanelPosition::Bottom || panel.position == PanelPosition::Left ||
            panel.position == PanelPosition::Right);
}

// The copy / move list sits at the bottom left unless the panel is at the
// bottom.
bool copyAtTopWith(const PanelSettings &panel) {
    return panel.enabled && panel.position == PanelPosition::Bottom;
}

CasParameters casParametersOf(const ViewerSettings &viewer) {
    return {.sharpening = static_cast<float>(viewer.casSharpening),
            .contrast = static_cast<float>(viewer.casContrast)};
}
} // namespace

OverlayCoordinator::OverlayCoordinator(const UiSettingsSnapshot &settings, QObject *parent)
    : QObject(parent) {
    for (OverlayState *state : {&mCopy, &mRename}) {
        connect(state, &OverlayState::openChanged, this,
                &OverlayCoordinator::keyboardOverlayOpenChanged);
    }
    connect(&mRenamePrompt, &RenamePromptController::closeRequested, &mRename,
            &OverlayState::close);
    connect(&mCasSettingsEditor, &CasSettingsEditor::parametersEdited, this,
            [this](const CasParameters &parameters) {
                mCasParameters = parameters;
                emit casParametersEdited(parameters);
            });
    mCasParameters = casParametersOf(settings.viewer);
    mSettingsCasParameters = mCasParameters;
    applySettings(settings);
}

NotificationOverlayModel *OverlayCoordinator::messages() { return &mMessages; }
FullscreenChromeController *OverlayCoordinator::fullscreenChrome() { return &mFullscreenChrome; }
OverlayState *OverlayCoordinator::imageInfo() { return &mImageInfo; }
ImageInfoModel *OverlayCoordinator::imageInfoModel() { return &mImageInfoModel; }
OverlayState *OverlayCoordinator::saveConfirm() { return &mSaveConfirm; }
OverlayState *OverlayCoordinator::copy() { return &mCopy; }
CopyTargetsModel *OverlayCoordinator::copyTargets() { return &mCopyTargets; }
OverlayState *OverlayCoordinator::rename() { return &mRename; }
RenamePromptController *OverlayCoordinator::renamePrompt() { return &mRenamePrompt; }
OverlayState *OverlayCoordinator::colorAdjustments() { return &mColorAdjustments; }
ColorAdjustmentsEditor *OverlayCoordinator::colorAdjustmentsEditor() { return &mColorAdjustmentsEditor; }
OverlayState *OverlayCoordinator::casSettings() { return &mCasSettings; }
CasSettingsEditor *OverlayCoordinator::casSettingsEditor() { return &mCasSettingsEditor; }

bool OverlayCoordinator::isKeyboardOverlayOpen() const {
    return mCopy.isOpen() || mRename.isOpen();
}

bool OverlayCoordinator::isSaveConfirmAtTop() const {
    return mSaveConfirmAtTop;
}

bool OverlayCoordinator::isCopyAtTop() const {
    return mCopyAtTop;
}

//------------------------------------------------------------------------------
void OverlayCoordinator::applySettings(const UiSettingsSnapshot &settings) {
    mShowSaveConfirm = settings.overlays.showSaveOverlay;
    setPlacement(saveConfirmAtTopWith(settings.panel), copyAtTopWith(settings.panel));
    mFullscreenChrome.applySettings(settings);
    // An edit made in the overlay is already current; a different value in
    // the settings comes from elsewhere (settings dialog).
    const CasParameters settingsCas = casParametersOf(settings.viewer);
    if (settingsCas != mSettingsCasParameters) {
        mSettingsCasParameters = settingsCas;
        mCasParameters = settingsCas;
    }
}

void OverlayCoordinator::setFolderViewActive(bool active) {
    if (mFolderViewActive == active)
        return;
    mFolderViewActive = active;
    mFullscreenChrome.setViewMode(active ? MODE_FOLDERVIEW : MODE_DOCUMENT);
    if (active) {
        for (OverlayState *state : {&mCopy, &mRename, &mColorAdjustments, &mCasSettings})
            state->close();
        mImageInfoHiddenByFolderView = mImageInfo.isOpen();
        mImageInfo.close();
    } else if (mImageInfoHiddenByFolderView) {
        mImageInfoHiddenByFolderView = false;
        mImageInfo.setOpen(true);
    }
}

void OverlayCoordinator::setDocumentDisplayed(bool displayed) {
    mDocumentDisplayed = displayed;
}

void OverlayCoordinator::setFullscreen(bool fullscreen) {
    mFullscreenChrome.setFullscreen(fullscreen);
}

void OverlayCoordinator::setFileInfo(const ShellFileInfo &info) {
    mRenamePrompt.setName(info.fileName);
    mFullscreenChrome.setFileInfo(info);
}

void OverlayCoordinator::setMetadata(const MetadataEntries &entries) {
    mImageInfoModel.setEntries(entries);
}

void OverlayCoordinator::pointerMoved(QPointF position, bool windowActive) {
    mPointerPosition = position;
    if (windowActive)
        mFullscreenChrome.pointerMoved();
}

//------------------------------------------------------------------------------
void OverlayCoordinator::toggleImageInfo() {
    if (mFolderViewActive)
        return;
    mImageInfo.toggle();
}

void OverlayCoordinator::toggleCopy() {
    toggleCopyMode(CopyTargetsModel::Mode::Copy);
}

void OverlayCoordinator::toggleMove() {
    toggleCopyMode(CopyTargetsModel::Mode::Move);
}

void OverlayCoordinator::toggleCopyMode(CopyTargetsModel::Mode mode) {
    if (!mDocumentDisplayed || mFolderViewActive)
        return;
    if (!mCopyTargets.isLoaded())
        emit copyTargetsNeeded();
    if (mCopyTargets.mode() == mode) {
        mCopy.toggle();
        return;
    }
    mCopyTargets.setMode(mode);
    mCopy.setOpen(true);
}

void OverlayCoordinator::toggleRename(const QString &currentName) {
    if (mRename.isOpen()) {
        mRename.close();
        return;
    }
    mRenamePrompt.setBackdrop(mFolderViewActive);
    mRenamePrompt.setName(currentName);
    mRename.setOpen(true);
}

void OverlayCoordinator::toggleColorAdjustments() {
    if (mFolderViewActive)
        return;
    if (mColorAdjustments.isOpen())
        mColorAdjustments.close();
    else
        openAtPointer(mColorAdjustments);
}

void OverlayCoordinator::toggleCasSettings() {
    if (mFolderViewActive)
        return;
    if (mCasSettings.isOpen()) {
        mCasSettings.close();
        return;
    }
    mCasSettingsEditor.load(mCasParameters);
    openAtPointer(mCasSettings);
}

void OverlayCoordinator::setSaveConfirmVisible(bool visible) {
    if (visible && !mShowSaveConfirm)
        return;
    mSaveConfirm.setOpen(visible);
}

void OverlayCoordinator::requestSave() {
    emit saveRequested();
}

void OverlayCoordinator::requestSaveAs() {
    emit saveAsRequested();
}

void OverlayCoordinator::requestDiscard() {
    emit discardEditsRequested();
}

void OverlayCoordinator::openAtPointer(OverlayState &state) {
    state.setAnchor(mPointerPosition);
    state.setOpen(true);
}

void OverlayCoordinator::setPlacement(bool saveConfirmAtTop, bool copyAtTop) {
    if (mSaveConfirmAtTop == saveConfirmAtTop && mCopyAtTop == copyAtTop)
        return;
    mSaveConfirmAtTop = saveConfirmAtTop;
    mCopyAtTop = copyAtTop;
    emit placementChanged();
}
