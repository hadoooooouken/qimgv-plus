#include <QSignalSpy>
#include <QTest>
#include <QUrl>

#include "gui/quick/ui/mainwindowshell.h"
#include "testsuites.h"

namespace {
using namespace Qt::StringLiterals;
} // namespace

// MainWindowShell (the Quick main window's state and drops).
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
};

int runQuickShellTests(int argc, char **argv) {
  QuickShellTests tests;
  return QTest::qExec(&tests, argc, argv);
}

#include "tst_quickshell.moc"
