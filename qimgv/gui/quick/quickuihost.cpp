#include "quickuihost.h"

#include <QDebug>
#include <QLatin1StringView>
#include <QtQml/QQmlExtensionPlugin>

// The QML modules are static libraries; importing their static plugins keeps
// the linker from discarding the module registration and resources.
Q_IMPORT_QML_PLUGIN(qimgv_uiPlugin)
Q_IMPORT_QML_PLUGIN(qimgv_renderPlugin)

namespace {
using namespace Qt::StringLiterals;

constexpr QLatin1StringView mainWindowModule = "qimgv.ui"_L1;
constexpr QLatin1StringView mainWindowType = "Main"_L1;
} // namespace

//------------------------------------------------------------------------------
bool QuickUiHost::start() {
  engine.loadFromModule(mainWindowModule, mainWindowType);
  if (engine.rootObjects().isEmpty()) {
    qCritical() << "QuickUiHost: failed to create" << mainWindowType
                << "from QML module" << mainWindowModule;
    return false;
  }
  return true;
}
