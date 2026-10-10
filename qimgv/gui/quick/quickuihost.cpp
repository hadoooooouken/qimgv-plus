#include "quickuihost.h"

#include <QDebug>
#include <QDir>
#include <QGuiApplication>
#include <QLatin1StringView>
#include <QQuickGraphicsConfiguration>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QVariant>
#include <QtQml/QQmlExtensionPlugin>

#include "components/actionmanager/actionmanager.h"
#include "gui/quick/adapters/bridgesnapshots.h"
#include "settings.h"
#include "utils/startuptiming.h"

// The QML modules are static libraries; importing their static plugins keeps
// the linker from discarding the module registration and resources.
Q_IMPORT_QML_PLUGIN(qimgv_bridgesPlugin)
Q_IMPORT_QML_PLUGIN(qimgv_stylePlugin)
Q_IMPORT_QML_PLUGIN(qimgv_uiPlugin)
Q_IMPORT_QML_PLUGIN(qimgv_renderPlugin)

namespace {
using namespace Qt::StringLiterals;

constexpr QLatin1StringView mainWindowModule = "qimgv.ui"_L1;
constexpr QLatin1StringView mainWindowType = "Main"_L1;
constexpr QLatin1StringView viewportControllerProperty = "viewportController"_L1;
constexpr QLatin1StringView windowShellProperty = "windowShell"_L1;
constexpr QLatin1StringView overlaysProperty = "overlays"_L1;
constexpr QLatin1StringView thumbnailPanelProperty = "thumbnailPanel"_L1;
constexpr QLatin1StringView contextMenuProperty = "contextMenu"_L1;
constexpr QLatin1StringView cropProperty = "crop"_L1;
constexpr QLatin1StringView folderViewProperty = "folderView"_L1;
constexpr QLatin1StringView dialogsProperty = "dialogs"_L1;
constexpr QLatin1StringView settingsDialogProperty = "settingsDialog"_L1;

constexpr QLatin1StringView bridgesModule = "qimgv.bridges"_L1;
constexpr QLatin1StringView settingsBridgeType = "AppSettings"_L1;
constexpr QLatin1StringView themeBridgeType = "Theme"_L1;
constexpr QLatin1StringView actionBridgeType = "Actions"_L1;

// Pipeline cache file in the application cache directory, one per graphics
// API (the cached data is API specific).
constexpr QLatin1StringView pipelineCacheFilePattern = "quick-pipeline-%1.cache"_L1;

// Disables the DXGI vertical blank thread of the Qt Windows platform plugin.
constexpr char noVblankThreadVariable[] = "QT_D3D_NO_VBLANK_THREAD";

struct GraphicsApiInfo {
  QSGRendererInterface::GraphicsApi api;
  QLatin1StringView name;
};

GraphicsApiInfo graphicsApiInfo(QuickGraphicsApi api) {
  switch (api) {
  case QuickGraphicsApi::Direct3D12:
    return {QSGRendererInterface::Direct3D12, "d3d12"_L1};
  case QuickGraphicsApi::Vulkan:
    return {QSGRendererInterface::Vulkan, "vulkan"_L1};
  case QuickGraphicsApi::Direct3D11:
    break;
  }
  return {QSGRendererInterface::Direct3D11, "d3d11"_L1};
}
} // namespace

//------------------------------------------------------------------------------
QuickUiHost::QuickUiHost(Settings &settings, ActionManager &actionManager,
                         ScriptManager &scriptManager)
    : mSettings(settings),
      mActionManager(actionManager),
      mViewMode(settings.defaultViewMode()),
      mThumbnailPanelView(std::make_shared<DirectoryViewAdapter>()),
      mFolderGridView(std::make_shared<DirectoryViewAdapter>()),
      mDispatcher(actionManager),
      mSettingsBridge(BridgeSnapshots::readUiSettings(settings)),
      mThemeBridge(BridgeSnapshots::readTheme(settings)),
      mActionBridge(mDispatcher),
      mViewport(BridgeSnapshots::readUiSettings(settings)),
      mViewerPort(mViewport),
      mViewerActions(actionManager, settings, mViewport),
      mViewerToggleStore(settings),
      mViewerToggles(mViewerToggleStore),
      mOverlays(BridgeSnapshots::readUiSettings(settings)),
      mOverlayActions(actionManager, settings, mOverlays, mViewport),
      mThumbnailPanel(*mThumbnailPanelView, BridgeSnapshots::readUiSettings(settings)),
      mContextMenu(mDispatcher),
      mContextMenuActions(actionManager, scriptManager, mContextMenu, mViewport),
      mCrop(BridgeSnapshots::readUiSettings(settings)),
      mCropActions(QuickCropContext{
          .crop = mCrop,
          .viewport = mViewport,
          .thumbnailPanel = mThumbnailPanel,
          .contextMenu = mContextMenu,
          .events = mEvents,
          .settings = settings,
      }),
      mFolderGrid(*mFolderGridView, BridgeSnapshots::readUiSettings(settings)),
      mFolderView(mFolderGrid, BridgeSnapshots::readUiSettings(settings), QDir::homePath()),
      mFolderViewActions(QuickFolderViewContext{
          .folderView = mFolderView,
          .grid = mFolderGrid,
          .gridView = *mFolderGridView,
          .events = mEvents,
          .settings = settings,
          .actions = actionManager,
      }),
      mDialogs(mDialogCoordinator, settings),
      mSettingsStore(settings, actionManager, scriptManager),
      mShortcutStore(actionManager, scriptManager),
      mSettingsEditor(mSettingsStore, mShortcutStore),
      mSettingsDialog(mSettingsEditor) {
  // settingsChanged also announces theme switches and shortcut edits; each
  // receiver acts only on what actually changed.
  QObject::connect(&settings, &Settings::settingsChanged, &mSettingsBridge,
                   [this]() { onSettingsChanged(); });
  forwardViewportEvents();
  connectViewerToggles();
  forwardOverlayEvents();
  connectThumbnailPanel();
  connectFolderView();
  connectSettingsDialog();
}

QuickUiHost::~QuickUiHost() = default;

//------------------------------------------------------------------------------
void QuickUiHost::forwardViewportEvents() {
  UiEvents *events = &mEvents;
  QObject::connect(&mViewport, &ImageViewportController::upscaleRequested,
                   events, &UiEvents::upscaleRequested);
  QObject::connect(&mViewport, &ImageViewportController::renderingSettled,
                   events, &UiEvents::documentRenderingSettled);
  QObject::connect(&mViewport, &ImageViewportController::draggedOut, events,
                   &UiEvents::draggedOut);
  QObject::connect(&mViewport, &ImageViewportController::nextImageRequested,
                   events, &UiEvents::nextImageRequested);
  QObject::connect(&mViewport, &ImageViewportController::prevImageRequested,
                   events, &UiEvents::prevImageRequested);

  NotificationOverlayModel *messages = mOverlays.messages();
  QObject::connect(&mViewerActions, &QuickViewerActions::notificationRequested,
                   messages, &NotificationOverlayModel::showNotification);
  QObject::connect(&mViewport, &ImageViewportController::playbackError,
                   messages, [messages](const QString &message) {
                     messages->showError(message);
                   });
}

//------------------------------------------------------------------------------
// The Upscayl and HDR tone mapping actions.
void QuickUiHost::connectViewerToggles() {
  ViewerToggles *toggles = &mViewerToggles;
  QObject::connect(&mActionManager, &ActionManager::toggleUpscayl, toggles,
                   &ViewerToggles::toggleUpscayl);
  QObject::connect(&mActionManager, &ActionManager::cycleUpscaylModel, toggles,
                   &ViewerToggles::cycleUpscaylModel);
  QObject::connect(&mActionManager, &ActionManager::toggleHdrToneMapping, toggles,
                   &ViewerToggles::toggleHdrToneMapping);
  QObject::connect(toggles, &ViewerToggles::notificationRequested,
                   mOverlays.messages(), &NotificationOverlayModel::showNotification);
  QObject::connect(toggles, &ViewerToggles::upscaledCropHideRequested, &mViewport,
                   &ImageViewportController::hideUpscaledCrop);
}

//------------------------------------------------------------------------------
// Requests made in the overlays go to Core.
void QuickUiHost::forwardOverlayEvents() {
  UiEvents *events = &mEvents;
  CopyTargetsModel *copyTargets = mOverlays.copyTargets();
  QObject::connect(copyTargets, &CopyTargetsModel::copyRequested, events,
                   &UiEvents::copyRequested);
  QObject::connect(copyTargets, &CopyTargetsModel::moveRequested, events,
                   &UiEvents::moveRequested);
  QObject::connect(mOverlays.renamePrompt(),
                   &RenamePromptController::renameRequested, events,
                   &UiEvents::renameRequested);
  QObject::connect(mOverlays.colorAdjustmentsEditor(),
                   &ColorAdjustmentsEditor::applyRequested, events,
                   &UiEvents::colorAdjustmentsApplyRequested);
  QObject::connect(&mOverlays, &OverlayCoordinator::saveRequested, events,
                   &UiEvents::saveRequested);
  QObject::connect(&mOverlays, &OverlayCoordinator::saveAsRequested, events,
                   &UiEvents::saveAsRequested);
  QObject::connect(&mOverlays, &OverlayCoordinator::discardEditsRequested,
                   events, &UiEvents::discardEditsRequested);
}

//------------------------------------------------------------------------------
// The panel content is created once the first document was rendered, so it
// competes with nothing on the way to the first frame; a pinned panel holds
// its space from the start. Thumbnails are requested for the application's
// device pixel ratio, like the widget strip.
void QuickUiHost::connectThumbnailPanel() {
  mThumbnailPanel.setLabelFont(QGuiApplication::font());
  mThumbnailPanel.setDevicePixelRatio(qGuiApp->devicePixelRatio());
  QObject::connect(&mThumbnailPanel, &ThumbnailPanelController::pinRequested,
                   &mSettings, &Settings::setPanelPinned);
  QObject::connect(
      &mViewport, &ImageViewportController::renderingSettled, &mThumbnailPanel,
      [this]() { mThumbnailPanel.allowCreation(); },
      Qt::SingleShotConnection);
}

//------------------------------------------------------------------------------
// The grid requests thumbnails for the application's device pixel ratio and
// lays its labels out with the application font, like the widget grid.
void QuickUiHost::connectFolderView() {
  mFolderGrid.setLabelFont(QGuiApplication::font());
  mFolderGrid.setDevicePixelRatio(qGuiApp->devicePixelRatio());
}

//------------------------------------------------------------------------------
// The settings window opens on "openSettings" and on the context menu's
// "Configure menu" (on the scripts page). Clearing the thumbnail cache goes
// to Core, which clears it before the request returns (direct connection).
void QuickUiHost::connectSettingsDialog() {
  QObject::connect(&mActionManager, &ActionManager::openSettings, &mSettingsDialog,
                   [this]() {
                     mContextMenu.close();
                     mSettingsDialog.show(SettingsEditorModel::Page::General);
                   });
  QObject::connect(&mContextMenu, &ContextMenuModel::scriptSettingsRequested,
                   &mSettingsDialog, [this]() {
                     mSettingsDialog.show(SettingsEditorModel::Page::Scripts);
                   });
  QObject::connect(&mSettingsStore, &AppSettingsStore::clearThumbnailCacheRequested,
                   &mEvents, &UiEvents::clearThumbnailCacheRequested);
}

//------------------------------------------------------------------------------
void QuickUiHost::onSettingsChanged() {
  const UiSettingsSnapshot snapshot = BridgeSnapshots::readUiSettings(mSettings);
  mSettingsBridge.apply(snapshot);
  mViewport.applySettings(snapshot);
  mOverlays.applySettings(snapshot);
  mThumbnailPanel.applySettings(snapshot);
  mCrop.applySettings(snapshot);
  mFolderView.applySettings(snapshot);
  mThemeBridge.apply(BridgeSnapshots::readTheme(mSettings));
  mActionBridge.refresh();
  mContextMenu.refreshShortcuts();
}

//------------------------------------------------------------------------------
bool QuickUiHost::registerBridges() {
  const bool registered =
      mEngine.setExternalSingletonInstance(bridgesModule, settingsBridgeType,
                                           &mSettingsBridge) &&
      mEngine.setExternalSingletonInstance(bridgesModule, themeBridgeType,
                                           &mThemeBridge) &&
      mEngine.setExternalSingletonInstance(bridgesModule, actionBridgeType,
                                           &mActionBridge);
  if (!registered) {
    qCritical() << "QuickUiHost: failed to register the singletons of QML "
                   "module"
                << bridgesModule;
  }
  return registered;
}

//------------------------------------------------------------------------------
// Must run before the first QQuickWindow is created.
void QuickUiHost::applyGraphicsApi() {
  const QSGRendererInterface::GraphicsApi api =
      graphicsApiInfo(mSettings.quickGraphicsApi()).api;
  // Workaround for Qt 6.12: its DXGI vertical blank thread (used for the
  // update requests of Direct3D windows) stops delivering them on some
  // systems (seen on an NVIDIA GeForce RTX 3060 at 164 Hz, also with Qt's own
  // qml tool): after the first frame no further frame is rendered and every
  // animation and QML timer stalls. Without it, update requests fall back to
  // a timer and the render loop still paces frames with the swap chain's
  // vertical sync. The service reads the variable when it is first used,
  // which is after this point; a value set by the user is kept.
  const bool direct3D = api == QSGRendererInterface::Direct3D11 ||
                        api == QSGRendererInterface::Direct3D12;
  if (direct3D && !qEnvironmentVariableIsSet(noVblankThreadVariable))
    qputenv(noVblankThreadVariable, "1");
  QQuickWindow::setGraphicsApi(api);
}

//------------------------------------------------------------------------------
// Must run before the window is exposed for the first time. The cache is
// read when the scene graph initializes and written when the window releases
// its graphics resources.
void QuickUiHost::configurePipelineCache(QQuickWindow &window) {
  const QString cacheFile =
      mSettings.tmpDir() +
      QString(pipelineCacheFilePattern)
          .arg(graphicsApiInfo(mSettings.quickGraphicsApi()).name);
  QQuickGraphicsConfiguration configuration = window.graphicsConfiguration();
  configuration.setPipelineCacheLoadFile(cacheFile);
  configuration.setPipelineCacheSaveFile(cacheFile);
  window.setGraphicsConfiguration(configuration);
}

//------------------------------------------------------------------------------
bool QuickUiHost::start() {
  if (!registerBridges())
    return false;

  applyGraphicsApi();
  mEngine.setInitialProperties(
      {{viewportControllerProperty, QVariant::fromValue(&mViewport)},
       {windowShellProperty, QVariant::fromValue(&mWindowShell)},
       {overlaysProperty, QVariant::fromValue(&mOverlays)},
       {thumbnailPanelProperty, QVariant::fromValue(&mThumbnailPanel)},
       {contextMenuProperty, QVariant::fromValue(&mContextMenu)},
       {cropProperty, QVariant::fromValue(&mCrop)},
       {folderViewProperty, QVariant::fromValue(&mFolderView)},
       {dialogsProperty, QVariant::fromValue(&mDialogCoordinator)},
       {settingsDialogProperty, QVariant::fromValue(&mSettingsDialog)}});
  mEngine.loadFromModule(mainWindowModule, mainWindowType);
  const QList<QObject *> roots = mEngine.rootObjects();
  QQuickWindow *window =
      roots.isEmpty() ? nullptr : qobject_cast<QQuickWindow *>(roots.constFirst());
  if (!window) {
    qCritical() << "QuickUiHost: failed to create" << mainWindowType
                << "from QML module" << mainWindowModule;
    return false;
  }

  configurePipelineCache(*window);
  QObject::connect(
      window, &QQuickWindow::frameSwapped, window,
      []() { logStartupMilestone(u"Qt Quick UI: first frame presented"); },
      static_cast<Qt::ConnectionType>(Qt::DirectConnection |
                                      Qt::SingleShotConnection));

  mWindowController = std::make_unique<QuickMainWindowController>(
      QuickMainWindowContext{
          .window = *window,
          .shell = mWindowShell,
          .viewport = mViewport,
          .overlays = mOverlays,
          .thumbnailPanel = mThumbnailPanel,
          .contextMenu = mContextMenu,
          .crop = mCrop,
          .folderView = mFolderView,
          .folderViewActions = mFolderViewActions,
          .viewMode = mViewMode,
          .events = mEvents,
          .settings = mSettings,
          .actions = mActionManager,
      });
  QObject::connect(&mViewerPort, &QuickViewerPort::documentShown,
                   mWindowController.get(),
                   &QuickMainWindowController::fitWindowToDocument);
  return true;
}

//------------------------------------------------------------------------------
std::optional<UiPorts> QuickUiHost::ports() {
  if (!mWindowController) {
    qCritical() << "QuickUiHost: the ports are requested before the main window"
                   " was created";
    return std::nullopt;
  }
  return UiPorts{
      *mOverlays.messages(),
      mDialogs,
      mViewerPort,
      *mWindowController,
      *mWindowController,
      mViewMode,
      mEvents,
      mThumbnailPanelView,
      mFolderGridView,
  };
}
