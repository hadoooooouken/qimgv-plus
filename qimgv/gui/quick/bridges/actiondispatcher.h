#pragma once

#include <QString>
#include <QStringList>

class QInputEvent;
class QKeyEvent;

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
  // Shortcut text of the key combination event forms, as shortcuts are
  // stored ("Ctrl+R"); empty when it forms none.
  [[nodiscard]] virtual QString shortcutText(QInputEvent &event) const = 0;
  // Name of the key of event without modifiers, independent of the keyboard
  // layout ("3" for the key in the digit row); empty when unknown.
  [[nodiscard]] virtual QString keyText(const QKeyEvent &event) const = 0;

protected:
  IActionDispatcher() = default;
  IActionDispatcher(const IActionDispatcher &) = default;
  IActionDispatcher &operator=(const IActionDispatcher &) = default;
};
