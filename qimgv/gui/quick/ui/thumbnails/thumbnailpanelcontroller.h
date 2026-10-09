#pragma once

#include <QColor>
#include <QFont>
#include <QObject>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

#include "gui/quick/bridges/uisettings.h"
#include "gui/quick/ui/thumbnails/thumbnaillistmodel.h"
#include "gui/quick/ui/thumbnails/thumbnailstriplayout.h"

// The thumbnail panel of the document view (MainPanel.qml) with the rules of
// the widget DocumentWidget, MainPanel and SlidePanel:
// - the panel exists while the panel setting is on; its content is created
//   once creation is allowed (after the first document was shown), so it
//   adds nothing to the first frame;
// - a pinned panel is part of the layout (docked) and shown while the window
//   is at least kMinimumWindowWidth x kMinimumWindowHeight;
// - an unpinned panel floats over the viewer: it slides in when the pointer
//   enters its area with no button pressed (in fullscreen only, with that
//   setting), and slides out panelHideDelayMs after the pointer left the
//   area grown by kHoverMarginPx - at least kWindowExitMinimumHideDelayMs
//   after the pointer left the window or the window was deactivated. A
//   press in the area (a drag of the image) keeps it hidden until the
//   pointer leaves the area. Entering the folder view hides it at once;
// - the exit button is shown in fullscreen for a panel at the top or right;
// - the pin button publishes the new pinned state to store.
// It also derives the strip layout from the settings and the label font, and
// configures the thumbnail model with it (request size in device pixels,
// crop, unloading, scrolling). While the panel slides, the model requests no
// thumbnails.
//
// Owned by the Quick UI host. GUI thread only.
class ThumbnailPanelController final : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by the Quick UI host")
    Q_PROPERTY(ThumbnailListModel *model READ model CONSTANT FINAL)
    Q_PROPERTY(bool enabled READ isEnabled NOTIFY enabledChanged FINAL)
    Q_PROPERTY(bool creationAllowed READ isCreationAllowed NOTIFY creationAllowedChanged FINAL)
    Q_PROPERTY(bool shown READ isShown NOTIFY shownChanged FINAL)
    Q_PROPERTY(bool animated READ isAnimated NOTIFY shownChanged FINAL)
    Q_PROPERTY(bool pinned READ isPinned NOTIFY pinnedChanged FINAL)
    Q_PROPERTY(bool docked READ isDocked NOTIFY dockedChanged FINAL)
    Q_PROPERTY(SettingsEnums::PanelPosition position READ position NOTIFY layoutChanged FINAL)
    Q_PROPERTY(ThumbnailStripLayout layout READ layout NOTIFY layoutChanged FINAL)
    Q_PROPERTY(bool exitButtonVisible READ isExitButtonVisible NOTIFY exitButtonVisibleChanged FINAL)
    Q_PROPERTY(int slideDistance READ slideDistance CONSTANT FINAL)
    Q_PROPERTY(int animationDuration READ animationDuration CONSTANT FINAL)

public:
    static constexpr int kMinimumWindowWidth = 800;
    static constexpr int kMinimumWindowHeight = 500;
    static constexpr int kHoverMarginPx = 8;
    // Grace period after the pointer left the window (an incidental dip
    // onto the taskbar), independent of a shorter hide delay setting.
    static constexpr int kWindowExitMinimumHideDelayMs = 600;
    // The slide: distance and duration of the show and hide animation.
    static constexpr int kSlideDistancePx = 40;
    static constexpr int kAnimationDurationMs = 300;
    // Text colours on light and dark surfaces, chosen by qGray().
    static constexpr int kLightSurfaceGray = 128;

    // model must outlive the controller; settings: the initial UI settings.
    ThumbnailPanelController(ThumbnailListModel &model, const UiSettingsSnapshot &settings,
                             QObject *parent = nullptr);

    [[nodiscard]] ThumbnailListModel *model();
    [[nodiscard]] bool isEnabled() const;
    [[nodiscard]] bool isCreationAllowed() const;
    [[nodiscard]] bool isShown() const;
    // The last change of shown slides (otherwise it is immediate).
    [[nodiscard]] bool isAnimated() const;
    [[nodiscard]] bool isPinned() const;
    [[nodiscard]] bool isDocked() const;
    [[nodiscard]] SettingsEnums::PanelPosition position() const;
    [[nodiscard]] ThumbnailStripLayout layout() const;
    [[nodiscard]] bool isExitButtonVisible() const;
    [[nodiscard]] int slideDistance() const;
    [[nodiscard]] int animationDuration() const;
    // Area of the shown panel in window coordinates (the trigger area).
    [[nodiscard]] QRectF panelRect() const;

    // --- application state -----------------------------------------------
    void applySettings(const UiSettingsSnapshot &settings);
    // Font of the cell labels (the application font).
    void setLabelFont(const QFont &font);
    // Device pixel ratio the thumbnails are requested for.
    void setDevicePixelRatio(qreal ratio);
    // The panel content may be created from now on.
    void allowCreation();
    void setFullscreen(bool fullscreen);
    void setFolderViewActive(bool active);
    void setWindowSize(QSizeF size);
    // The pointer moved to position (window coordinates) with buttons held.
    void pointerMoved(QPointF position, Qt::MouseButtons buttons);
    // The pointer left the window, or the window was deactivated.
    void pointerLeftWindow();
    // Hides an unpinned panel at once.
    void hideNow();
    // Pointer moves bring the floating panel only while the viewer takes
    // input; turning it off (crop mode) hides an unpinned panel at once.
    void setInteractionEnabled(bool enabled);

    // --- QML -------------------------------------------------------------
    Q_INVOKABLE void togglePinned();
    // The show or hide slide runs.
    Q_INVOKABLE void setAnimationRunning(bool running);
    // Black or white, whichever reads on background (unselected labels).
    Q_INVOKABLE QColor labelTextColor(const QColor &background) const;

signals:
    void enabledChanged();
    void creationAllowedChanged();
    void shownChanged();
    void pinnedChanged();
    void dockedChanged();
    void layoutChanged();
    void exitButtonVisibleChanged();
    // The pin button changed the pinned state; to be stored.
    void pinRequested(bool pinned);

private:
    enum class HideSource { None, PointerExit, WindowExit };

    [[nodiscard]] bool isSizeAllowed() const;
    [[nodiscard]] bool hoverShowAllowed() const;
    void setShown(bool shown, bool animated);
    void setPinned(bool pinned);
    void updatePinnedVisibility();
    void updateDocked();
    void updateExitButton();
    void updateLayout();
    void configureModel();
    void scheduleHide(HideSource source);
    void cancelHide();
    void onHideTimeout();

    ThumbnailListModel &mModel;
    PanelSettings mPanel;
    FolderViewSettings mFolderView;
    ViewerSettings mViewer;
    QFont mLabelFont;
    qreal mDevicePixelRatio = 1.0;
    ThumbnailStripLayout mLayout;
    QSizeF mWindowSize;
    QTimer mHideTimer;
    HideSource mHideSource = HideSource::None;
    bool mCreationAllowed = false;
    bool mShown = false;
    bool mAnimated = false;
    bool mPinned = false;
    bool mDocked = false;
    bool mFullscreen = false;
    bool mFolderViewActive = false;
    bool mInteractionEnabled = true;
    bool mExitButtonVisible = false;
    // A press started in the trigger area: no hover show until it is left.
    bool mAvoidShow = false;
    QPointF mPointerPosition;
    bool mPointerInWindow = false;
};
