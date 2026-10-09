#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

#include "components/shellinfo/fileinfotext.h"
#include "gui/ports/shellport.h"
#include "gui/quick/bridges/uisettings.h"
#include "settings_types.h"

// The window chrome shown only in fullscreen (FullscreenInfoOverlay.qml and
// ControlsOverlay.qml), with the rules of the widget UI:
// - the info bar (position, file name, details) is active in fullscreen
//   while the infoBarFullscreen setting is on; it shows when it becomes
//   active, when the file changes and when the pointer moves, and fades out
//   after kHideTimeoutMs;
// - the controls (folder view, settings, exit) are active in fullscreen
//   when the thumbnail panel is off or at the bottom or left; they show on
//   pointer moves and fade out after kHideTimeoutMs unless hovered.
//
// Owned by OverlayCoordinator. GUI thread only.
class FullscreenChromeController final : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by OverlayCoordinator")
    Q_PROPERTY(bool infoBarActive READ isInfoBarActive NOTIFY infoBarActiveChanged FINAL)
    Q_PROPERTY(bool infoBarShown READ isInfoBarShown NOTIFY infoBarShownChanged FINAL)
    Q_PROPERTY(bool controlsActive READ areControlsActive NOTIFY controlsActiveChanged FINAL)
    Q_PROPERTY(bool controlsShown READ areControlsShown NOTIFY controlsShownChanged FINAL)
    Q_PROPERTY(QString positionText READ positionText NOTIFY infoTextChanged FINAL)
    Q_PROPERTY(QString nameText READ nameText NOTIFY infoTextChanged FINAL)
    Q_PROPERTY(QString detailsText READ detailsText NOTIFY infoTextChanged FINAL)

public:
    // Time without pointer movement after which the chrome fades out.
    static constexpr int kHideTimeoutMs = 2000;

    explicit FullscreenChromeController(QObject *parent = nullptr);

    [[nodiscard]] bool isInfoBarActive() const;
    [[nodiscard]] bool isInfoBarShown() const;
    [[nodiscard]] bool areControlsActive() const;
    [[nodiscard]] bool areControlsShown() const;
    [[nodiscard]] QString positionText() const;
    [[nodiscard]] QString nameText() const;
    [[nodiscard]] QString detailsText() const;

    void setFullscreen(bool fullscreen);
    void applySettings(const UiSettingsSnapshot &settings);
    // A new current file: updates the texts and shows the active info bar.
    void setFileInfo(const ShellFileInfo &info);
    void setViewMode(ViewMode mode);
    // Turns the info bar setting over (fullscreen only) and publishes it.
    void toggleInfoBar();
    // The pointer moved in the active window.
    void pointerMoved();

    // The pointer is over the controls.
    Q_INVOKABLE void setControlsHovered(bool hovered);

signals:
    void infoBarActiveChanged();
    void infoBarShownChanged();
    void controlsActiveChanged();
    void controlsShownChanged();
    void infoTextChanged();
    // The info bar setting changed through toggleInfoBar(); to be stored.
    void infoBarSettingToggled(bool enabled);

private:
    void updateActive();
    void updateTexts();
    void showInfoBar();
    void showControls();
    void setInfoBarShown(bool shown);
    void setControlsShown(bool shown);
    void onControlsTimeout();

    QTimer mInfoBarTimer;
    QTimer mControlsTimer;
    bool mFullscreen = false;
    bool mInfoBarEnabled = false;
    bool mControlsAllowed = false;
    bool mInfoBarActive = false;
    bool mControlsActive = false;
    bool mInfoBarShown = false;
    bool mControlsShown = false;
    bool mControlsHovered = false;
    ShellFileInfo mInfo;
    ViewMode mViewMode = MODE_DOCUMENT;
    FullscreenInfoText mText;
};
