#pragma once

#include <QMap>
#include <QObject>
#include <QPoint>
#include <QString>
#include <QtQml/qqmlregistration.h>

#include "gui/quick/bridges/actionbridge.h"
#include "gui/quick/bridges/actiondispatcher.h"
#include "gui/quick/bridges/settingsbridge.h"
#include "gui/quick/bridges/themebridge.h"

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

private:
  QMap<QString, QString> mShortcuts; // <action, shortcut>
};

// Per-engine test services: the three bridges over a fake dispatcher and
// fixed snapshots, plus the hooks tst_bridges.qml uses to change them.
// Published as the Fixture singleton of the qimgv.tests module.
class BridgeTestFixture : public QObject {
  Q_OBJECT
  QML_NAMED_ELEMENT(Fixture)
  QML_SINGLETON
  QML_UNCREATABLE("Provided by QmlTestSetup")
  Q_PROPERTY(QString lastInvoked READ lastInvoked FINAL)
  Q_PROPERTY(int lastKey READ lastKey FINAL)
  Q_PROPERTY(int lastModifiers READ lastModifiers FINAL)
  Q_PROPERTY(QPoint lastWheelAngleDelta READ lastWheelAngleDelta FINAL)

public:
  explicit BridgeTestFixture(QObject *parent = nullptr);

  [[nodiscard]] SettingsBridge &settingsBridge();
  [[nodiscard]] ThemeBridge &themeBridge();
  [[nodiscard]] ActionBridge &actionBridge();

  [[nodiscard]] QString lastInvoked() const;
  [[nodiscard]] int lastKey() const;
  [[nodiscard]] int lastModifiers() const;
  [[nodiscard]] QPoint lastWheelAngleDelta() const;

  // Flips viewer.smoothZoom and publishes the snapshot (viewer area only).
  Q_INVOKABLE void toggleSmoothZoom();
  // Publishes the current snapshot again (no area changes).
  Q_INVOKABLE void reapplySettings();
  // Switches between the dark and the light test theme.
  Q_INVOKABLE void switchTheme();
  // Rebinds action and refreshes the actions bridge.
  Q_INVOKABLE void setShortcut(const QString &action, const QString &shortcut);

private:
  FakeActionDispatcher mDispatcher;
  UiSettingsSnapshot mSettings;
  bool mDark = true;
  SettingsBridge mSettingsBridge;
  ThemeBridge mThemeBridge;
  ActionBridge mActionBridge;
};
