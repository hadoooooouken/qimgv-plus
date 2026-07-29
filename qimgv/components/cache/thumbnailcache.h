#pragma once

#include "decodedthumbnailcache.h"
#include "thumbnailcachemaintenance.h"
#include "thumbnailsourcestamp.h"

#include <QByteArray>
#include <QImage>
#include <QList>
#include <QObject>
#include <QSqlDatabase>
#include <QThreadStorage>
#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>

class ThumbnailCache : public QObject
{
    Q_OBJECT
public:
    struct WriteEntry {
        QString id;
        ThumbnailSourceStamp sourceStamp;
        int originalWidth = 0;
        int originalHeight = 0;
        QString label;
        QByteArray encodedData;
        bool requiresLinearColorSpace = false;
    };

    struct AccessTouch {
        QString id;
        qint64 accessedAt = 0;
        quint64 generation = 0;
    };

    struct ReadResult {
        std::unique_ptr<QImage> image;
        bool requiresLinearColorSpace = false;
        std::optional<AccessTouch> accessTouch;
    };

    explicit ThumbnailCache();
    ~ThumbnailCache() override;

    [[nodiscard]] bool saveThumbnails(const QList<WriteEntry> &entries);
    [[nodiscard]] bool
    applyAccessTouches(const QList<AccessTouch> &accessTouches);
    [[nodiscard]] bool performStartupMaintenance();
    [[nodiscard]] ReadResult
    readThumbnail(const QString &id, const ThumbnailSourceStamp &sourceStamp);
    void storeDecodedThumbnail(const QString &id,
                               const ThumbnailSourceStamp &sourceStamp,
                               const QImage &image,
                               bool requiresLinearColorSpace);
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
    static constexpr qint64 kDecodedCacheMaximumBytes =
        128 * kBytesPerMegabyte;
    static constexpr qint64 kAccessTouchIntervalSeconds = 60 * 60;
    static constexpr int kDatabaseBusyTimeoutMilliseconds = 2000;
    static constexpr int kWalAutoCheckpointPages = 256;
    [[nodiscard]] QSqlDatabase getDatabaseConnection();
    [[nodiscard]] bool configureDatabaseConnection(QSqlDatabase &db);
    [[nodiscard]] bool ensureDatabaseInitialized(QSqlDatabase &db);
    [[nodiscard]] bool initializeDatabaseSchema(QSqlDatabase &db);
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

    static std::mutex sDatabaseWriteMutex;

    QThreadStorage<ThreadLocalConnection *> threadConnections;
    QString databasePath;
    ThumbnailCacheMaintenance maintenance;
    DecodedThumbnailCache decodedCache;
    std::atomic<bool> databaseInitialized{false};
    std::mutex startupMaintenanceMutex;
    std::atomic<bool> startupMaintenanceComplete{false};
    qint64 bytesSinceMaintenance = 0;
};
