#pragma once

#include <QQmlApplicationEngine>

#include <memory>
#include <optional>

#include "components/viewertoggles/appviewertogglestore.h"
#include "components/viewertoggles/viewertoggles.h"
#include "components/viewmode/viewmodecontroller.h"
#include "gui/ports/uievents.h"
#include "gui/ports/uiports.h"
#include "gui/quick/adapters/actionmanagerdispatcher.h"
#include "gui/quick/adapters/directoryviewadapter.h"
#include "gui/quick/adapters/quickcontextmenuactions.h"
#include "gui/quick/adapters/quickcropactions.h"
#include "gui/quick/adapters/quickdialogport.h"
#include "gui/quick/adapters/quickfolderviewactions.h"
#include "gui/quick/adapters/quickmainwindowcontroller.h"
#include "gui/quick/adapters/quickoverlayactions.h"
#include "gui/quick/adapters/quickvieweractions.h"
#include "gui/quick/adapters/quickviewerport.h"
#include "gui/quick/bridges/actionbridge.h"
#include "gui/quick/bridges/settingsbridge.h"
#include "gui/quick/bridges/themebridge.h"
#include "gui/quick/ui/crop/cropcontroller.h"
#include "gui/quick/ui/dialogs/dialogcoordinator.h"
#include "gui/quick/ui/folderview/foldergridcontroller.h"
#include "gui/quick/ui/folderview/folderviewcontroller.h"
#include "gui/quick/ui/imageviewportcontroller.h"
#include "gui/quick/ui/mainwindowshell.h"
#include "gui/quick/ui/menus/contextmenumodel.h"
#include "gui/quick/ui/overlays/overlaycoordinator.h"
#include "gui/quick/ui/settings/settingsdialogcontroller.h"
#include "gui/settingseditor/appsettingsstore.h"
#include "gui/quick/ui/thumbnails/thumbnailpanelcontroller.h"

class ActionManager;
class QQuickWindow;
class ScriptManager;
class Settings;

// Composition root of the Qt Quick UI. Owns the global-service bridges,
// the image viewport controller with its viewer port and actions, the
// viewer's Upscayl and HDR toggles (ViewerToggles), the overlays
// (OverlayCoordinator, which
// is also the notification port) with their actions, the thumbnail panel
// (its directory view, the DirectoryViewAdapter, and its
// ThumbnailPanelController), the context menu (ContextMenuModel), the crop
// mode (CropController) and the folder view (its grid's directory view, the
// FolderGridController and the FolderViewController) with their actions,
// the modal dialogs (DialogCoordinator, shown through QuickDialogPort), the
// settings window (SettingsDialogController over the shared
// SettingsEditorModel and its application stores), the
// view mode state, the inbound UiEvents, the port adapters and the QML
// engine; keeps the bridges, the viewport, the overlays, the panel, the menu,
// the crop mode and the folder view in sync with Settings and creates the
// main window from the qimgv.ui module. Core talks to this UI only through
// ports().
//
// Must be destroyed before QGuiApplication and after every Core built from
// ports(); settings, actionManager and scriptManager must outlive it.
class QuickUiHost {
public:
  QuickUiHost(Settings &settings, ActionManager &actionManager,
              ScriptManager &scriptManager);
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
  void connectViewerToggles();
  void forwardOverlayEvents();
  void connectThumbnailPanel();
  void connectFolderView();
  void connectSettingsDialog();
  void onSettingsChanged();

  Settings &mSettings;
  ActionManager &mActionManager;
  UiEvents mEvents;
  ViewModeController mViewMode;
  std::shared_ptr<DirectoryViewAdapter> mThumbnailPanelView;
  std::shared_ptr<DirectoryViewAdapter> mFolderGridView;
  // The bridges, the viewport controller, the overlays, the thumbnail panel,
  // the context menu, the crop mode, the folder view, the dialogs, the
  // settings window and the window shell are declared before the engine so
  // that they outlive it, as
  // QQmlEngine::setExternalSingletonInstance() and the main window's
  // required properties require.
  ActionManagerDispatcher mDispatcher;
  SettingsBridge mSettingsBridge;
  ThemeBridge mThemeBridge;
  ActionBridge mActionBridge;
  ImageViewportController mViewport;
  QuickViewerPort mViewerPort;
  QuickViewerActions mViewerActions;
  AppViewerToggleStore mViewerToggleStore;
  ViewerToggles mViewerToggles;
  OverlayCoordinator mOverlays;
  QuickOverlayActions mOverlayActions;
  ThumbnailPanelController mThumbnailPanel;
  ContextMenuModel mContextMenu;
  QuickContextMenuActions mContextMenuActions;
  CropController mCrop;
  QuickCropActions mCropActions;
  FolderGridController mFolderGrid;
  FolderViewController mFolderView;
  QuickFolderViewActions mFolderViewActions;
  DialogCoordinator mDialogCoordinator;
  QuickDialogPort mDialogs;
  AppSettingsStore mSettingsStore;
  AppShortcutStore mShortcutStore;
  SettingsEditorModel mSettingsEditor;
  SettingsDialogController mSettingsDialog;
  MainWindowShell mWindowShell;
  QQmlApplicationEngine mEngine;
  // Works on the window the engine owns, so it is destroyed first.
  std::unique_ptr<QuickMainWindowController> mWindowController;
};
