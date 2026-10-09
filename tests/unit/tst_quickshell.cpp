#include <QSignalSpy>
#include <QTest>
#include <QUrl>

#include "gui/quick/adapters/placeholderdirectoryview.h"
#include "gui/quick/ui/mainwindowshell.h"
#include "testsuites.h"

namespace {
using namespace Qt::StringLiterals;
} // namespace

// MainWindowShell (the Quick main window's state and drops) and the
// placeholder directory views the Quick UI hands to Core.
class QuickShellTests : public QObject {
  Q_OBJECT

private slots:
  void dropsAreForwardedWithTheirSource() {
    MainWindowShell shell;
    QObject source;
    QSignalSpy dropSpy(&shell, &MainWindowShell::urlsDropped);
    const QList<QUrl> urls{QUrl::fromLocalFile(u"C:/images/a.png"_s),
                           QUrl::fromLocalFile(u"C:/images/b.png"_s)};

    shell.dropUrls(urls, &source);
    QCOMPARE(dropSpy.count(), 1);
    QCOMPARE(dropSpy.first().at(0).value<QList<QUrl>>(), urls);
    QCOMPARE(dropSpy.first().at(1).value<QObject *>(), &source);
  }

  void dropsWithoutUrlsAreIgnored() {
    MainWindowShell shell;
    QSignalSpy dropSpy(&shell, &MainWindowShell::urlsDropped);
    QTest::ignoreMessage(QtWarningMsg, "MainWindowShell: a drop without URLs was ignored");
    shell.dropUrls({}, nullptr);
    QCOMPARE(dropSpy.count(), 0);
  }

  void stateChangesAreNotifiedOnce() {
    MainWindowShell shell;
    QSignalSpy folderSpy(&shell, &MainWindowShell::folderViewActiveChanged);
    QSignalSpy fullscreenSpy(&shell, &MainWindowShell::fullscreenChanged);
    shell.setFolderViewActive(true);
    shell.setFolderViewActive(true);
    shell.setFullscreen(true);
    shell.setFullscreen(true);
    QVERIFY(shell.folderViewActive());
    QVERIFY(shell.fullscreen());
    QCOMPARE(folderSpy.count(), 1);
    QCOMPARE(fullscreenSpy.count(), 1);
  }

  void placeholderReportsEachPopulate() {
    PlaceholderDirectoryView view;
    QSignalSpy populatedSpy(&view, &PlaceholderDirectoryView::populated);
    view.populate(5);
    view.populate(3);
    QCOMPARE(populatedSpy.count(), 2);
    QCOMPARE(view.itemCount(), 3);
  }

  void placeholderKeepsAValidSelection() {
    PlaceholderDirectoryView view;
    view.populate(5);
    view.select(QList<int>{1, 1, 4, 7, -1});
    QCOMPARE(view.selection(), (QList<int>{1, 4}));
    view.select(2);
    QCOMPARE(view.selection(), QList<int>{2});
    view.populate(5);
    QVERIFY(view.selection().isEmpty());
  }

  void placeholderSelectionFollowsInsertsAndRemovals() {
    PlaceholderDirectoryView view;
    view.populate(5);
    view.select(QList<int>{1, 3});
    view.insertItem(2);
    QCOMPARE(view.selection(), (QList<int>{1, 4}));
    QCOMPARE(view.itemCount(), 6);
    view.removeItem(1);
    QCOMPARE(view.selection(), QList<int>{3});
    QCOMPARE(view.itemCount(), 5);
  }

  void placeholderIsADirectoryViewForThePresenter() {
    auto view = std::make_shared<PlaceholderDirectoryView>();
    std::shared_ptr<IDirectoryView> asView = view;
    // DirectoryPresenter connects to the view's signals through QObject.
    QVERIFY(dynamic_cast<QObject *>(asView.get()) == view.get());
  }
};

int runQuickShellTests(int argc, char **argv) {
  QuickShellTests tests;
  return QTest::qExec(&tests, argc, argv);
}

#include "tst_quickshell.moc"
