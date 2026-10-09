#include "shortcuttext.h"

#include <QDebug>
#include <QKeySequence>
#include <QMetaType>

//------------------------------------------------------------------------------
ShortcutText::ShortcutText(QObject *parent) : QObject(parent) {}

//------------------------------------------------------------------------------
QString ShortcutText::of(const QVariant &shortcut) const {
  if (!shortcut.isValid() || shortcut.isNull())
    return {};

  QKeySequence sequence;
  switch (shortcut.metaType().id()) {
  case QMetaType::QKeySequence:
    sequence = shortcut.value<QKeySequence>();
    break;
  case QMetaType::QString:
    sequence =
        QKeySequence::fromString(shortcut.toString(), QKeySequence::PortableText);
    break;
  case QMetaType::Int:
  case QMetaType::Double:
    sequence = QKeySequence(
        static_cast<QKeySequence::StandardKey>(shortcut.toInt()));
    break;
  default:
    qWarning() << "ShortcutText: unsupported shortcut value" << shortcut;
    return {};
  }
  return sequence.toString(QKeySequence::NativeText);
}
