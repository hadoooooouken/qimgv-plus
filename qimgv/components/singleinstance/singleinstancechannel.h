#pragma once

#include <QLocalServer>
#include <QObject>
#include <QString>

#include <memory>

// Timeouts of the hand-off from a secondary to the primary instance.
struct SingleInstanceTimeouts {
    int connectMs = 500;
    int writeMs = 1000;
};

// Local-socket channel between the instances of the application. A secondary
// instance hands its path to the primary with forwardToPrimary() and exits;
// the primary listen()s and receives the paths through pathReceived(). Both
// user interfaces use it, so a second launch always raises the running one.
// GUI thread only.
class SingleInstanceChannel final : public QObject {
    Q_OBJECT
public:
    // serverName identifies the channel; see serverNameFor().
    explicit SingleInstanceChannel(QString serverName,
                                   SingleInstanceTimeouts timeouts = {},
                                   QObject *parent = nullptr);
    ~SingleInstanceChannel() override;

    // Server name shared by the instances that use the same temporary
    // directory (one per user session).
    [[nodiscard]] static QString serverNameFor(const QString &tempPath);

    // Sends path (may be empty: "just raise") to a running primary instance.
    // Returns false when no primary instance answered.
    bool forwardToPrimary(const QString &path);

    // Makes this process the primary instance. Removes a stale server left
    // by a crashed instance first. Returns false (and logs the reason) when
    // the server could not listen.
    bool listen();

signals:
    // A secondary instance asked to open path (empty: raise the window).
    void pathReceived(const QString &path);

private:
    void acceptConnection();

    QString serverName;
    SingleInstanceTimeouts timeouts;
    std::unique_ptr<QLocalServer> server;
};
