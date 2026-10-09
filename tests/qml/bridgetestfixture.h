#pragma once

#include <QMap>
#include <QObject>
#include <QPoint>
#include <QPointF>
#include <QQuickWindow>
#include <QString>
#include <QUrl>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include "gui/quick/bridges/actionbridge.h"
#include "gui/quick/bridges/actiondispatcher.h"
#include "gui/quick/bridges/settingsbridge.h"
#include "gui/quick/bridges/themebridge.h"
#include "gui/quick/ui/imageviewportcontroller.h"
#include "gui/quick/ui/mainwindowshell.h"
#include "gui/quick/ui/overlays/overlaycoordinator.h"
#include "gui/quick/ui/thumbnails/thumbnailpanelcontroller.h"

// Records what ActionBridge forwards instead of running actions.
class FakeActionDispatcher final : public IActionDispatcher {
public:
  FakeActionDispatcher();

  [[nodiscard]] QStringList actionNames() const override;
  [[nodiscard]] QString shortcutFor(const QString &action) const override;
  bool invoke(const QString &action) override;
  bool processEvent(QInputEvent &event) override;
  [[nodiscard]] QString shortcutText(QInputEvent &event) const override;
  [[nodiscard]] QString keyText(const QKeyEvent &event) const override;

  void setShortcut(const QString &action, const QString &shortcut);

  QString lastInvoked;
  int lastKey = 0;
  int lastModifiers = 0;
  QPoint lastWheelAngleDelta;
  // QEvent::Type and Qt::MouseButton of the mouse events, oldest first.
  QVariantList mouseEventTypes;
  QVariantList mouseButtons;

private:
  QMap<QString, QString> mShortcuts; // <action, shortcut>
};

// Per-engine test services: the three bridges over a fake dispatcher and
// fixed snapshots, an image viewport controller, plus the hooks the tests use
// to change them. Published as the Fixture singleton of the qimgv.tests
// module.
class BridgeTestFixture : public QObject {
  Q_OBJECT
  QML_NAMED_ELEMENT(Fixture)
  QML_SINGLETON
  QML_UNCREATABLE("Provided by QmlTestSetup")
  Q_PROPERTY(QString lastInvoked READ lastInvoked FINAL)
  Q_PROPERTY(int lastKey READ lastKey FINAL)
  Q_PROPERTY(int lastModifiers READ lastModifiers FINAL)
  Q_PROPERTY(QPoint lastWheelAngleDelta READ lastWheelAngleDelta FINAL)
  Q_PROPERTY(QVariantList mouseEventTypes READ mouseEventTypes FINAL)
  Q_PROPERTY(QVariantList mouseButtons READ mouseButtons FINAL)
  Q_PROPERTY(ImageViewportController *viewportController READ viewportController CONSTANT FINAL)
  Q_PROPERTY(MainWindowShell *windowShell READ windowShell CONSTANT FINAL)
  Q_PROPERTY(QList<QUrl> lastDroppedUrls READ lastDroppedUrls FINAL)
  Q_PROPERTY(OverlayCoordinator *overlays READ overlays CONSTANT FINAL)
  Q_PROPERTY(QString lastFileRequest READ lastFileRequest FINAL)
  Q_PROPERTY(ThumbnailPanelController *thumbnailPanel READ thumbnailPanel CONSTANT FINAL)
  Q_PROPERTY(int thumbnailRequestCount READ thumbnailRequestCount FINAL)
  Q_PROPERTY(int requestedThumbnailCount READ requestedThumbnailCount FINAL)
  Q_PROPERTY(int lastActivatedThumbnail READ lastActivatedThumbnail FINAL)
  Q_PROPERTY(int lastPinRequest READ lastPinRequest FINAL)

public:
  explicit BridgeTestFixture(QObject *parent = nullptr);

  [[nodiscard]] SettingsBridge &settingsBridge();
  [[nodiscard]] ThemeBridge &themeBridge();
  [[nodiscard]] ActionBridge &actionBridge();

  [[nodiscard]] QString lastInvoked() const;
  [[nodiscard]] int lastKey() const;
  [[nodiscard]] int lastModifiers() const;
  [[nodiscard]] QPoint lastWheelAngleDelta() const;
  [[nodiscard]] QVariantList mouseEventTypes() const;
  [[nodiscard]] QVariantList mouseButtons() const;
  [[nodiscard]] ImageViewportController *viewportController();
  [[nodiscard]] MainWindowShell *windowShell();
  [[nodiscard]] QList<QUrl> lastDroppedUrls() const;
  [[nodiscard]] OverlayCoordinator *overlays();
  // The last request the overlays made: "copy:<dir>", "move:<dir>",
  // "rename:<name>", "save", "saveAs" or "discard".
  [[nodiscard]] QString lastFileRequest() const;
  [[nodiscard]] ThumbnailPanelController *thumbnailPanel();
  // Requests of the thumbnail model (thumbnailsNeeded) and the items they
  // named, since the last population.
  [[nodiscard]] int thumbnailRequestCount() const;
  [[nodiscard]] int requestedThumbnailCount() const;
  // The last activated item, -1 for none.
  [[nodiscard]] int lastActivatedThumbnail() const;
  // The last pin button request: 1 pin, 0 unpin, -1 none.
  [[nodiscard]] int lastPinRequest() const;

  // Flips viewer.smoothZoom and publishes the snapshot (viewer area only).
  Q_INVOKABLE void toggleSmoothZoom();
  // Publishes the current snapshot again (no area changes).
  Q_INVOKABLE void reapplySettings();
  // Switches between the application's dark and light theme.
  Q_INVOKABLE void switchTheme();
  // Rebinds action and refreshes the actions bridge.
  Q_INVOKABLE void setShortcut(const QString &action, const QString &shortcut);
  // Shows a plain image of width x height pixels in the viewport, as a new
  // document (fitted with the default fit mode).
  Q_INVOKABLE void showTestImage(int width, int height);
  // Sets the window shell state, as the Quick UI host does.
  Q_INVOKABLE void setFolderViewActive(bool active);
  Q_INVOKABLE void setFullscreen(bool fullscreen);
  // Drags a file with url from another application onto window at
  // position (window coordinates) and drops it there, with the drag and drop
  // events the platform sends. Returns whether the drop was accepted.
  Q_INVOKABLE bool dropExternalFile(QQuickWindow *window, QPointF position,
                                    const QUrl &url);
  // Forgets the input the dispatcher recorded.
  Q_INVOKABLE void clearInputLog();

  // Overlay requests as the actions and the shell port make them.
  Q_INVOKABLE void toggleCopy();
  Q_INVOKABLE void toggleMove();
  Q_INVOKABLE void toggleImageInfo();
  Q_INVOKABLE void toggleRename(const QString &currentName);
  Q_INVOKABLE void toggleColorAdjustments();
  Q_INVOKABLE void toggleCasSettings();
  Q_INVOKABLE void setSaveConfirmVisible(bool visible);
  Q_INVOKABLE void showMessage(const QString &text);
  // Metadata of count entries "Name <i>" / "Value <i>".
  Q_INVOKABLE void setMetadataEntries(int count);
  Q_INVOKABLE void pointerMoved(QPointF position);
  // Closes every overlay, hides the message and forgets the last request.
  Q_INVOKABLE void closeOverlays();
  // Thumbnail panel: enabled with kTestPreviewsSize previews and a short
  // hide delay, pinned or floating, at position (SettingsEnums::
  // PanelPosition), with the extended style or not.
  Q_INVOKABLE void configureThumbnailPanel(bool pinned, int position, bool extended);
  Q_INVOKABLE void allowThumbnailPanelCreation();
  Q_INVOKABLE void setPanelWindowSize(int width, int height);
  // Populates the strip with count items and forgets the requests.
  Q_INVOKABLE void populateThumbnails(int count);
  Q_INVOKABLE void selectThumbnail(int index);
  // Answers every request not answered yet with a decoded image; returns
  // how many thumbnails were delivered.
  Q_INVOKABLE int deliverRequestedThumbnails();
  Q_INVOKABLE void panelPointerMoved(QPointF position, int buttons);

  // Path of fileName next to the test executable, for images the tests save
  // for review.
  Q_INVOKABLE QString artifactPath(const QString &fileName) const;

private:
  FakeActionDispatcher mDispatcher;
  UiSettingsSnapshot mSettings;
  bool mDark = true;
  SettingsBridge mSettingsBridge;
  ThemeBridge mThemeBridge;
  ActionBridge mActionBridge;
  ImageViewportController mViewport;
  MainWindowShell mWindowShell;
  OverlayCoordinator mOverlays;
  QList<QUrl> mLastDroppedUrls;
  QString mLastFileRequest;
  ThumbnailListModel mThumbnails;
  ThumbnailPanelController mThumbnailPanel;
  QList<int> mUnansweredThumbnails;
  int mThumbnailRequestCount = 0;
  int mRequestedThumbnailCount = 0;
  int mLastActivatedThumbnail = -1;
  int mLastPinRequest = -1;
};
