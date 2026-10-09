#pragma once

#include <QQmlApplicationEngine>

#include "gui/quick/adapters/actionmanagerdispatcher.h"
#include "gui/quick/adapters/quickvieweractions.h"
#include "gui/quick/adapters/quickviewerport.h"
#include "gui/quick/bridges/actionbridge.h"
#include "gui/quick/bridges/settingsbridge.h"
#include "gui/quick/bridges/themebridge.h"
#include "gui/quick/ui/imageviewportcontroller.h"

class ActionManager;
class Settings;

// Composition root of the Qt Quick UI (--ui=quick). Owns the global-service
// bridges, the image viewport controller with its viewer port and actions,
// and the QML engine; keeps the bridges and the viewport in sync with
// Settings and creates the main window from the qimgv.ui module. Must be
// destroyed before QApplication; settings and actionManager must outlive it.
//
// Core is not attached yet (S2.1): the viewer port and the viewport's
// outbound signals (scaling requests, next/previous image, drag-out,
// rendering settled) are wired to UiEvents then.
class QuickUiHost {
public:
  QuickUiHost(Settings &settings, ActionManager &actionManager);
  QuickUiHost(const QuickUiHost &) = delete;
  QuickUiHost &operator=(const QuickUiHost &) = delete;

  // Creates and shows the main window. Returns false when the window could
  // not be created; the QML errors are logged by the engine and the failure
  // itself through qCritical().
  [[nodiscard]] bool start();

private:
  // Publishes the singleton instances to the engine. Returns false (and logs
  // through qCritical()) when the engine rejects one of them.
  [[nodiscard]] bool registerBridges();
  void onSettingsChanged();

  Settings &mSettings;
  // The bridges and the viewport controller are declared before the engine
  // so that they outlive it, as QQmlEngine::setExternalSingletonInstance()
  // and the main window's required viewportController property require.
  ActionManagerDispatcher mDispatcher;
  SettingsBridge mSettingsBridge;
  ThemeBridge mThemeBridge;
  ActionBridge mActionBridge;
  ImageViewportController mViewport;
  QuickViewerPort mViewerPort;
  QuickViewerActions mViewerActions;
  QQmlApplicationEngine mEngine;
};
