#pragma once

#include <QObject>
#include <QString>
#include <QVariant>
#include <QtQml/qqmlregistration.h>

// Formats the shortcut of a Qt Quick Action for display in a menu item, as
// QMenu does: the platform's native text ("Ctrl+C"). The style's MenuItem
// draws the shortcut itself, in the secondary text colour of the widget
// context menu, instead of through MenuItemIconLabel (which draws it in the
// label colour).
//
// Stateless; GUI thread (created by the QML engine).
class ShortcutText : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON

public:
  explicit ShortcutText(QObject *parent = nullptr);

  // Native text of shortcut, which is what Action.shortcut holds: a key
  // sequence, a portable-text string ("Ctrl+C") or a
  // QKeySequence::StandardKey value. Returns an empty string for an empty
  // shortcut and logs a warning for a value that is none of these.
  Q_INVOKABLE QString of(const QVariant &shortcut) const;
};
