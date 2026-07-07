#pragma once

#include <QObject>
#include <memory>
#include <QDir>
#include <QDebug>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QThreadStorage>
#include "sourcecontainers/thumbnail.h"

class ThumbnailCache : public QObject
{
    Q_OBJECT
public:
    explicit ThumbnailCache();

    void saveThumbnail(const QImage *image, QString id);
    std::unique_ptr<QImage> readThumbnail(QString id);
    QString thumbnailPath(QString id);
    bool exists(QString id);
    void clear();

signals:

public slots:

private:
    // Owns exactly one SQLite connection for the thread that created it.
    // QThreadStorage guarantees this destructor runs on the owning thread
    // itself (on thread exit, or when setLocalData() replaces the entry),
    // never from a foreign thread, satisfying QSqlDatabase's thread-affinity
    // requirement.
    class ThreadLocalConnection {
    public:
        ThreadLocalConnection(QSqlDatabase database, QString name)
            : db(std::move(database)), connectionName(std::move(name)) {}

        ~ThreadLocalConnection() {
            if (db.isOpen())
                db.close();
            db = QSqlDatabase(); // drop the handle before removeDatabase()
            QSqlDatabase::removeDatabase(connectionName);
        }

        QSqlDatabase db;
        QString connectionName;
    };

    QSqlDatabase getDatabaseConnection();
    QThreadStorage<ThreadLocalConnection *> threadConnections;
    QString cacheDirPath;
};
