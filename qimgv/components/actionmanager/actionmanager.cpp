#include "actionmanager.h"
#include "settings.h"

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
  actionManager->defaults.insert("Esc", "folderView");
  actionManager->defaults.insert("Space", "toggleFitMode");
  actionManager->defaults.insert("1", "fitWindow");
  actionManager->defaults.insert("2", "fitWidth");
  actionManager->defaults.insert("3", "fitNormal");
  actionManager->defaults.insert("4", "fitHeight");
  actionManager->defaults.insert("R", "resize");
  actionManager->defaults.insert("H", "flipH");
  actionManager->defaults.insert("V", "flipV");
  actionManager->defaults.insert(InputMap::keyNameCtrl() + "+R", "rotateRight");
  actionManager->defaults.insert(InputMap::keyNameCtrl() + "+L", "rotateLeft");
  actionManager->defaults.insert(",", "prevPage");
  actionManager->defaults.insert(".", "nextPage");
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
  actionManager->defaults.insert(InputMap::keyNameShift() + "+C",
                                 "copyViewportClipboard");
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
  actionManager->defaults.insert(InputMap::keyNameShift() + "+N", "cycleScalingFilter");
  actionManager->defaults.insert(InputMap::keyNameShift() + "+P",
                                 "togglePanorama");
  actionManager->defaults.insert(InputMap::keyNameAlt() + "+I", "toggleUpscayl");
  actionManager->defaults.insert(InputMap::keyNameAlt() + "+" + InputMap::keyNameShift() + "+I", "cycleUpscaylModel");
}
//------------------------------------------------------------------------------
void ActionManager::initShortcuts() {
  actionManager->readShortcuts();
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
  bool changed = false;

  // In 0.9.2 the default bindings for these two actions changed; force
  // every pre-0.9.2 install onto the new defaults, once, on the upgrade
  // that crosses that version.
  if (lastVer < QVersionNumber(0, 9, 2)) {
    resetDefaults("print");
    resetDefaults("openSettings");
    changed = true;
  }

  // Actions whose default keybinding is force-assigned on the upgrade
  // that introduces them, and never again afterwards - not on every
  // startup, and not if the user has already claimed the action or the
  // key for something else.
  static const QStringList forceBoundOnIntroduction = {
      "toggleScalingFilter",
      "cycleScalingFilter",
      "copyViewportClipboard",
      "cycleUpscaylModel",
  };

  const QMap<QString, QVersionNumber> &actionVersions = appActions->getMap();

  for (const QString &action : forceBoundOnIntroduction) {
    if (!actionVersions.contains(action))
      continue;

    // Only relevant on the single upgrade that crosses the action's
    // introduction version. On any later upgrade the user has already
    // had a chance to see - and possibly remove - the default, and that
    // choice must stick.
    if (lastVer >= actionVersions.value(action))
      continue;

    const QString key = defaults.key(action, "");
    if (key.isEmpty())
      continue;

    // Respect an explicit user choice: bind only if the action has no
    // shortcut of its own AND the default key is still free.
    if (shortcuts.key(action, "").isEmpty() && !shortcuts.contains(key)) {
      shortcuts.insert(key, action);
      changed = true;
    }
  }

  // shortcuts only reaches disk through an explicit save. Without this,
  // any migration above would be lost on the very next launch, since
  // adjustFromVersion() will not run again for a version boundary it has
  // already passed.
  if (changed)
    saveShortcuts();
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
  QString name = actionName;
  if (name == "fitWindowStretch") {
    name = "fitHeight";
  }
  ActionType type = validateAction(name);
  if (type == ActionType::ACTION_NORMAL) {
    QMetaObject::invokeMethod(this, name.toLatin1().constData(),
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
  QString name = actionName;
  if (name == "fitWindowStretch") {
    name = "fitHeight";
  }
  if (appActions->getMap().contains(name))
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
  // Migrate legacy "fitWindowStretch" shortcuts to "fitHeight"
  for (auto i = shortcuts.begin(); i != shortcuts.end(); ++i) {
    if (i.value() == "fitWindowStretch") {
      i.value() = "fitHeight";
    }
  }
  if (shortcuts.isEmpty()) {
    resetDefaults();
  }
  actionManager->validateShortcuts();

  // If the user doesn't have a shortcut for prevPage, and comma is not bound, assign it.
  if (shortcuts.key("prevPage", "").isEmpty() && !shortcuts.contains(",")) {
    shortcuts.insert(",", "prevPage");
  }

  // If the user doesn't have a shortcut for nextPage, and period is not bound, assign it.
  if (shortcuts.key("nextPage", "").isEmpty() && !shortcuts.contains(".")) {
    shortcuts.insert(".", "nextPage");
  }

  // If the user doesn't have a shortcut for toggleUpscayl, and Alt+I is not bound to anything else,
  // we bind it to Alt+I by default.
  QString altI = InputMap::keyNameAlt() + "+I";
  if (shortcuts.key("toggleUpscayl", "").isEmpty() && !shortcuts.contains(altI)) {
    shortcuts.insert(altI, "toggleUpscayl");
  }
}
//------------------------------------------------------------------------------
bool ActionManager::processEvent(QInputEvent *event) {
  return actionManager->invokeActionForShortcut(
      ShortcutBuilder::fromEvent(event));
}
