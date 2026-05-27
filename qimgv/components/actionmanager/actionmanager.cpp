#include "actionmanager.h"

ActionManager *actionManager = nullptr;

ActionManager::ActionManager(QObject *parent) : QObject(parent) {}
//------------------------------------------------------------------------------
ActionManager::~ActionManager() {
  if (actionManager == this) {
    actionManager = nullptr;
  }
}
//------------------------------------------------------------------------------
ActionManager *ActionManager::getInstance() {
  if (!actionManager) {
    actionManager = new ActionManager();
    initDefaults();
    initShortcuts();
  }
  return actionManager;
}
//------------------------------------------------------------------------------
void ActionManager::initDefaults() {
  actionManager->defaults.insert("Right", "nextImage");
  actionManager->defaults.insert("Left", "prevImage");
  actionManager->defaults.insert("XButton2", "nextImage");
  actionManager->defaults.insert("XButton1", "prevImage");
  actionManager->defaults.insert("WheelDown", "zoomOut");
  actionManager->defaults.insert("WheelUp", "zoomIn");
  actionManager->defaults.insert("F", "toggleFullscreen");
  actionManager->defaults.insert("Enter", "toggleFullscreen");
  actionManager->defaults.insert("LMB_DoubleClick", "fitNormal");
  actionManager->defaults.insert("MiddleButton", "folderView");
  actionManager->defaults.insert("Backspace", "folderView");
  actionManager->defaults.insert("Space", "toggleFitMode");
  actionManager->defaults.insert("1", "fitWindow");
  actionManager->defaults.insert("2", "fitWidth");
  actionManager->defaults.insert("3", "fitNormal");
  actionManager->defaults.insert("4", "fitWindowStretch");
  actionManager->defaults.insert("R", "resize");
  actionManager->defaults.insert("H", "flipH");
  actionManager->defaults.insert("V", "flipV");
  actionManager->defaults.insert(InputMap::keyNameCtrl() + "+R", "rotateRight");
  actionManager->defaults.insert(InputMap::keyNameCtrl() + "+L", "rotateLeft");
  actionManager->defaults.insert(InputMap::keyNameCtrl() + "+WheelUp",
                                 "zoomInCursor");
  actionManager->defaults.insert(InputMap::keyNameCtrl() + "+WheelDown",
                                 "zoomOutCursor");
  actionManager->defaults.insert("=", "zoomIn");
  actionManager->defaults.insert("+", "zoomIn");
  actionManager->defaults.insert("-", "zoomOut");
  actionManager->defaults.insert(InputMap::keyNameCtrl() + "+Down", "zoomOut");
  actionManager->defaults.insert(InputMap::keyNameCtrl() + "+Up", "zoomIn");
  actionManager->defaults.insert("Up", "fitNormal");
  actionManager->defaults.insert("Down", "fitWindow");
  actionManager->defaults.insert(InputMap::keyNameCtrl() + "+O", "open");
  actionManager->defaults.insert(InputMap::keyNameCtrl() + "+S", "save");
  actionManager->defaults.insert(InputMap::keyNameCtrl() + "+" +
                                     InputMap::keyNameShift() + "+S",
                                 "saveAs");
  actionManager->defaults.insert(InputMap::keyNameCtrl() + "+W",
                                 "setWallpaper");
  actionManager->defaults.insert("X", "crop");
  actionManager->defaults.insert(InputMap::keyNameCtrl() + "+P", "print");
  actionManager->defaults.insert(InputMap::keyNameAlt() + "+X", "exit");
  actionManager->defaults.insert(InputMap::keyNameCtrl() + "+Q", "exit");
  actionManager->defaults.insert("Esc", "closeFullScreenOrExit");
  actionManager->defaults.insert("Del", "moveToTrash");
  actionManager->defaults.insert(InputMap::keyNameShift() + "+Del",
                                 "removeFile");
  actionManager->defaults.insert("C", "copyFile");
  actionManager->defaults.insert("M", "moveFile");
  actionManager->defaults.insert("Home", "jumpToFirst");
  actionManager->defaults.insert("End", "jumpToLast");
  actionManager->defaults.insert("F5", "reloadImage");
  actionManager->defaults.insert(InputMap::keyNameCtrl() + "+C",
                                 "copyFileClipboard");
  actionManager->defaults.insert(InputMap::keyNameCtrl() + "+" +
                                     InputMap::keyNameShift() + "+C",
                                 "copyPathClipboard");
  actionManager->defaults.insert("F2", "renameFile");
  actionManager->defaults.insert("RMB", "contextMenu");
  actionManager->defaults.insert("Menu", "contextMenu");
  actionManager->defaults.insert("I", "toggleImageInfo");
  actionManager->defaults.insert(InputMap::keyNameCtrl() + "+`",
                                 "toggleShuffle");
  actionManager->defaults.insert(InputMap::keyNameCtrl() + "+D",
                                 "showInDirectory");
  actionManager->defaults.insert("`", "toggleSlideshow");
  actionManager->defaults.insert(InputMap::keyNameCtrl() + "+Z",
                                 "discardEdits");
  actionManager->defaults.insert(InputMap::keyNameShift() + "+Right",
                                 "nextDirectory");
  actionManager->defaults.insert(InputMap::keyNameShift() + "+Left",
                                 "prevDirectory");
  actionManager->defaults.insert(InputMap::keyNameShift() + "+F",
                                 "toggleFullscreenInfoBar");
  actionManager->defaults.insert(InputMap::keyNameCtrl() + "+V", "pasteFile");
  actionManager->defaults.insert("P", "openSettings");
  actionManager->defaults.insert("N", "toggleScalingFilter");
  actionManager->defaults.insert(InputMap::keyNameShift() + "+P",
                                 "togglePanorama");
#ifdef USE_UPSCAYL
  actionManager->defaults.insert(InputMap::keyNameAlt() + "+I", "toggleUpscayl");
#endif
}
//------------------------------------------------------------------------------
void ActionManager::initShortcuts() {
  actionManager->readShortcuts();
  if (actionManager->shortcuts.isEmpty()) {
    actionManager->resetDefaults();
  }
}
//------------------------------------------------------------------------------
void ActionManager::addShortcut(const QString &keys, const QString &action) {
  ActionType type = validateAction(action);
  if (type != ActionType::ACTION_INVALID) {
    actionManager->shortcuts.insert(keys, action);
  }
}
//------------------------------------------------------------------------------
void ActionManager::removeShortcut(const QString &keys) {
  actionManager->shortcuts.remove(keys);
}
//------------------------------------------------------------------------------
QStringList ActionManager::actionList() { return appActions->getList(); }
//------------------------------------------------------------------------------
const QMap<QString, QString> &ActionManager::allShortcuts() {
  return actionManager->shortcuts;
}
//------------------------------------------------------------------------------
void ActionManager::removeAllShortcuts() { shortcuts.clear(); }
//------------------------------------------------------------------------------
void ActionManager::removeAllShortcuts(QString actionName) {
  if (validateAction(actionName) == ActionType::ACTION_INVALID)
    return;

  for (auto i = shortcuts.begin(); i != shortcuts.end();) {
    if (i.value() == actionName)
      i = shortcuts.erase(i);
    else
      ++i;
  }
}
//------------------------------------------------------------------------------
QString ActionManager::keyForNativeScancode(quint32 scanCode) {
  if (inputMap->keys().contains(scanCode)) {
    return inputMap->keys()[scanCode];
  }
  return "";
}
//------------------------------------------------------------------------------
void ActionManager::resetDefaults() {
  actionManager->shortcuts = actionManager->defaults;
}
//------------------------------------------------------------------------------
void ActionManager::resetDefaults(QString action) {
  removeAllShortcuts(action);
  QMapIterator<QString, QString> i(defaults);
  while (i.hasNext()) {
    i.next();
    if (i.value() == action) {
      shortcuts.insert(i.key(), i.value());
      qDebug() << "[ActionManager] new action " << i.value()
               << " - assigning as [" << i.key() << "]";
    }
  }
}
//------------------------------------------------------------------------------
void ActionManager::adjustFromVersion(QVersionNumber lastVer) {
  // All legacy version checks and migrations have been removed
  Q_UNUSED(lastVer);
}
//------------------------------------------------------------------------------
void ActionManager::saveShortcuts() {
  settings->saveShortcuts(actionManager->shortcuts);
}
//------------------------------------------------------------------------------
QString ActionManager::actionForShortcut(const QString &keys) {
  return actionManager->shortcuts[keys];
}

const QString ActionManager::shortcutForAction(QString action) {
  return shortcuts.key(action, "");
}

const QList<QString> ActionManager::shortcutsForAction(QString action) {
  return shortcuts.keys(action);
}
//------------------------------------------------------------------------------
bool ActionManager::invokeAction(const QString &actionName) {
  ActionType type = validateAction(actionName);
  if (type == ActionType::ACTION_NORMAL) {
    QMetaObject::invokeMethod(this, actionName.toLatin1().constData(),
                              Qt::DirectConnection);
    return true;
  } else if (type == ActionType::ACTION_SCRIPT) {
    QString scriptName = actionName;
    scriptName.remove(0, 2);
    emit runScript(scriptName);
    return true;
  }
  return false;
}
//------------------------------------------------------------------------------
bool ActionManager::invokeActionForShortcut(const QString &shortcut) {
  if (!shortcut.isEmpty() && actionManager->shortcuts.contains(shortcut)) {
    return invokeAction(actionManager->shortcuts[shortcut]);
  }
  return false;
}
//------------------------------------------------------------------------------
void ActionManager::validateShortcuts() {
  for (auto i = shortcuts.begin(); i != shortcuts.end();) {
    if (validateAction(i.value()) == ActionType::ACTION_INVALID)
      i = shortcuts.erase(i);
    else
      ++i;
  }
}
//------------------------------------------------------------------------------
inline ActionType ActionManager::validateAction(const QString &actionName) {
  if (appActions->getMap().contains(actionName))
    return ActionType::ACTION_NORMAL;
  if (actionName.startsWith("s:")) {
    QString scriptName = actionName;
    scriptName.remove(0, 2);
    if (scriptManager->scriptExists(scriptName))
      return ActionType::ACTION_SCRIPT;
  }
  return ActionType::ACTION_INVALID;
}
//------------------------------------------------------------------------------
void ActionManager::readShortcuts() {
  settings->readShortcuts(shortcuts);
  actionManager->validateShortcuts();

  // If the user doesn't have a shortcut for toggleScalingFilter, and "N" is not bound to anything else,
  // we bind it to "N" by default.
  if (shortcuts.key("toggleScalingFilter", "").isEmpty() && !shortcuts.contains("N")) {
    shortcuts.insert("N", "toggleScalingFilter");
  }

#ifdef USE_UPSCAYL
  // If the user doesn't have a shortcut for toggleUpscayl, and Alt+I is not bound to anything else,
  // we bind it to Alt+I by default.
  QString altI = InputMap::keyNameAlt() + "+I";
  if (shortcuts.key("toggleUpscayl", "").isEmpty() && !shortcuts.contains(altI)) {
    shortcuts.insert(altI, "toggleUpscayl");
  }
#endif
}
//------------------------------------------------------------------------------
bool ActionManager::processEvent(QInputEvent *event) {
  return actionManager->invokeActionForShortcut(
      ShortcutBuilder::fromEvent(event));
}