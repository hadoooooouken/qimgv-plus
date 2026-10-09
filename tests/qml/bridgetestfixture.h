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

// Records what ActionBridge forwards instead of running actions.
class FakeActionDispatcher final : public IActionDispatcher {
public:
  FakeActionDispatcher();

  [[nodiscard]] QStringList actionNames() const override;
  [[nodiscard]] QString shortcutFor(const QString &action) const override;
  bool invoke(const QString &action) override;
  bool processEvent(QInputEvent &event) override;

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
  QList<QUrl> mLastDroppedUrls;
};
