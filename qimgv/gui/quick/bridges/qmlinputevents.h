#pragma once

#include <QKeyEvent>
#include <QWheelEvent>

#include <memory>

class QObject;

// Rebuilds Qt input events from the QML KeyEvent / WheelEvent objects that
// Keys.onPressed and MouseArea.onWheel handlers receive, so that QML
// can forward input into the toolkit-agnostic ActionManager::processEvent().
// The QML event types are private Qt classes; they are read through their
// public meta-object properties only.
namespace QmlInputEvents {

// Returns nullptr and logs a warning when qmlEvent is null or does not expose
// the KeyEvent properties.
[[nodiscard]] std::unique_ptr<QKeyEvent> keyPressFrom(const QObject *qmlEvent);

// Returns nullptr and logs a warning when qmlEvent is null or does not expose
// the WheelEvent properties. The global position equals the local one: QML
// wheel events carry no screen position, and shortcuts do not use it.
[[nodiscard]] std::unique_ptr<QWheelEvent> wheelFrom(const QObject *qmlEvent);

} // namespace QmlInputEvents
