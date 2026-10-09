#include <QMap>
#include <QSignalSpy>
#include <QTest>

#include "gui/quick/bridges/actiondispatcher.h"
#include "gui/quick/ui/menus/contextmenumodel.h"
#include "testsuites.h"
#include "utils/actions.h"
#include "utils/fluenticon.h"

namespace {
using namespace Qt::StringLiterals;
using Kind = ContextMenuModel::Kind;
using Tone = ContextMenuModel::Tone;

// Runs nothing; records the invoked actions and answers shortcuts from a
// map, like ActionManager::shortcutForAction() (scripts included).
class FakeDispatcher final : public IActionDispatcher {
public:
  [[nodiscard]] QStringList actionNames() const override { return Actions().getList(); }
  [[nodiscard]] QString shortcutFor(const QString &action) const override {
    return shortcuts.value(action);
  }
  bool invoke(const QString &action) override {
    invoked << action;
    return Actions().getList().contains(action) || action.startsWith(u"s:"_s);
  }
  bool processEvent(QInputEvent &) override { return false; }
  [[nodiscard]] QString shortcutText(QInputEvent &) const override { return {}; }
  [[nodiscard]] QString keyText(const QKeyEvent &) const override { return {}; }

  QMap<QString, QString> shortcuts{
      {u"fitWindow"_s, u"1"_s},
      {u"zoomIn"_s, u"Ctrl++"_s},
      {u"crop"_s, u"X"_s},
      {u"copyFile"_s, u"C"_s},
      {u"openSettings"_s, u"P"_s},
      {u"removeFile"_s, u"Shift+Del"_s},
      {u"s:Edit"_s, u"Ctrl+E"_s},
  };
  QStringList invoked;
};

QStringList actionsOf(const std::vector<ContextMenuEntry> &entries) {
  QStringList actions;
  for (const ContextMenuEntry &entry : entries)
    actions << entry.action;
  return actions;
}

const ContextMenuEntry &entryFor(const std::vector<ContextMenuEntry> &entries,
                                 const QString &action) {
  for (const ContextMenuEntry &entry : entries) {
    if (entry.action == action)
      return entry;
  }
  qFatal("No context menu entry for %s", qPrintable(action));
}

const ContextMenuEntry &entryOfKind(const std::vector<ContextMenuEntry> &entries, Kind kind) {
  for (const ContextMenuEntry &entry : entries) {
    if (entry.kind == static_cast<int>(kind))
      return entry;
  }
  qFatal("No context menu entry of kind %d", static_cast<int>(kind));
}

// Every row of the menu that runs an action.
std::vector<ContextMenuEntry> actionEntries(const ContextMenuModel &menu) {
  std::vector<ContextMenuEntry> all;
  for (const auto *part : {&menu.zoomEntries(), &menu.transformEntries(), &menu.itemEntries(),
                           &menu.scriptEntries()}) {
    for (const ContextMenuEntry &entry : *part) {
      if (!entry.action.isEmpty())
        all.push_back(entry);
    }
  }
  return all;
}
} // namespace

class ContextMenuTests : public QObject {
  Q_OBJECT

private slots:
  void rowsFollowTheWidgetMenu() {
    FakeDispatcher dispatcher;
    ContextMenuModel menu(dispatcher);
    QCOMPARE(actionsOf(menu.zoomEntries()),
             QStringList({u"fitWindow"_s, u"fitWidth"_s, u"fitHeight"_s, u"fitNormal"_s,
                          u"zoomIn"_s, u"zoomOut"_s}));
    QCOMPARE(actionsOf(menu.transformEntries()),
             QStringList({u"rotateLeft"_s, u"rotateRight"_s, u"flipV"_s, u"flipH"_s,
                          u"crop"_s, u"resize"_s}));
    QCOMPARE(actionsOf(menu.itemEntries()),
             QStringList({u"colorAdjustments"_s, u"togglePanorama"_s, u"toggleUpscayl"_s,
                          u"casSettings"_s, QString(), u"copyFile"_s, u"moveFile"_s,
                          u"folderView"_s, u"showInDirectory"_s, u"toggleImageInfo"_s,
                          u"openSettings"_s, QString(), QString(), QString(),
                          u"renameFile"_s, u"setWallpaper"_s, u"print"_s, QString(),
                          u"moveToTrash"_s, u"removeFile"_s}));
    const std::vector<ContextMenuEntry> &items = menu.itemEntries();
    QCOMPARE(items.at(4).kind, static_cast<int>(Kind::Separator));
    QCOMPARE(items.at(12).kind, static_cast<int>(Kind::Expander));
    QCOMPARE(items.at(13).kind, static_cast<int>(Kind::Submenu));
    QCOMPARE(entryFor(items, u"moveToTrash"_s).tone, static_cast<int>(Tone::Trash));
    QCOMPARE(entryFor(items, u"removeFile"_s).tone, static_cast<int>(Tone::Danger));
    QCOMPARE(entryFor(items, u"copyFile"_s).tone, static_cast<int>(Tone::Normal));
    QCOMPARE(entryFor(items, u"copyFile"_s).text, u"Quick copy"_s);
  }

  void everyMenuActionIsAnApplicationAction() {
    FakeDispatcher dispatcher;
    ContextMenuModel menu(dispatcher);
    const QStringList known = Actions().getList();
    for (const ContextMenuEntry &entry : actionEntries(menu))
      QVERIFY2(known.contains(entry.action), qPrintable(entry.action));
  }

  void shortcutsAreTheOnesOfTheActionSystem() {
    FakeDispatcher dispatcher;
    ContextMenuModel menu(dispatcher);
    menu.setScripts({u"Edit"_s});
    for (const ContextMenuEntry &entry : actionEntries(menu))
      QCOMPARE(entry.shortcut, dispatcher.shortcutFor(entry.action));
    QCOMPARE(entryFor(menu.scriptEntries(), u"s:Edit"_s).shortcut, u"Ctrl+E"_s);

    // Edited shortcuts are read again, row by row.
    dispatcher.shortcuts.insert(u"copyFile"_s, u"Ctrl+Shift+C"_s);
    dispatcher.shortcuts.remove(u"crop"_s);
    QSignalSpy reset(menu.items(), &QAbstractItemModel::modelReset);
    QSignalSpy changed(menu.items(), &QAbstractItemModel::dataChanged);
    menu.refreshShortcuts();
    QCOMPARE(reset.count(), 0);
    QCOMPARE(changed.count(), 1);
    for (const ContextMenuEntry &entry : actionEntries(menu))
      QCOMPARE(entry.shortcut, dispatcher.shortcutFor(entry.action));
    QVERIFY(entryFor(menu.transformEntries(), u"crop"_s).shortcut.isEmpty());

    // The roles carry the same text.
    QAbstractItemModel *items = menu.items();
    const QHash<int, QByteArray> roles = items->roleNames();
    const int shortcutRole = roles.key("entryShortcut");
    const int actionRole = roles.key("entryAction");
    for (int row = 0; row < items->rowCount(); ++row) {
      const QModelIndex index = items->index(row, 0);
      QCOMPARE(index.data(shortcutRole).toString(),
               dispatcher.shortcutFor(index.data(actionRole).toString()));
    }
  }

  void imageRowsNeedADisplayedImage() {
    FakeDispatcher dispatcher;
    ContextMenuModel menu(dispatcher);
    for (const ContextMenuEntry &entry : menu.transformEntries())
      QVERIFY(!entry.enabled);
    for (const ContextMenuEntry &entry : menu.zoomEntries())
      QVERIFY(entry.enabled);
    QVERIFY(!entryFor(menu.itemEntries(), u"copyFile"_s).enabled);
    QVERIFY(entryFor(menu.itemEntries(), u"folderView"_s).enabled);
    QVERIFY(entryFor(menu.itemEntries(), u"print"_s).enabled);
    QVERIFY(!entryOfKind(menu.itemEntries(), Kind::Submenu).enabled);

    menu.setImageDisplayed(true);
    for (const ContextMenuEntry &entry : menu.transformEntries())
      QVERIFY(entry.enabled);
    QVERIFY(entryFor(menu.itemEntries(), u"copyFile"_s).enabled);
    QVERIFY(entryOfKind(menu.itemEntries(), Kind::Submenu).enabled);
  }

  void casSettingsNeedAnImageWithTheCasFilter() {
    FakeDispatcher dispatcher;
    ContextMenuModel menu(dispatcher);
    menu.setCasFilterActive(true);
    QVERIFY(!entryFor(menu.itemEntries(), u"casSettings"_s).shown);
    menu.setImageDisplayed(true);
    QVERIFY(entryFor(menu.itemEntries(), u"casSettings"_s).shown);
    menu.setCasFilterActive(false);
    QVERIFY(!entryFor(menu.itemEntries(), u"casSettings"_s).shown);
  }

  void moreExpandsInPlaceAndCollapsesOnOpening() {
    FakeDispatcher dispatcher;
    ContextMenuModel menu(dispatcher);
    QVERIFY(!entryFor(menu.itemEntries(), u"renameFile"_s).shown);
    QVERIFY(!entryOfKind(menu.itemEntries(), Kind::Submenu).shown);
    QCOMPARE(entryOfKind(menu.itemEntries(), Kind::Expander).icon,
             static_cast<int>(FluentIcon::ChevronDown20));

    QSignalSpy reset(menu.items(), &QAbstractItemModel::modelReset);
    menu.toggleMore();
    QVERIFY(menu.isMoreExpanded());
    QCOMPARE(reset.count(), 0);
    QVERIFY(entryFor(menu.itemEntries(), u"renameFile"_s).shown);
    QVERIFY(entryFor(menu.itemEntries(), u"removeFile"_s).shown);
    QVERIFY(entryOfKind(menu.itemEntries(), Kind::Submenu).shown);
    QCOMPARE(entryOfKind(menu.itemEntries(), Kind::Expander).icon,
             static_cast<int>(FluentIcon::ChevronUp20));

    menu.toggle();
    QVERIFY(menu.isOpen());
    QVERIFY(!menu.isMoreExpanded());
    QVERIFY(!entryFor(menu.itemEntries(), u"renameFile"_s).shown);
  }

  void menuActionOpensAndClosesTheMenu() {
    FakeDispatcher dispatcher;
    ContextMenuModel menu(dispatcher);
    QSignalSpy open(&menu, &ContextMenuModel::openChanged);
    menu.toggle();
    QVERIFY(menu.isOpen());
    menu.toggle();
    QVERIFY(!menu.isOpen());
    QCOMPARE(open.count(), 2);
    // Closing a closed menu says nothing.
    menu.close();
    QCOMPARE(open.count(), 2);
  }

  void folderViewAndLockedViewerKeepTheMenuClosed() {
    FakeDispatcher dispatcher;
    ContextMenuModel menu(dispatcher);
    menu.toggle();
    menu.setFolderViewActive(true);
    QVERIFY(!menu.isOpen());
    menu.toggle();
    QVERIFY(!menu.isOpen());

    menu.setFolderViewActive(false);
    menu.toggle();
    QVERIFY(menu.isOpen());
    menu.setInteractionEnabled(false);
    QVERIFY(!menu.isOpen());
    menu.toggle();
    QVERIFY(!menu.isOpen());
    menu.setInteractionEnabled(true);
    menu.toggle();
    QVERIFY(menu.isOpen());
  }

  void triggeringARowClosesTheMenuAndRunsTheAction() {
    FakeDispatcher dispatcher;
    ContextMenuModel menu(dispatcher);
    menu.toggle();
    menu.trigger(u"copyFile"_s);
    QVERIFY(!menu.isOpen());
    QCOMPARE(dispatcher.invoked, QStringList{u"copyFile"_s});

    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("unknown action"));
    menu.trigger(u"noSuchAction"_s);
  }

  void scriptsAreListedWithConfigureMenu() {
    FakeDispatcher dispatcher;
    ContextMenuModel menu(dispatcher);
    QSignalSpy reset(menu.scripts(), &QAbstractItemModel::modelReset);
    menu.setScripts({u"Edit"_s, u"Upload"_s});
    QCOMPARE(reset.count(), 1);
    QCOMPARE(actionsOf(menu.scriptEntries()), QStringList({u"s:Edit"_s, u"s:Upload"_s}));
    QCOMPARE(menu.scriptEntries().at(1).text, u"Upload"_s);
    // The same scripts again change nothing.
    menu.setScripts({u"Edit"_s, u"Upload"_s});
    QCOMPARE(reset.count(), 1);

    menu.toggle();
    menu.trigger(u"s:Edit"_s);
    QCOMPARE(dispatcher.invoked, QStringList{u"s:Edit"_s});

    QSignalSpy settings(&menu, &ContextMenuModel::scriptSettingsRequested);
    menu.toggle();
    menu.configureScripts();
    QCOMPARE(settings.count(), 1);
    QVERIFY(!menu.isOpen());
  }
};

int runContextMenuTests(int argc, char **argv) {
  ContextMenuTests tests;
  return QTest::qExec(&tests, argc, argv);
}

#include "tst_contextmenu.moc"
