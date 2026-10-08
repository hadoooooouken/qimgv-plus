#include <QtQml/QQmlExtensionPlugin>
#include <QtQuickTest/quicktest.h>

// qimgv.ui is a static QML module; see gui/quick/quickuihost.cpp.
Q_IMPORT_QML_PLUGIN(qimgv_uiPlugin)

QUICK_TEST_MAIN(qimgv_qml_tests)
