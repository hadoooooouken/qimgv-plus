#include "actionmanagerdispatcher.h"

#include <QKeyEvent>

#include "components/actionmanager/actionmanager.h"
#include "shortcutbuilder.h"

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

QString ActionManagerDispatcher::shortcutText(QInputEvent &event) const {
  return ShortcutBuilder::fromEvent(&event);
}

QString ActionManagerDispatcher::keyText(const QKeyEvent &event) const {
  return mActionManager.keyForNativeScancode(event.nativeScanCode());
}
