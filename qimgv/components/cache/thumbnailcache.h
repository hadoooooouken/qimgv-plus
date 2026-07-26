#pragma once

#include "thumbnailcachemaintenance.h"

#include <QImage>
#include <QObject>
#include <QSqlDatabase>
#include <QThreadStorage>
#include <atomic>
#include <memory>
#include <mutex>
#include <utility>

class ThumbnailCache : public QObject
{
    Q_OBJECT
public:
    explicit ThumbnailCache();
    ~ThumbnailCache() override;

    [[nodiscard]] bool saveThumbnail(const QImage *image, QString id,
                                     const QString &sourcePath = QString());
    [[nodiscard]] bool performStartupMaintenance();
    std::unique_ptr<QImage> readThumbnail(QString id);
    QString thumbnailPath(QString id);
    bool exists(QString id);
    [[nodiscard]] bool clear();

private:
    class ThreadLocalConnection {
    public:
        ThreadLocalConnection(QSqlDatabase database, QString name,
                              bool schemaInitialized)
            : db(std::move(database)),
              connectionName(std::move(name)),
              schemaReady(schemaInitialized)
        {
        }

        ~ThreadLocalConnection()
        {
            if (db.isOpen())
                db.close();
            db = QSqlDatabase();
            QSqlDatabase::removeDatabase(connectionName);
        }

        QSqlDatabase db;
        QString connectionName;
        bool schemaReady = false;
    };

    static constexpr qint64 kBytesPerMegabyte = 1024 * 1024;
    static constexpr qint64 kMaintenanceGrowthBytes = 8 * kBytesPerMegabyte;
    static constexpr qint64 kAccessTouchIntervalSeconds = 60 * 60;
    static constexpr int kDatabaseBusyTimeoutMilliseconds = 2000;
    static constexpr int kWalAutoCheckpointPages = 256;
    static constexpr int kThumbnailEncodingEffort = 2;
    static constexpr int kThumbnailEncodingQuality = 85;
    static constexpr char kThumbnailEncodingFormat[] = "JXL";

    [[nodiscard]] QSqlDatabase getDatabaseConnection();
    [[nodiscard]] bool initializeDatabase(QSqlDatabase &db);
    [[nodiscard]] bool ensureStartupMaintenance(QSqlDatabase &db);
    [[nodiscard]] bool executeSchemaStatement(QSqlDatabase &db,
                                              const QString &statement,
                                              const QString &operation);
    [[nodiscard]] ThumbnailCacheMaintenance::Quota currentQuota() const;
    [[nodiscard]] bool removeEntry(QSqlDatabase &db, const QString &id,
                                   const QString &reason);
    [[nodiscard]] bool runMaintenanceLocked(
        QSqlDatabase &db,
        const ThumbnailCacheMaintenance::Request &request);
    [[nodiscard]] bool maintenanceGrowthLimitReached(qint64 encodedBytes);

    static std::mutex sDatabaseAccessMutex;

    QThreadStorage<ThreadLocalConnection *> threadConnections;
    QString databasePath;
    ThumbnailCacheMaintenance maintenance;
    std::mutex startupMaintenanceMutex;
    std::atomic<bool> startupMaintenanceComplete{false};
    qint64 bytesSinceMaintenance = 0;
};
