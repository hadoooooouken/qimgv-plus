#include "actionbridge.h"

#include "gui/quick/bridges/actiondispatcher.h"
#include "gui/quick/bridges/qmlinputevents.h"

#include <QDebug>

#include <memory>
#include <utility>

//------------------------------------------------------------------------------
ActionBridge::ActionBridge(IActionDispatcher &dispatcher, QObject *parent)
    : QObject(parent), mDispatcher(dispatcher), mActions(readEntries()) {}

ActionBridge::~ActionBridge() = default;

//------------------------------------------------------------------------------
QAbstractItemModel *ActionBridge::actions() const { return mActions.model(); }

//------------------------------------------------------------------------------
bool ActionBridge::invoke(const QString &name) {
  return mDispatcher.invoke(name);
}

QString ActionBridge::shortcutFor(const QString &name) const {
  return mDispatcher.shortcutFor(name);
}

//------------------------------------------------------------------------------
bool ActionBridge::handleKeyEvent(QObject *event) {
  const std::unique_ptr<QKeyEvent> keyEvent =
      QmlInputEvents::keyPressFrom(event);
  if (!keyEvent) {
    qWarning() << "ActionBridge::handleKeyEvent: not a QML KeyEvent:" << event;
    return false;
  }
  return mDispatcher.processEvent(*keyEvent);
}

QString ActionBridge::shortcutText(QObject *event) const {
  const std::unique_ptr<QKeyEvent> keyEvent =
      QmlInputEvents::keyPressFrom(event);
  if (!keyEvent) {
    qWarning() << "ActionBridge::shortcutText: not a QML KeyEvent:" << event;
    return {};
  }
  return mDispatcher.shortcutText(*keyEvent);
}

QString ActionBridge::keyText(QObject *event) const {
  const std::unique_ptr<QKeyEvent> keyEvent =
      QmlInputEvents::keyPressFrom(event);
  if (!keyEvent) {
    qWarning() << "ActionBridge::keyText: not a QML KeyEvent:" << event;
    return {};
  }
  return mDispatcher.keyText(*keyEvent);
}

bool ActionBridge::handleWheelEvent(QObject *event) {
  const std::unique_ptr<QWheelEvent> wheelEvent =
      QmlInputEvents::wheelFrom(event);
  if (!wheelEvent) {
    qWarning() << "ActionBridge::handleWheelEvent: not a QML WheelEvent:"
               << event;
    return false;
  }
  return mDispatcher.processEvent(*wheelEvent);
}

bool ActionBridge::handleMousePress(QObject *event) {
  return handleMouseEvent(event, QEvent::MouseButtonPress);
}

bool ActionBridge::handleMouseRelease(QObject *event) {
  return handleMouseEvent(event, QEvent::MouseButtonRelease);
}

bool ActionBridge::handleMouseDoubleClick(QObject *event) {
  return handleMouseEvent(event, QEvent::MouseButtonDblClick);
}

bool ActionBridge::handleMouseEvent(QObject *event, QEvent::Type type) {
  const std::unique_ptr<QMouseEvent> mouseEvent =
      QmlInputEvents::mouseButtonFrom(event, type);
  if (!mouseEvent) {
    qWarning() << "ActionBridge: not a QML MouseEvent:" << event;
    return false;
  }
  return mDispatcher.processEvent(*mouseEvent);
}

//------------------------------------------------------------------------------
void ActionBridge::refresh() {
  std::vector<ActionEntry> entries = readEntries();
  if (entries == mActions.range())
    return;
  mActions.assign(std::move(entries));
  emit shortcutsChanged();
}

//------------------------------------------------------------------------------
std::vector<ActionEntry> ActionBridge::readEntries() const {
  const QStringList names = mDispatcher.actionNames();
  std::vector<ActionEntry> entries;
  entries.reserve(static_cast<size_t>(names.size()));
  for (const QString &name : names)
    entries.push_back({.name = name, .shortcut = mDispatcher.shortcutFor(name)});
  return entries;
}
