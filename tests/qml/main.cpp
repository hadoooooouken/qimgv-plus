#include <QLatin1StringView>
#include <QObject>
#include <QQmlEngine>
#include <QtQml/QQmlExtensionPlugin>
#include <QtQuickTest/quicktest.h>

#include "bridgetestfixture.h"

// The qimgv.* QML modules are static; see gui/quick/quickuihost.cpp.
Q_IMPORT_QML_PLUGIN(qimgv_bridgesPlugin)
Q_IMPORT_QML_PLUGIN(qimgv_uiPlugin)
Q_IMPORT_QML_PLUGIN(qimgv_renderPlugin)

namespace {
using namespace Qt::StringLiterals;

constexpr QLatin1StringView bridgesModule = "qimgv.bridges"_L1;
constexpr QLatin1StringView testsModule = "qimgv.tests"_L1;
} // namespace

// Gives every test engine its own fixture and publishes the fixture's bridges
// as the qimgv.bridges singletons, as QuickUiHost does in the application.
class QmlTestSetup : public QObject {
  Q_OBJECT

public slots:
  void qmlEngineAvailable(QQmlEngine *engine) {
    // Owned by the engine (QObject parent): destroyed with it, after the
    // engine's own QML teardown.
    auto *fixture = new BridgeTestFixture(engine);
    const bool registered =
        engine->setExternalSingletonInstance(bridgesModule, "AppSettings"_L1,
                                             &fixture->settingsBridge()) &&
        engine->setExternalSingletonInstance(bridgesModule, "Theme"_L1,
                                             &fixture->themeBridge()) &&
        engine->setExternalSingletonInstance(bridgesModule, "Actions"_L1,
                                             &fixture->actionBridge()) &&
        engine->setExternalSingletonInstance(testsModule, "Fixture"_L1,
                                             fixture);
    if (!registered)
      qFatal("QmlTestSetup: failed to register the test singletons");
  }
};

QUICK_TEST_MAIN_WITH_SETUP(qimgv_qml_tests, QmlTestSetup)

#include "main.moc"
