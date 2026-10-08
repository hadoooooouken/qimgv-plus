#include "quickuihost.h"

#include <QDebug>
#include <QLatin1StringView>
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

constexpr QLatin1StringView bridgesModule = "qimgv.bridges"_L1;
constexpr QLatin1StringView settingsBridgeType = "AppSettings"_L1;
constexpr QLatin1StringView themeBridgeType = "Theme"_L1;
constexpr QLatin1StringView actionBridgeType = "Actions"_L1;
} // namespace

//------------------------------------------------------------------------------
QuickUiHost::QuickUiHost(Settings &settings, ActionManager &actionManager)
    : mDispatcher(actionManager),
      mSettingsBridge(BridgeSnapshots::readUiSettings(settings)),
      mThemeBridge(BridgeSnapshots::readTheme(settings)),
      mActionBridge(mDispatcher) {
  // settingsChanged also announces theme switches and shortcut edits; each
  // bridge emits only for what actually changed.
  QObject::connect(&settings, &Settings::settingsChanged, &mSettingsBridge,
                   [this, &settings]() {
                     mSettingsBridge.apply(
                         BridgeSnapshots::readUiSettings(settings));
                   });
  QObject::connect(&settings, &Settings::settingsChanged, &mThemeBridge,
                   [this, &settings]() {
                     mThemeBridge.apply(BridgeSnapshots::readTheme(settings));
                   });
  QObject::connect(&settings, &Settings::settingsChanged, &mActionBridge,
                   &ActionBridge::refresh);
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

  mEngine.loadFromModule(mainWindowModule, mainWindowType);
  if (mEngine.rootObjects().isEmpty()) {
    qCritical() << "QuickUiHost: failed to create" << mainWindowType
                << "from QML module" << mainWindowModule;
    return false;
  }
  return true;
}
