#pragma once

#include "gui/quick/bridges/actiondispatcher.h"

class ActionManager;

// IActionDispatcher over the application's ActionManager, for ActionBridge.
class ActionManagerDispatcher final : public IActionDispatcher {
public:
  // actionManager must outlive the dispatcher.
  explicit ActionManagerDispatcher(ActionManager &actionManager);

  [[nodiscard]] QStringList actionNames() const override;
  [[nodiscard]] QString shortcutFor(const QString &action) const override;
  bool invoke(const QString &action) override;
  bool processEvent(QInputEvent &event) override;
  [[nodiscard]] QString shortcutText(QInputEvent &event) const override;
  [[nodiscard]] QString keyText(const QKeyEvent &event) const override;

private:
  ActionManager &mActionManager;
};
