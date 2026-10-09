#pragma once

#include <QQmlApplicationEngine>

#include <memory>
#include <optional>

#include "components/viewmode/viewmodecontroller.h"
#include "gui/ports/uievents.h"
#include "gui/ports/uiports.h"
#include "gui/quick/adapters/actionmanagerdispatcher.h"
#include "gui/quick/adapters/interimports.h"
#include "gui/quick/adapters/placeholderdirectoryview.h"
#include "gui/quick/adapters/quickmainwindowcontroller.h"
#include "gui/quick/adapters/quickvieweractions.h"
#include "gui/quick/adapters/quickviewerport.h"
#include "gui/quick/bridges/actionbridge.h"
#include "gui/quick/bridges/settingsbridge.h"
#include "gui/quick/bridges/themebridge.h"
#include "gui/quick/ui/imageviewportcontroller.h"
#include "gui/quick/ui/mainwindowshell.h"

class ActionManager;
class QQuickWindow;
class Settings;

// Composition root of the Qt Quick UI (--ui=quick), the counterpart of
// WidgetUi. Owns the global-service bridges, the image viewport controller
// with its viewer port and actions, the view mode state, the inbound
// UiEvents, the port adapters and the QML engine; keeps the bridges and the
// viewport in sync with Settings and creates the main window from the
// qimgv.ui module. Core talks to this UI only through ports().
//
// Must be destroyed before QApplication and after every Core built from
// ports(); settings and actionManager must outlive it.
class QuickUiHost {
public:
  QuickUiHost(Settings &settings, ActionManager &actionManager);
  ~QuickUiHost();
  QuickUiHost(const QuickUiHost &) = delete;
  QuickUiHost &operator=(const QuickUiHost &) = delete;

  // Applies the graphics API, creates the (hidden) main window and
  // configures its pipeline cache. Returns false when the window could not
  // be created; the QML errors are logged by the engine and the failure
  // itself through qCritical(). Core shows the window through its port.
  [[nodiscard]] bool start();

  // The ports of this UI; std::nullopt until start() succeeded.
  [[nodiscard]] std::optional<UiPorts> ports();

private:
  // Publishes the singleton instances to the engine. Returns false (and logs
  // through qCritical()) when the engine rejects one of them.
  [[nodiscard]] bool registerBridges();
  void applyGraphicsApi();
  void configurePipelineCache(QQuickWindow &window);
  void forwardViewportEvents();
  void onSettingsChanged();

  Settings &mSettings;
  ActionManager &mActionManager;
  UiEvents mEvents;
  ViewModeController mViewMode;
  LoggingNotificationPort mNotifications;
  DecliningDialogPort mDialogs;
  std::shared_ptr<PlaceholderDirectoryView> mThumbnailPanelView;
  std::shared_ptr<PlaceholderDirectoryView> mFolderView;
  // The bridges, the viewport controller and the window shell are declared
  // before the engine so that they outlive it, as
  // QQmlEngine::setExternalSingletonInstance() and the main window's
  // required properties require.
  ActionManagerDispatcher mDispatcher;
  SettingsBridge mSettingsBridge;
  ThemeBridge mThemeBridge;
  ActionBridge mActionBridge;
  ImageViewportController mViewport;
  QuickViewerPort mViewerPort;
  QuickViewerActions mViewerActions;
  MainWindowShell mWindowShell;
  QQmlApplicationEngine mEngine;
  // Works on the window the engine owns, so it is destroyed first.
  std::unique_ptr<QuickMainWindowController> mWindowController;
};
