#include "thumbnailcache.h"

#include "settings.h"

#include <QBuffer>
#include <QDateTime>
#include <QDebug>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QThread>
#include <algorithm>
#include <limits>

std::mutex ThumbnailCache::sDatabaseAccessMutex;

ThumbnailCache::ThumbnailCache()
    : databasePath(settings->thumbnailCacheDir() +
                   QStringLiteral("thumbnails.db")),
      maintenance(databasePath)
{
}

ThumbnailCache::~ThumbnailCache()
{
    if (threadConnections.hasLocalData())
        threadConnections.setLocalData(nullptr);
}

QSqlDatabase ThumbnailCache::getDatabaseConnection()
{
    if (threadConnections.hasLocalData()) {
        ThreadLocalConnection *connection = threadConnections.localData();
        if (!connection) {
            qWarning() << "Thumbnail cache thread-local connection is null";
            return {};
        }

        if (!connection->db.isOpen() || !connection->schemaReady) {
            std::lock_guard lock(sDatabaseAccessMutex);
            if (!connection->db.isOpen() && !connection->db.open()) {
                qWarning() << "Failed to reopen thumbnail cache database:"
                           << connection->db.lastError().text();
                return connection->db;
            }
            connection->schemaReady = initializeDatabase(connection->db);
            if (!connection->schemaReady)
                connection->db.close();
        }
        return connection->db;
    }

    const QString connectionName =
        QStringLiteral("thumbnail_db_%1_%2")
            .arg(reinterpret_cast<quintptr>(this))
            .arg(reinterpret_cast<quintptr>(QThread::currentThreadId()));

    std::lock_guard lock(sDatabaseAccessMutex);
    QSqlDatabase db =
        QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    db.setDatabaseName(databasePath);

    bool schemaReady = false;
    if (!db.open()) {
        qWarning() << "Failed to open thumbnail cache database:"
                   << db.lastError().text();
    } else {
        schemaReady = initializeDatabase(db);
        if (!schemaReady)
            db.close();
    }

    threadConnections.setLocalData(
        new ThreadLocalConnection(db, connectionName, schemaReady));
    return db;
}

bool ThumbnailCache::initializeDatabase(QSqlDatabase &db)
{
    if (!executeSchemaStatement(
            db,
            QStringLiteral("PRAGMA busy_timeout=%1;")
                .arg(kDatabaseBusyTimeoutMilliseconds),
            QStringLiteral("Failed to configure thumbnail cache busy timeout")) ||
        !executeSchemaStatement(
            db, QStringLiteral("PRAGMA journal_mode=WAL;"),
            QStringLiteral("Failed to enable thumbnail cache WAL mode")) ||
        !executeSchemaStatement(
            db, QStringLiteral("PRAGMA synchronous=NORMAL;"),
            QStringLiteral("Failed to configure thumbnail cache synchronization")) ||
        !executeSchemaStatement(
            db,
            QStringLiteral("PRAGMA wal_autocheckpoint=%1;")
                .arg(kWalAutoCheckpointPages),
            QStringLiteral("Failed to configure thumbnail cache WAL checkpoint"))) {
        return false;
    }

    if (!db.transaction()) {
        qWarning() << "Failed to start thumbnail cache schema transaction:"
                   << db.lastError().text();
        return false;
    }

    const auto failSchemaTransaction = [&db]() {
        if (!db.rollback()) {
            qWarning() << "Failed to roll back thumbnail cache schema transaction:"
                       << db.lastError().text();
        }
        return false;
    };

    if (!executeSchemaStatement(
            db,
            QStringLiteral(
                "CREATE TABLE IF NOT EXISTS thumbnails ("
                "id TEXT PRIMARY KEY, "
                "last_modified TEXT, "
                "original_width INTEGER, "
                "original_height INTEGER, "
                "label TEXT, "
                "data BLOB, "
                "source_path TEXT, "
                "last_accessed INTEGER NOT NULL DEFAULT 0"
                ");"),
            QStringLiteral("Failed to create thumbnail cache table"))) {
        return failSchemaTransaction();
    }

    QSet<QString> existingColumns;
    QSqlQuery tableInfoQuery(db);
    if (!tableInfoQuery.exec(QStringLiteral("PRAGMA table_info(thumbnails);"))) {
        qWarning() << "Failed to inspect thumbnail cache schema:"
                   << tableInfoQuery.lastError().text();
        return failSchemaTransaction();
    }
    while (tableInfoQuery.next())
        existingColumns.insert(tableInfoQuery.value(1).toString());
    tableInfoQuery.finish();

    if (!existingColumns.contains(QStringLiteral("source_path")) &&
        !executeSchemaStatement(
            db,
            QStringLiteral(
                "ALTER TABLE thumbnails ADD COLUMN source_path TEXT;"),
            QStringLiteral("Failed to add thumbnail source-path column"))) {
        return failSchemaTransaction();
    }
    if (!existingColumns.contains(QStringLiteral("last_accessed")) &&
        !executeSchemaStatement(
            db,
            QStringLiteral(
                "ALTER TABLE thumbnails ADD COLUMN "
                "last_accessed INTEGER NOT NULL DEFAULT 0;"),
            QStringLiteral("Failed to add thumbnail access-time column"))) {
        return failSchemaTransaction();
    }

    const QStringList schemaStatements{
        QStringLiteral("DROP INDEX IF EXISTS idx_thumbnails_last_accessed;"),
        QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_thumbnails_lru "
            "ON thumbnails(last_accessed, id);"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS thumbnail_cache_state ("
            "singleton_id INTEGER PRIMARY KEY CHECK(singleton_id = 1), "
            "entry_count INTEGER NOT NULL DEFAULT 0 "
            "    CHECK(entry_count >= 0), "
            "payload_bytes INTEGER NOT NULL DEFAULT 0 "
            "    CHECK(payload_bytes >= 0), "
            "stale_cursor TEXT NOT NULL DEFAULT ''"
            ");"),
        QStringLiteral(
            "INSERT OR IGNORE INTO thumbnail_cache_state "
            "(singleton_id, entry_count, payload_bytes, stale_cursor) "
            "SELECT 1, COUNT(*), COALESCE(SUM(LENGTH(data)), 0), '' "
            "FROM thumbnails;"),
        QStringLiteral(
            "CREATE TRIGGER IF NOT EXISTS thumbnail_cache_stats_insert "
            "AFTER INSERT ON thumbnails BEGIN "
            "UPDATE thumbnail_cache_state "
            "SET entry_count = entry_count + 1, "
            "    payload_bytes = payload_bytes + "
            "        COALESCE(LENGTH(NEW.data), 0) "
            "WHERE singleton_id = 1; "
            "END;"),
        QStringLiteral(
            "CREATE TRIGGER IF NOT EXISTS thumbnail_cache_stats_delete "
            "AFTER DELETE ON thumbnails BEGIN "
            "UPDATE thumbnail_cache_state "
            "SET entry_count = MAX(0, entry_count - 1), "
            "    payload_bytes = MAX("
            "        0, payload_bytes - COALESCE(LENGTH(OLD.data), 0)"
            "    ) "
            "WHERE singleton_id = 1; "
            "END;"),
        QStringLiteral(
            "CREATE TRIGGER IF NOT EXISTS thumbnail_cache_stats_update "
            "AFTER UPDATE OF data ON thumbnails BEGIN "
            "UPDATE thumbnail_cache_state "
            "SET payload_bytes = MAX("
            "        0, payload_bytes - COALESCE(LENGTH(OLD.data), 0) + "
            "           COALESCE(LENGTH(NEW.data), 0)"
            "    ) "
            "WHERE singleton_id = 1; "
            "END;"),
    };

    for (const QString &statement : schemaStatements) {
        if (!executeSchemaStatement(
                db, statement,
                QStringLiteral("Failed to initialize thumbnail cache schema"))) {
            return failSchemaTransaction();
        }
    }

    if (!db.commit()) {
        qWarning() << "Failed to commit thumbnail cache schema transaction:"
                   << db.lastError().text();
        return failSchemaTransaction();
    }
    return true;
}

bool ThumbnailCache::executeSchemaStatement(QSqlDatabase &db,
                                            const QString &statement,
                                            const QString &operation)
{
    QSqlQuery query(db);
    if (query.exec(statement))
        return true;

    qWarning() << operation << ':' << query.lastError().text();
    return false;
}

QString ThumbnailCache::thumbnailPath(QString id)
{
    Q_UNUSED(id)
    return databasePath;
}

bool ThumbnailCache::exists(QString id)
{
    QSqlDatabase db = getDatabaseConnection();
    if (!db.isOpen()) {
        qWarning() << "Cannot inspect a closed thumbnail cache database";
        return false;
    }
    if (!ensureStartupMaintenance(db))
        qWarning() << "Thumbnail cache startup maintenance will be retried";

    std::lock_guard lock(sDatabaseAccessMutex);
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT 1 FROM thumbnails "
        "WHERE id = :id AND LENGTH(data) > 0;"));
    query.bindValue(QStringLiteral(":id"), id);
    if (!query.exec()) {
        qWarning() << "Failed to inspect thumbnail cache entry:"
                   << query.lastError().text();
        return false;
    }
    return query.next();
}

bool ThumbnailCache::saveThumbnail(const QImage *image, QString id,
                                   const QString &sourcePath)
{
    if (!image || image->isNull()) {
        qWarning() << "Cannot cache an invalid thumbnail image";
        return false;
    }
    if (id.isEmpty()) {
        qWarning() << "Cannot cache a thumbnail with an empty identifier";
        return false;
    }

    QByteArray encodedThumbnail;
    QBuffer buffer(&encodedThumbnail);
    if (!buffer.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to open thumbnail encoding buffer:"
                   << buffer.errorString();
        return false;
    }

    QImage thumbCopy = *image;
    thumbCopy.setText(QStringLiteral("effort"),
                      QString::number(kThumbnailEncodingEffort));
    const bool encoded =
        thumbCopy.save(&buffer, kThumbnailEncodingFormat,
                       kThumbnailEncodingQuality);
    buffer.close();
    if (!encoded || encodedThumbnail.isEmpty()) {
        qWarning() << "Failed to encode thumbnail as JXL";
        return false;
    }

    QSqlDatabase db = getDatabaseConnection();
    if (!db.isOpen()) {
        qWarning() << "Cannot save to a closed thumbnail cache database";
        return false;
    }
    if (!ensureStartupMaintenance(db)) {
        qWarning() << "Cannot save before thumbnail cache maintenance succeeds";
        return false;
    }

    std::lock_guard lock(sDatabaseAccessMutex);
    const ThumbnailCacheMaintenance::Quota quota = currentQuota();
    if (quota.maximumStorageBytes > 0 &&
        encodedThumbnail.size() >= quota.maximumStorageBytes) {
        qWarning() << "Encoded thumbnail is too large for the cache quota:"
                   << encodedThumbnail.size() << "bytes";
        return false;
    }

    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "INSERT INTO thumbnails "
        "(id, last_modified, original_width, original_height, label, data, "
        " source_path, last_accessed) "
        "VALUES "
        "(:id, :last_modified, :original_width, :original_height, :label, "
        " :data, :source_path, :last_accessed) "
        "ON CONFLICT(id) DO UPDATE SET "
        "last_modified = excluded.last_modified, "
        "original_width = excluded.original_width, "
        "original_height = excluded.original_height, "
        "label = excluded.label, "
        "data = excluded.data, "
        "source_path = excluded.source_path, "
        "last_accessed = excluded.last_accessed;"));
    query.bindValue(QStringLiteral(":id"), id);
    query.bindValue(QStringLiteral(":last_modified"),
                    image->text(QStringLiteral("lastModified")));
    query.bindValue(QStringLiteral(":original_width"),
                    image->text(QStringLiteral("originalWidth")).toInt());
    query.bindValue(QStringLiteral(":original_height"),
                    image->text(QStringLiteral("originalHeight")).toInt());
    query.bindValue(QStringLiteral(":label"),
                    image->text(QStringLiteral("label")));
    query.bindValue(QStringLiteral(":data"), encodedThumbnail);
    query.bindValue(QStringLiteral(":source_path"), sourcePath);
    query.bindValue(QStringLiteral(":last_accessed"),
                    QDateTime::currentSecsSinceEpoch());

    if (!query.exec()) {
        qWarning() << "Failed to save thumbnail to database:"
                   << query.lastError().text();
        return false;
    }
    query.finish();

    const bool growthMaintenance =
        maintenanceGrowthLimitReached(encodedThumbnail.size());
    ThumbnailCacheMaintenance::Result inspection =
        maintenance.inspect(db, quota);
    if (!inspection.succeeded) {
        qWarning() << inspection.errorMessage;
        if (!removeEntry(db, id,
                         QStringLiteral("usage inspection failed after insert"))) {
            return false;
        }
        return false;
    }

    if (!inspection.withinQuota) {
        ThumbnailCacheMaintenance::Request maintenanceRequest;
        maintenanceRequest.quota = quota;
        maintenanceRequest.protectedEntryId = id;
        maintenanceRequest.scanStalePaths = growthMaintenance;
        if (!runMaintenanceLocked(db, maintenanceRequest)) {
            if (!removeEntry(
                    db, id,
                    QStringLiteral("cache quota could not be enforced"))) {
                return false;
            }
            ThumbnailCacheMaintenance::Request recoveryRequest;
            recoveryRequest.quota = quota;
            recoveryRequest.reconcileUsage = true;
            if (!runMaintenanceLocked(db, recoveryRequest)) {
                qWarning() << "Thumbnail cache recovery maintenance failed";
            }
            return false;
        }
    }

    if (growthMaintenance && inspection.withinQuota) {
        ThumbnailCacheMaintenance::Request maintenanceRequest;
        maintenanceRequest.quota = quota;
        maintenanceRequest.protectedEntryId = id;
        maintenanceRequest.scanStalePaths = true;
        if (!runMaintenanceLocked(db, maintenanceRequest)) {
            qWarning() << "Thumbnail cache growth maintenance will be retried";
        }
    }
    return true;
}

bool ThumbnailCache::performStartupMaintenance()
{
    QSqlDatabase db = getDatabaseConnection();
    if (!db.isOpen()) {
        qWarning() << "Cannot maintain a closed thumbnail cache database";
        return false;
    }
    return ensureStartupMaintenance(db);
}

std::unique_ptr<QImage> ThumbnailCache::readThumbnail(QString id)
{
    QSqlDatabase db = getDatabaseConnection();
    if (!db.isOpen()) {
        qWarning() << "Cannot read from a closed thumbnail cache database";
        return nullptr;
    }
    if (!ensureStartupMaintenance(db))
        qWarning() << "Thumbnail cache startup maintenance will be retried";

    QString lastModified;
    int originalWidth = 0;
    int originalHeight = 0;
    QString label;
    QByteArray encodedThumbnail;
    qint64 lastAccessed = 0;

    {
        std::lock_guard lock(sDatabaseAccessMutex);
        QSqlQuery query(db);
        query.prepare(QStringLiteral(
            "SELECT last_modified, original_width, original_height, label, "
            "data, last_accessed FROM thumbnails WHERE id = :id;"));
        query.bindValue(QStringLiteral(":id"), id);

        if (!query.exec()) {
            qWarning() << "Failed to read thumbnail cache entry:"
                       << query.lastError().text();
            return nullptr;
        }
        if (!query.next())
            return nullptr;

        lastModified = query.value(0).toString();
        originalWidth = query.value(1).toInt();
        originalHeight = query.value(2).toInt();
        label = query.value(3).toString();
        encodedThumbnail = query.value(4).toByteArray();
        lastAccessed = query.value(5).toLongLong();
        query.finish();

        const qint64 now = QDateTime::currentSecsSinceEpoch();
        if (now - lastAccessed > kAccessTouchIntervalSeconds) {
            QSqlQuery touchQuery(db);
            touchQuery.prepare(QStringLiteral(
                "UPDATE thumbnails SET last_accessed = :now WHERE id = :id;"));
            touchQuery.bindValue(QStringLiteral(":now"), now);
            touchQuery.bindValue(QStringLiteral(":id"), id);
            if (!touchQuery.exec()) {
                qWarning() << "Failed to update thumbnail cache access time:"
                           << touchQuery.lastError().text();
            }
        }
    }

    auto thumbnail = std::make_unique<QImage>();
    if (!encodedThumbnail.isEmpty() &&
        thumbnail->loadFromData(encodedThumbnail)) {
        thumbnail->setText(QStringLiteral("lastModified"), lastModified);
        thumbnail->setText(QStringLiteral("originalWidth"),
                           QString::number(originalWidth));
        thumbnail->setText(QStringLiteral("originalHeight"),
                           QString::number(originalHeight));
        thumbnail->setText(QStringLiteral("label"), label);
        return thumbnail;
    }

    std::lock_guard lock(sDatabaseAccessMutex);
    if (!removeEntry(db, id,
                     QStringLiteral("thumbnail data is empty or corrupt"))) {
        qWarning() << "Corrupt thumbnail cache entry could not be removed";
    }
    return nullptr;
}

bool ThumbnailCache::clear()
{
    QSqlDatabase db = getDatabaseConnection();
    if (!db.isOpen()) {
        qWarning() << "Cannot clear a closed thumbnail cache database";
        return false;
    }

    std::lock_guard lock(sDatabaseAccessMutex);
    QSqlQuery deleteQuery(db);
    if (!deleteQuery.exec(QStringLiteral("DELETE FROM thumbnails;"))) {
        qWarning() << "Failed to clear thumbnail cache:"
                   << deleteQuery.lastError().text();
        return false;
    }
    deleteQuery.finish();

    QSqlQuery cursorQuery(db);
    cursorQuery.prepare(QStringLiteral(
        "UPDATE thumbnail_cache_state SET stale_cursor = '' "
        "WHERE singleton_id = 1;"));
    if (!cursorQuery.exec()) {
        qWarning() << "Failed to reset thumbnail cleanup cursor:"
                   << cursorQuery.lastError().text();
        return false;
    }
    cursorQuery.finish();

    ThumbnailCacheMaintenance::Request request;
    request.quota = currentQuota();
    request.reconcileUsage = true;
    const ThumbnailCacheMaintenance::Result result = maintenance.run(db, request);
    if (!result.succeeded)
        qWarning() << result.errorMessage;

    bytesSinceMaintenance = 0;
    startupMaintenanceComplete.store(result.succeeded,
                                     std::memory_order_release);
    return result.succeeded;
}

bool ThumbnailCache::ensureStartupMaintenance(QSqlDatabase &db)
{
    if (startupMaintenanceComplete.load(std::memory_order_acquire))
        return true;

    std::lock_guard startupLock(startupMaintenanceMutex);
    if (startupMaintenanceComplete.load(std::memory_order_relaxed))
        return true;

    std::lock_guard databaseLock(sDatabaseAccessMutex);
    ThumbnailCacheMaintenance::Request request;
    request.quota = currentQuota();
    request.scanStalePaths = true;
    request.reconcileUsage = true;
    const bool succeeded = runMaintenanceLocked(db, request);
    startupMaintenanceComplete.store(succeeded, std::memory_order_release);
    return succeeded;
}

ThumbnailCacheMaintenance::Quota ThumbnailCache::currentQuota() const
{
    ThumbnailCacheMaintenance::Quota quota;
    const qint64 maximumMegabytes = settings->thumbnailCacheMaxSizeMB();
    if (maximumMegabytes > 0 &&
        maximumMegabytes <=
            std::numeric_limits<qint64>::max() / kBytesPerMegabyte) {
        quota.maximumStorageBytes = maximumMegabytes * kBytesPerMegabyte;
    } else if (maximumMegabytes > 0) {
        quota.maximumStorageBytes = std::numeric_limits<qint64>::max();
    }
    quota.maximumEntries = settings->thumbnailCacheMaxEntries();
    return quota;
}

bool ThumbnailCache::removeEntry(QSqlDatabase &db, const QString &id,
                                 const QString &reason)
{
    QSqlQuery query(db);
    query.prepare(QStringLiteral("DELETE FROM thumbnails WHERE id = :id;"));
    query.bindValue(QStringLiteral(":id"), id);
    if (!query.exec()) {
        qWarning() << "Failed to remove thumbnail cache entry after" << reason
                   << ':' << query.lastError().text();
        return false;
    }
    qWarning() << "Removed thumbnail cache entry because" << reason << ':' << id;
    return true;
}

bool ThumbnailCache::runMaintenanceLocked(
    QSqlDatabase &db,
    const ThumbnailCacheMaintenance::Request &request)
{
    const ThumbnailCacheMaintenance::Result result = maintenance.run(db, request);
    if (!result.succeeded) {
        qWarning() << "Thumbnail cache maintenance failed:"
                   << result.errorMessage;
        return false;
    }

    bytesSinceMaintenance = 0;
    return true;
}

bool ThumbnailCache::maintenanceGrowthLimitReached(qint64 encodedBytes)
{
    if (encodedBytes <= 0)
        return false;

    const qint64 remaining =
        std::max<qint64>(0, kMaintenanceGrowthBytes - bytesSinceMaintenance);
    if (encodedBytes >= remaining) {
        bytesSinceMaintenance = kMaintenanceGrowthBytes;
        return true;
    }

    bytesSinceMaintenance += encodedBytes;
    return false;
}
