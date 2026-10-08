#pragma once

#include <QQmlApplicationEngine>

// Owns the QML engine of the Qt Quick UI (--ui=quick) and creates its main
// window from the qimgv.ui module. Must be destroyed before QApplication.
class QuickUiHost {
public:
  QuickUiHost() = default;
  QuickUiHost(const QuickUiHost &) = delete;
  QuickUiHost &operator=(const QuickUiHost &) = delete;

  // Creates and shows the main window. Returns false when the window could
  // not be created; the QML errors are logged by the engine and the failure
  // itself through qCritical().
  [[nodiscard]] bool start();

private:
  QQmlApplicationEngine engine;
};
