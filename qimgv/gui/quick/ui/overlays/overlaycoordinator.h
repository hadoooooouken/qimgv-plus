#pragma once

#include <QObject>
#include <QPointF>
#include <QtQml/qqmlregistration.h>

#include "gui/ports/shellport.h"
#include "gui/quick/bridges/uisettings.h"
#include "gui/quick/ui/overlays/cassettingseditor.h"
#include "gui/quick/ui/overlays/coloradjustmentseditor.h"
#include "gui/quick/ui/overlays/copytargetsmodel.h"
#include "gui/quick/ui/overlays/fullscreenchromecontroller.h"
#include "gui/quick/ui/overlays/imageinfomodel.h"
#include "gui/quick/ui/overlays/notificationoverlaymodel.h"
#include "gui/quick/ui/overlays/overlaystate.h"
#include "gui/quick/ui/overlays/renamepromptcontroller.h"

// The overlays of the Qt Quick UI: owns their visibility (OverlayState) and
// content models and applies the rules of the widget main window (MW) that
// concern more than one overlay:
// - entering the folder view closes copy / move, rename, colour adjustments
//   and CAS settings and hides the image info, which returns with the
//   document view;
// - image info, copy / move, colour adjustments and CAS settings open only in
//   the document view, copy / move only while an image is displayed;
// - copy / move and rename take the keyboard focus while open;
// - the save confirmation opens only when the showSaveOverlay setting is on;
// - colour adjustments and CAS settings open at the pointer;
// - the save confirmation and the copy / move list move to the top when the
//   thumbnail panel would cover them.
// The content models decide everything inside their overlay. Main.qml gets
// the coordinator as a required property and lays the overlays out.
//
// Owned by the Quick UI host. GUI thread only.
class OverlayCoordinator final : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by the Quick UI host")
    Q_PROPERTY(NotificationOverlayModel *messages READ messages CONSTANT FINAL)
    Q_PROPERTY(FullscreenChromeController *fullscreenChrome READ fullscreenChrome CONSTANT FINAL)
    Q_PROPERTY(OverlayState *imageInfo READ imageInfo CONSTANT FINAL)
    Q_PROPERTY(ImageInfoModel *imageInfoModel READ imageInfoModel CONSTANT FINAL)
    Q_PROPERTY(OverlayState *saveConfirm READ saveConfirm CONSTANT FINAL)
    Q_PROPERTY(OverlayState *copy READ copy CONSTANT FINAL)
    Q_PROPERTY(CopyTargetsModel *copyTargets READ copyTargets CONSTANT FINAL)
    Q_PROPERTY(OverlayState *rename READ rename CONSTANT FINAL)
    Q_PROPERTY(RenamePromptController *renamePrompt READ renamePrompt CONSTANT FINAL)
    Q_PROPERTY(OverlayState *colorAdjustments READ colorAdjustments CONSTANT FINAL)
    Q_PROPERTY(ColorAdjustmentsEditor *colorAdjustmentsEditor READ colorAdjustmentsEditor CONSTANT FINAL)
    Q_PROPERTY(OverlayState *casSettings READ casSettings CONSTANT FINAL)
    Q_PROPERTY(CasSettingsEditor *casSettingsEditor READ casSettingsEditor CONSTANT FINAL)
    Q_PROPERTY(bool keyboardOverlayOpen READ isKeyboardOverlayOpen NOTIFY keyboardOverlayOpenChanged FINAL)
    Q_PROPERTY(bool saveConfirmAtTop READ isSaveConfirmAtTop NOTIFY placementChanged FINAL)
    Q_PROPERTY(bool copyAtTop READ isCopyAtTop NOTIFY placementChanged FINAL)

public:
    // settings: the initial UI settings (applySettings()).
    explicit OverlayCoordinator(const UiSettingsSnapshot &settings, QObject *parent = nullptr);

    [[nodiscard]] NotificationOverlayModel *messages();
    [[nodiscard]] FullscreenChromeController *fullscreenChrome();
    [[nodiscard]] OverlayState *imageInfo();
    [[nodiscard]] ImageInfoModel *imageInfoModel();
    [[nodiscard]] OverlayState *saveConfirm();
    [[nodiscard]] OverlayState *copy();
    [[nodiscard]] CopyTargetsModel *copyTargets();
    [[nodiscard]] OverlayState *rename();
    [[nodiscard]] RenamePromptController *renamePrompt();
    [[nodiscard]] OverlayState *colorAdjustments();
    [[nodiscard]] ColorAdjustmentsEditor *colorAdjustmentsEditor();
    [[nodiscard]] OverlayState *casSettings();
    [[nodiscard]] CasSettingsEditor *casSettingsEditor();

    // An overlay that takes the keyboard focus is open.
    [[nodiscard]] bool isKeyboardOverlayOpen() const;
    [[nodiscard]] bool isSaveConfirmAtTop() const;
    [[nodiscard]] bool isCopyAtTop() const;

    // --- state of the window and the document ----------------------------
    void applySettings(const UiSettingsSnapshot &settings);
    void setFolderViewActive(bool active);
    void setDocumentDisplayed(bool displayed);
    void setFullscreen(bool fullscreen);
    void setFileInfo(const ShellFileInfo &info);
    void setMetadata(const MetadataEntries &entries);
    // The pointer moved to position (window coordinates); windowActive: the
    // window has the focus (the fullscreen chrome follows only then).
    void pointerMoved(QPointF position, bool windowActive);

    // --- actions and shell port requests ---------------------------------
    void toggleImageInfo();
    void toggleCopy();
    void toggleMove();
    void toggleRename(const QString &currentName);
    void toggleColorAdjustments();
    void toggleCasSettings();
    void setSaveConfirmVisible(bool visible);

    // --- save confirmation buttons (QML) ---------------------------------
    Q_INVOKABLE void requestSave();
    Q_INVOKABLE void requestSaveAs();
    Q_INVOKABLE void requestDiscard();

signals:
    void keyboardOverlayOpenChanged();
    void placementChanged();
    void saveRequested();
    void saveAsRequested();
    void discardEditsRequested();
    // The copy / move list opens for the first time; the destinations must be
    // set (CopyTargetsModel::setTargets()) before this returns.
    void copyTargetsNeeded();
    // A CAS parameter was edited; the viewer and the settings follow.
    void casParametersEdited(const CasParameters &parameters);

private:
    void toggleCopyMode(CopyTargetsModel::Mode mode);
    void openAtPointer(OverlayState &state);
    void setPlacement(bool saveConfirmAtTop, bool copyAtTop);

    NotificationOverlayModel mMessages;
    FullscreenChromeController mFullscreenChrome;
    OverlayState mImageInfo{false};
    ImageInfoModel mImageInfoModel;
    OverlayState mSaveConfirm{false};
    OverlayState mCopy{true};
    CopyTargetsModel mCopyTargets;
    OverlayState mRename{true};
    RenamePromptController mRenamePrompt;
    OverlayState mColorAdjustments{false};
    ColorAdjustmentsEditor mColorAdjustmentsEditor;
    OverlayState mCasSettings{false};
    CasSettingsEditor mCasSettingsEditor;

    bool mFolderViewActive = false;
    bool mDocumentDisplayed = false;
    bool mImageInfoHiddenByFolderView = false;
    bool mShowSaveConfirm = false;
    bool mSaveConfirmAtTop = false;
    bool mCopyAtTop = false;
    // The current CAS parameters (edits are stored without a settings
    // notification) and the ones of the last settings snapshot.
    CasParameters mCasParameters;
    CasParameters mSettingsCasParameters;
    QPointF mPointerPosition;
};
