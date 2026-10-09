#pragma once

#include <QAbstractItemModel>
#include <QEvent>
#include <QObject>
#include <QRangeModel>
#include <QRangeModelAdapter>
#include <QString>
#include <QtQml/qqmlregistration.h>

#include <vector>

class IActionDispatcher;

// One row of ActionBridge::actions: the action name and its primary shortcut.
struct ActionEntry {
  Q_GADGET
  Q_PROPERTY(QString name MEMBER name)
  Q_PROPERTY(QString shortcut MEMBER shortcut)

public:
  QString name;
  QString shortcut;

  friend bool operator==(const ActionEntry &, const ActionEntry &) = default;
};

// Each gadget property is a role (name, shortcut) of a single-column list.
template <> struct QRangeModel::RowOptions<ActionEntry> {
  static constexpr auto rowCategory = QRangeModel::RowCategory::MultiRoleItem;
};

// Actions and shortcuts for QML (Actions singleton).
//
// QML runs actions by name and forwards unhandled input:
//   Keys.onPressed: (event) => event.accepted = Actions.handleKeyEvent(event)
//   MouseArea { onWheel: (wheel) => wheel.accepted = Actions.handleWheelEvent(wheel) }
//   MouseArea { onPressed: (mouse) => Actions.handleMousePress(mouse) }
// Each rebuilds the matching Qt event and passes it to the dispatcher, which
// is ActionManager::processEvent() in the application. Mouse buttons form
// shortcuts like the widget UI: presses of every button but the right one,
// the release of the right button, and left-button double clicks.
//
// Shortcuts can be edited at runtime (settings dialog); the composition root
// calls refresh() on Settings::settingsChanged, which updates the model and
// emits shortcutsChanged() when anything changed.
//
// GUI thread only.
class ActionBridge : public QObject {
  Q_OBJECT
  QML_NAMED_ELEMENT(Actions)
  QML_SINGLETON
  QML_UNCREATABLE("Provided by QuickUiHost via setExternalSingletonInstance()")
  // Roles: name, shortcut. Read-only for QML by convention: edits made
  // through the model do not reach the dispatcher.
  Q_PROPERTY(QAbstractItemModel *actions READ actions CONSTANT FINAL)

public:
  // dispatcher must outlive the bridge.
  explicit ActionBridge(IActionDispatcher &dispatcher,
                        QObject *parent = nullptr);
  ~ActionBridge() override;

  [[nodiscard]] QAbstractItemModel *actions() const;

  // Runs the action called name; returns false when it is unknown.
  Q_INVOKABLE bool invoke(const QString &name);
  // Primary shortcut of the action called name, or an empty string.
  Q_INVOKABLE QString shortcutFor(const QString &name) const;
  // Forwards a QML KeyEvent; returns true when an action handled it.
  Q_INVOKABLE bool handleKeyEvent(QObject *event);
  // Forwards a QML WheelEvent; returns true when an action handled it.
  Q_INVOKABLE bool handleWheelEvent(QObject *event);
  // Forward a QML MouseEvent of a press, release or double click; return
  // true when an action handled it.
  Q_INVOKABLE bool handleMousePress(QObject *event);
  Q_INVOKABLE bool handleMouseRelease(QObject *event);
  Q_INVOKABLE bool handleMouseDoubleClick(QObject *event);
  // Shortcut text of a QML KeyEvent ("Ctrl+R"), as shortcuts are stored;
  // empty for events without a shortcut.
  Q_INVOKABLE QString shortcutText(QObject *event) const;
  // Layout-independent name of the key of a QML KeyEvent, without
  // modifiers ("3" for the key in the digit row).
  Q_INVOKABLE QString keyText(QObject *event) const;

  // Re-reads actions and shortcuts from the dispatcher.
  void refresh();

signals:
  void shortcutsChanged();

private:
  [[nodiscard]] std::vector<ActionEntry> readEntries() const;
  [[nodiscard]] bool handleMouseEvent(QObject *event, QEvent::Type type);

  IActionDispatcher &mDispatcher;
  QRangeModelAdapter<std::vector<ActionEntry>> mActions;
};
