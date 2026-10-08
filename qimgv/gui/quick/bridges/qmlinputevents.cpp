#include "qmlinputevents.h"

#include <QDebug>
#include <QObject>
#include <QPointF>
#include <QVariant>

#include <optional>

namespace {

// Property names of QQuickKeyEvent (QML KeyEvent).
constexpr const char *kKeyProperty = "key";
constexpr const char *kTextProperty = "text";
constexpr const char *kModifiersProperty = "modifiers";
constexpr const char *kAutoRepeatProperty = "isAutoRepeat";
constexpr const char *kCountProperty = "count";
constexpr const char *kNativeScanCodeProperty = "nativeScanCode";

// Property names of QQuickWheelEvent (QML WheelEvent); modifiers is shared.
constexpr const char *kXProperty = "x";
constexpr const char *kYProperty = "y";
constexpr const char *kAngleDeltaProperty = "angleDelta";
constexpr const char *kPixelDeltaProperty = "pixelDelta";
constexpr const char *kButtonsProperty = "buttons";
constexpr const char *kInvertedProperty = "inverted";

// Reads property name of object; logs and returns std::nullopt when the
// object does not have it or its value does not convert to T.
template <typename T>
std::optional<T> readProperty(const QObject &object, const char *name) {
  const QVariant value = object.property(name);
  if (!value.isValid() || !value.canConvert<T>()) {
    qWarning() << "QmlInputEvents:" << object.metaObject()->className()
               << "has no usable property" << name;
    return std::nullopt;
  }
  return value.value<T>();
}

} // namespace

//------------------------------------------------------------------------------
std::unique_ptr<QKeyEvent>
QmlInputEvents::keyPressFrom(const QObject *qmlEvent) {
  if (!qmlEvent) {
    qWarning() << "QmlInputEvents::keyPressFrom: null event";
    return nullptr;
  }
  const auto key = readProperty<int>(*qmlEvent, kKeyProperty);
  const auto text = readProperty<QString>(*qmlEvent, kTextProperty);
  const auto modifiers = readProperty<int>(*qmlEvent, kModifiersProperty);
  const auto autoRepeat = readProperty<bool>(*qmlEvent, kAutoRepeatProperty);
  const auto count = readProperty<int>(*qmlEvent, kCountProperty);
  const auto scanCode =
      readProperty<quint32>(*qmlEvent, kNativeScanCodeProperty);
  if (!key || !text || !modifiers || !autoRepeat || !count || !scanCode)
    return nullptr;

  constexpr quint32 kNoNativeVirtualKey = 0;
  constexpr quint32 kNoNativeModifiers = 0;
  return std::make_unique<QKeyEvent>(
      QEvent::KeyPress, *key, Qt::KeyboardModifiers::fromInt(*modifiers),
      *scanCode, kNoNativeVirtualKey, kNoNativeModifiers, *text, *autoRepeat,
      static_cast<quint16>(*count));
}

//------------------------------------------------------------------------------
std::unique_ptr<QWheelEvent>
QmlInputEvents::wheelFrom(const QObject *qmlEvent) {
  if (!qmlEvent) {
    qWarning() << "QmlInputEvents::wheelFrom: null event";
    return nullptr;
  }
  const auto x = readProperty<qreal>(*qmlEvent, kXProperty);
  const auto y = readProperty<qreal>(*qmlEvent, kYProperty);
  const auto angleDelta = readProperty<QPoint>(*qmlEvent, kAngleDeltaProperty);
  const auto pixelDelta = readProperty<QPoint>(*qmlEvent, kPixelDeltaProperty);
  const auto buttons = readProperty<int>(*qmlEvent, kButtonsProperty);
  const auto modifiers = readProperty<int>(*qmlEvent, kModifiersProperty);
  const auto inverted = readProperty<bool>(*qmlEvent, kInvertedProperty);
  if (!x || !y || !angleDelta || !pixelDelta || !buttons || !modifiers ||
      !inverted)
    return nullptr;

  const QPointF position(*x, *y);
  return std::make_unique<QWheelEvent>(
      position, position, *pixelDelta, *angleDelta,
      Qt::MouseButtons::fromInt(*buttons),
      Qt::KeyboardModifiers::fromInt(*modifiers), Qt::NoScrollPhase,
      *inverted);
}
