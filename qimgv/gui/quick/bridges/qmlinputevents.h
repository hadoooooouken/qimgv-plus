#pragma once

#include <QEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>

#include <memory>

class QObject;

// Rebuilds Qt input events from the QML KeyEvent / WheelEvent / MouseEvent
// objects that Keys.onPressed and MouseArea handlers receive, so that QML
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

// Returns nullptr and logs a warning when qmlEvent is null, does not expose
// the MouseEvent properties, or type is not a mouse button event type
// (MouseButtonPress, MouseButtonRelease, MouseButtonDblClick). The global
// position equals the local one, as for wheel events.
[[nodiscard]] std::unique_ptr<QMouseEvent> mouseButtonFrom(const QObject *qmlEvent,
                                                           QEvent::Type type);

} // namespace QmlInputEvents
