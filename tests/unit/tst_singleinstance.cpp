#include <QSignalSpy>
#include <QTest>
#include <QThread>
#include <QUuid>

#include <atomic>
#include <memory>

#include "components/singleinstance/singleinstancechannel.h"
#include "testsuites.h"

namespace {
using namespace Qt::StringLiterals;

// Short, so the "no primary instance" case does not slow the suite down.
constexpr SingleInstanceTimeouts kTestTimeouts{.connectMs = 100, .writeMs = 1000};

// A server name no other test run or application instance uses.
QString uniqueServerName() {
  return SingleInstanceChannel::serverNameFor(
      QUuid::createUuid().toString(QUuid::WithoutBraces));
}

// A secondary instance forwarding paths, one after the other, from its own
// thread: on Windows the write completes only once the primary reads it, and
// the primary reads in this thread's event loop (in the application the
// secondary is another process).
class SecondaryInstance {
public:
  SecondaryInstance(const QString &serverName, const QStringList &paths)
      : thread(QThread::create([this, serverName, paths]() {
          SingleInstanceChannel secondary(serverName, kTestTimeouts);
          for (const QString &path : paths) {
            if (secondary.forwardToPrimary(path))
              ++delivered;
          }
        })) {
    thread->start();
  }

  ~SecondaryInstance() { thread->wait(); }

  // Number of paths the primary accepted; valid after finish().
  int finish() {
    thread->wait();
    return delivered;
  }

private:
  std::atomic<int> delivered = 0;
  std::unique_ptr<QThread> thread;
};
} // namespace

class SingleInstanceTests : public QObject {
  Q_OBJECT

private slots:
  void serverNameDependsOnTheTempPathOnly() {
    const QString first = SingleInstanceChannel::serverNameFor(u"C:/Temp"_s);
    QCOMPARE(SingleInstanceChannel::serverNameFor(u"C:/Temp"_s), first);
    QVERIFY(SingleInstanceChannel::serverNameFor(u"D:/Temp"_s) != first);
    QVERIFY(first.startsWith(u"qimgv-plus-single-instance-"_s));
  }

  void forwardingWithoutAPrimaryFails() {
    SingleInstanceChannel secondary(uniqueServerName(), kTestTimeouts);
    QVERIFY(!secondary.forwardToPrimary(u"C:/images/a.png"_s));
  }

  void thePrimaryReceivesTheForwardedPath() {
    const QString serverName = uniqueServerName();
    SingleInstanceChannel primary(serverName, kTestTimeouts);
    QVERIFY(primary.listen());
    QSignalSpy receivedSpy(&primary, &SingleInstanceChannel::pathReceived);

    SecondaryInstance secondary(serverName, {u"C:/images/a.png"_s});
    QVERIFY(receivedSpy.wait());
    QCOMPARE(secondary.finish(), 1);
    QCOMPARE(receivedSpy.count(), 1);
    QCOMPARE(receivedSpy.first().first().toString(), u"C:/images/a.png"_s);
  }

  void anEmptyPathAsksToRaise() {
    const QString serverName = uniqueServerName();
    SingleInstanceChannel primary(serverName, kTestTimeouts);
    QVERIFY(primary.listen());
    QSignalSpy receivedSpy(&primary, &SingleInstanceChannel::pathReceived);

    SecondaryInstance secondary(serverName, {QString()});
    QVERIFY(receivedSpy.wait());
    QCOMPARE(secondary.finish(), 1);
    QVERIFY(receivedSpy.first().first().toString().isEmpty());
  }

  void everyForwardIsDelivered() {
    const QString serverName = uniqueServerName();
    SingleInstanceChannel primary(serverName, kTestTimeouts);
    QVERIFY(primary.listen());
    QSignalSpy receivedSpy(&primary, &SingleInstanceChannel::pathReceived);

    SecondaryInstance secondary(serverName, {u"C:/a.png"_s, u"C:/b.png"_s});
    QTRY_COMPARE(receivedSpy.count(), 2);
    QCOMPARE(secondary.finish(), 2);
    QStringList received{receivedSpy.at(0).first().toString(),
                         receivedSpy.at(1).first().toString()};
    received.sort();
    QCOMPARE(received, (QStringList{u"C:/a.png"_s, u"C:/b.png"_s}));
  }
};

int runSingleInstanceTests(int argc, char **argv) {
  SingleInstanceTests tests;
  return QTest::qExec(&tests, argc, argv);
}

#include "tst_singleinstance.moc"
