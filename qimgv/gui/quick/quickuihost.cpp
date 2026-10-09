#include "quickuihost.h"

#include <QDebug>
#include <QLatin1StringView>
#include <QVariant>
#include <QtQml/QQmlExtensionPlugin>

#include "gui/quick/adapters/bridgesnapshots.h"
#include "settings.h"

// The QML modules are static libraries; importing their static plugins keeps
// the linker from discarding the module registration and resources.
Q_IMPORT_QML_PLUGIN(qimgv_bridgesPlugin)
Q_IMPORT_QML_PLUGIN(qimgv_uiPlugin)
Q_IMPORT_QML_PLUGIN(qimgv_renderPlugin)

namespace {
using namespace Qt::StringLiterals;

constexpr QLatin1StringView mainWindowModule = "qimgv.ui"_L1;
constexpr QLatin1StringView mainWindowType = "Main"_L1;
constexpr QLatin1StringView viewportControllerProperty = "viewportController"_L1;

constexpr QLatin1StringView bridgesModule = "qimgv.bridges"_L1;
constexpr QLatin1StringView settingsBridgeType = "AppSettings"_L1;
constexpr QLatin1StringView themeBridgeType = "Theme"_L1;
constexpr QLatin1StringView actionBridgeType = "Actions"_L1;
} // namespace

//------------------------------------------------------------------------------
QuickUiHost::QuickUiHost(Settings &settings, ActionManager &actionManager)
    : mSettings(settings),
      mDispatcher(actionManager),
      mSettingsBridge(BridgeSnapshots::readUiSettings(settings)),
      mThemeBridge(BridgeSnapshots::readTheme(settings)),
      mActionBridge(mDispatcher),
      mViewport(BridgeSnapshots::readUiSettings(settings)),
      mViewerPort(mViewport),
      mViewerActions(actionManager, settings, mViewport) {
  // settingsChanged also announces theme switches and shortcut edits; each
  // receiver acts only on what actually changed.
  QObject::connect(&settings, &Settings::settingsChanged, &mSettingsBridge,
                   [this]() { onSettingsChanged(); });

  // Until the Quick UI has its notification overlay (S2.3) and Core (S2.1),
  // viewer messages and playback errors are logged.
  QObject::connect(&mViewerActions, &QuickViewerActions::notificationRequested,
                   &mViewerActions, [](const NotificationRequest &request) {
                     qInfo().noquote() << "QuickUiHost: viewer message:"
                                       << request.text;
                   });
  QObject::connect(&mViewport, &ImageViewportController::playbackError,
                   &mViewport, [](const QString &message) {
                     qWarning().noquote()
                         << "QuickUiHost: animation playback failed:"
                         << message;
                   });
}

//------------------------------------------------------------------------------
void QuickUiHost::onSettingsChanged() {
  const UiSettingsSnapshot snapshot = BridgeSnapshots::readUiSettings(mSettings);
  mSettingsBridge.apply(snapshot);
  mViewport.applySettings(snapshot);
  mThemeBridge.apply(BridgeSnapshots::readTheme(mSettings));
  mActionBridge.refresh();
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
bool QuickUiHost::start() {
  if (!registerBridges())
    return false;

  mEngine.setInitialProperties(
      {{viewportControllerProperty, QVariant::fromValue(&mViewport)}});
  mEngine.loadFromModule(mainWindowModule, mainWindowType);
  if (mEngine.rootObjects().isEmpty()) {
    qCritical() << "QuickUiHost: failed to create" << mainWindowType
                << "from QML module" << mainWindowModule;
    return false;
  }
  return true;
}
