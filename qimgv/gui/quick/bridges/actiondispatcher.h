#pragma once

#include <QString>
#include <QStringList>

class QInputEvent;

// Seam between ActionBridge and the application's action/shortcut system.
// The application implements it over ActionManager
// (gui/quick/adapters/actionmanagerdispatcher.h); tests use a fake.
//
// GUI thread only.
class IActionDispatcher {
public:
  virtual ~IActionDispatcher() = default;

  // Names of all actions, in a stable order.
  [[nodiscard]] virtual QStringList actionNames() const = 0;
  // Primary shortcut of action, or an empty string when it has none.
  [[nodiscard]] virtual QString shortcutFor(const QString &action) const = 0;
  // Runs action; returns false when it is unknown.
  virtual bool invoke(const QString &action) = 0;
  // Runs the action bound to the shortcut that event forms; returns false
  // when no action is bound to it.
  virtual bool processEvent(QInputEvent &event) = 0;

protected:
  IActionDispatcher() = default;
  IActionDispatcher(const IActionDispatcher &) = default;
  IActionDispatcher &operator=(const IActionDispatcher &) = default;
};
