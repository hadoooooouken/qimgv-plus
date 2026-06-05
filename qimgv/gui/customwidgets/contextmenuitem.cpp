#include "contextmenuitem.h"

void ContextMenuItem::setAction(QString text) {
    this->mAction = text;
    setShortcutText(actionManager->shortcutForAction(mAction));
}

void ContextMenuItem::onPress() {
    emit pressed();
    if(!mAction.isEmpty())
        actionManager->invokeAction(mAction);
}
