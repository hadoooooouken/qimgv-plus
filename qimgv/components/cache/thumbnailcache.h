#pragma once

#include <QObject>
#include <memory>
#include <QDir>
#include <QDebug>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QThreadStorage>
#include <atomic>
#include "sourcecontainers/thumbnail.h"

class ThumbnailCache : public QObject
{
    Q_OBJECT
public:
    explicit ThumbnailCache();

    // sourcePath is the original image path the thumbnail was generated from;
    // it is stored alongside the (hashed) cache key so stale entries whose
    // source file no longer exists can be identified later.
    void saveThumbnail(const QImage *image, QString id, const QString &sourcePath = QString());
    std::unique_ptr<QImage> readThumbnail(QString id);
    QString thumbnailPath(QString id);
    bool exists(QString id);
    void clear();

    // Deletes least-recently-used entries until the database's total BLOB
    // size is at or under maxBytes. No-op if maxBytes <= 0 or already under
    // quota.
    void enforceQuota(qint64 maxBytes);

    // Checks up to maxChecks entries (oldest-accessed first) for a source
    // path that no longer exists on disk, and deletes those rows. Returns
    // the number of entries removed.
    int cleanupStalePaths(int maxChecks = 200);

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

    // Runs enforceQuota() + cleanupStalePaths() every kMaintenanceInterval
    // saves, instead of on every single save, so normal browsing doesn't pay
    // for a SUM(LENGTH(data)) scan on each new thumbnail.
    void maybeRunMaintenance();
    static constexpr int kMaintenanceInterval = 100;

    QThreadStorage<ThreadLocalConnection *> threadConnections;
    QString cacheDirPath;
    std::atomic<int> mSaveCounter{0};
};
