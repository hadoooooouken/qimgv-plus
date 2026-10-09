#include "singleinstancechannel.h"

#include <QByteArray>
#include <QCryptographicHash>
#include <QDataStream>
#include <QDebug>
#include <QLocalSocket>

#include <utility>

#include <windows.h>

namespace {
using namespace Qt::StringLiterals;

constexpr QLatin1StringView serverNamePrefix = "qimgv-plus-single-instance-"_L1;
} // namespace

SingleInstanceChannel::SingleInstanceChannel(QString serverName,
                                             SingleInstanceTimeouts timeouts,
                                             QObject *parent)
    : QObject(parent),
      serverName(std::move(serverName)),
      timeouts(timeouts) {
}

// Pending client sockets are children of the server and go with it.
SingleInstanceChannel::~SingleInstanceChannel() = default;

QString SingleInstanceChannel::serverNameFor(const QString &tempPath) {
    return serverNamePrefix +
           QString::fromLatin1(
               QCryptographicHash::hash(tempPath.toUtf8(), QCryptographicHash::Md5)
                   .toHex());
}

bool SingleInstanceChannel::forwardToPrimary(const QString &path) {
    QLocalSocket socket;
    socket.connectToServer(serverName);
    if (!socket.waitForConnected(timeouts.connectMs))
        return false;

    QByteArray data;
    QDataStream out(&data, QIODevice::WriteOnly);
    out << path;
    // Lets the primary instance bring its window to the foreground.
    AllowSetForegroundWindow(ASFW_ANY);
    socket.write(data);
    if (!socket.waitForBytesWritten(timeouts.writeMs)) {
        qWarning() << "SingleInstanceChannel: the path was not delivered to the"
                      " running instance:"
                   << socket.errorString();
    }
    socket.disconnectFromServer();
    return true;
}

bool SingleInstanceChannel::listen() {
    QLocalServer::removeServer(serverName);
    server = std::make_unique<QLocalServer>();
    connect(server.get(), &QLocalServer::newConnection, this,
            &SingleInstanceChannel::acceptConnection);
    if (!server->listen(serverName)) {
        qWarning() << "SingleInstanceChannel: cannot listen on" << serverName
                   << ":" << server->errorString();
        server.reset();
        return false;
    }
    return true;
}

void SingleInstanceChannel::acceptConnection() {
    while (QLocalSocket *clientSocket = server->nextPendingConnection()) {
        connect(clientSocket, &QLocalSocket::disconnected, clientSocket,
                &QLocalSocket::deleteLater);
        connect(clientSocket, &QLocalSocket::readyRead, this, [this, clientSocket]() {
            QDataStream in(clientSocket);
            in.startTransaction();
            QString pathReceived;
            in >> pathReceived;
            // Incomplete message: wait for the rest.
            if (!in.commitTransaction())
                return;
            emit this->pathReceived(pathReceived);
        });
    }
}
