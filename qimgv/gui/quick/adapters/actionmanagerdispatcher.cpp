#include "actionmanagerdispatcher.h"

#include "components/actionmanager/actionmanager.h"

//------------------------------------------------------------------------------
ActionManagerDispatcher::ActionManagerDispatcher(ActionManager &actionManager)
    : mActionManager(actionManager) {}

//------------------------------------------------------------------------------
QStringList ActionManagerDispatcher::actionNames() const {
  return mActionManager.actionList();
}

QString ActionManagerDispatcher::shortcutFor(const QString &action) const {
  return mActionManager.shortcutForAction(action);
}

bool ActionManagerDispatcher::invoke(const QString &action) {
  return mActionManager.invokeAction(action);
}

bool ActionManagerDispatcher::processEvent(QInputEvent &event) {
  return mActionManager.processEvent(&event);
}
