#include "thumbnailcache.h"

#include "settings.h"

#include <QDateTime>
#include <QDebug>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QThread>
#include <algorithm>
#include <limits>

std::mutex ThumbnailCache::sDatabaseWriteMutex;

ThumbnailCache::ThumbnailCache()
    : databasePath(settings->thumbnailCacheDir() +
                   QStringLiteral("thumbnails.db")),
      maintenance(databasePath),
      decodedCache(kDecodedCacheMaximumBytes)
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
            if (!connection->db.isOpen() && !connection->db.open()) {
                qWarning() << "Failed to reopen thumbnail cache database:"
                           << connection->db.lastError().text();
                return connection->db;
            }
            connection->schemaReady =
                configureDatabaseConnection(connection->db) &&
                ensureDatabaseInitialized(connection->db);
            if (!connection->schemaReady)
                connection->db.close();
        }
        return connection->db;
    }

    const QString connectionName =
        QStringLiteral("thumbnail_db_%1_%2")
            .arg(reinterpret_cast<quintptr>(this))
            .arg(reinterpret_cast<quintptr>(QThread::currentThreadId()));

    QSqlDatabase db =
        QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    db.setDatabaseName(databasePath);

    bool schemaReady = false;
    if (!db.open()) {
        qWarning() << "Failed to open thumbnail cache database:"
                   << db.lastError().text();
    } else {
        schemaReady =
            configureDatabaseConnection(db) &&
            ensureDatabaseInitialized(db);
        if (!schemaReady)
            db.close();
    }

    threadConnections.setLocalData(
        new ThreadLocalConnection(db, connectionName, schemaReady));
    return db;
}

bool ThumbnailCache::configureDatabaseConnection(QSqlDatabase &db)
{
    if (!executeSchemaStatement(
            db,
            QStringLiteral("PRAGMA busy_timeout=%1;")
                .arg(kDatabaseBusyTimeoutMilliseconds),
            QStringLiteral("Failed to configure thumbnail cache busy timeout")) ||
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
    return true;
}

bool ThumbnailCache::ensureDatabaseInitialized(QSqlDatabase &db)
{
    if (databaseInitialized.load(std::memory_order_acquire))
        return true;

    std::lock_guard lock(sDatabaseWriteMutex);
    if (databaseInitialized.load(std::memory_order_relaxed))
        return true;

    const bool initialized = initializeDatabaseSchema(db);
    databaseInitialized.store(initialized, std::memory_order_release);
    return initialized;
}

bool ThumbnailCache::initializeDatabaseSchema(QSqlDatabase &db)
{
    if (!executeSchemaStatement(
            db, QStringLiteral("PRAGMA journal_mode=WAL;"),
            QStringLiteral("Failed to enable thumbnail cache WAL mode"))) {
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
                "source_mtime INTEGER, "
                "source_size INTEGER, "
                "requires_linear_color_space INTEGER NOT NULL DEFAULT 0, "
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
    if (!existingColumns.contains(QStringLiteral("source_mtime")) &&
        !executeSchemaStatement(
            db,
            QStringLiteral(
                "ALTER TABLE thumbnails ADD COLUMN source_mtime INTEGER;"),
            QStringLiteral("Failed to add thumbnail source-mtime column"))) {
        return failSchemaTransaction();
    }
    if (!existingColumns.contains(QStringLiteral("source_size")) &&
        !executeSchemaStatement(
            db,
            QStringLiteral(
                "ALTER TABLE thumbnails ADD COLUMN source_size INTEGER;"),
            QStringLiteral("Failed to add thumbnail source-size column"))) {
        return failSchemaTransaction();
    }
    if (!existingColumns.contains(
            QStringLiteral("requires_linear_color_space")) &&
        !executeSchemaStatement(
            db,
            QStringLiteral(
                "ALTER TABLE thumbnails ADD COLUMN "
                "requires_linear_color_space INTEGER NOT NULL DEFAULT 0;"),
            QStringLiteral(
                "Failed to add thumbnail color-space metadata column"))) {
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

bool ThumbnailCache::saveThumbnails(const QList<WriteEntry> &entries)
{
    if (entries.isEmpty()) {
        qWarning() << "Cannot save an empty thumbnail cache batch";
        return false;
    }
    for (const WriteEntry &entry : entries) {
        if (entry.id.isEmpty() || entry.encodedData.isEmpty() ||
            entry.sourceStamp.normalizedPath.isEmpty() ||
            entry.sourceStamp.size < 0) {
            qWarning() << "Cannot save an invalid thumbnail cache entry";
            return false;
        }
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

    std::lock_guard lock(sDatabaseWriteMutex);
    const ThumbnailCacheMaintenance::Quota quota = currentQuota();
    QList<const WriteEntry *> persistableEntries;
    persistableEntries.reserve(entries.size());
    for (const WriteEntry &entry : entries) {
        if (quota.maximumStorageBytes > 0 &&
            entry.encodedData.size() >= quota.maximumStorageBytes) {
            qWarning() << "Encoded thumbnail is too large for the cache quota:"
                       << entry.encodedData.size() << "bytes";
            continue;
        }
        persistableEntries.push_back(&entry);
    }
    if (persistableEntries.isEmpty())
        return true;

    if (!db.transaction()) {
        qWarning() << "Failed to start thumbnail cache write transaction:"
                   << db.lastError().text();
        return false;
    }

    const auto rollbackTransaction = [&db]() {
        if (!db.rollback()) {
            qWarning() << "Failed to roll back thumbnail cache write "
                          "transaction:"
                       << db.lastError().text();
        }
        return false;
    };

    QSqlQuery query(db);
    if (!query.prepare(QStringLiteral(
            "INSERT INTO thumbnails "
            "(id, last_modified, original_width, original_height, label, "
            " data, source_path, source_mtime, source_size, "
            " requires_linear_color_space, last_accessed) "
            "VALUES "
            "(:id, :last_modified, :original_width, :original_height, "
            " :label, :data, :source_path, :source_mtime, :source_size, "
            " :requires_linear_color_space, :last_accessed) "
            "ON CONFLICT(id) DO UPDATE SET "
            "last_modified = excluded.last_modified, "
            "original_width = excluded.original_width, "
            "original_height = excluded.original_height, "
            "label = excluded.label, "
            "data = excluded.data, "
            "source_path = excluded.source_path, "
            "source_mtime = excluded.source_mtime, "
            "source_size = excluded.source_size, "
            "requires_linear_color_space = "
            "    excluded.requires_linear_color_space, "
            "last_accessed = excluded.last_accessed;"))) {
        qWarning() << "Failed to prepare thumbnail cache batch UPSERT:"
                   << query.lastError().text();
        return rollbackTransaction();
    }

    qint64 encodedBatchBytes = 0;
    const qint64 lastAccessed = QDateTime::currentSecsSinceEpoch();
    for (const WriteEntry *entry : persistableEntries) {
        query.bindValue(QStringLiteral(":id"), entry->id);
        query.bindValue(QStringLiteral(":last_modified"),
                        QString::number(
                            entry->sourceStamp.modifiedTimeTicks));
        query.bindValue(QStringLiteral(":original_width"),
                        entry->originalWidth);
        query.bindValue(QStringLiteral(":original_height"),
                        entry->originalHeight);
        query.bindValue(QStringLiteral(":label"), entry->label);
        query.bindValue(QStringLiteral(":data"), entry->encodedData);
        query.bindValue(QStringLiteral(":source_path"),
                        entry->sourceStamp.normalizedPath);
        query.bindValue(QStringLiteral(":source_mtime"),
                        entry->sourceStamp.modifiedTimeTicks);
        query.bindValue(QStringLiteral(":source_size"),
                        entry->sourceStamp.size);
        query.bindValue(QStringLiteral(":requires_linear_color_space"),
                        entry->requiresLinearColorSpace);
        query.bindValue(QStringLiteral(":last_accessed"),
                        lastAccessed);

        if (!query.exec()) {
            qWarning() << "Failed to save thumbnail batch entry:"
                       << query.lastError().text();
            return rollbackTransaction();
        }
        query.finish();

        const qint64 entryBytes = entry->encodedData.size();
        if (entryBytes >
            std::numeric_limits<qint64>::max() - encodedBatchBytes) {
            encodedBatchBytes = std::numeric_limits<qint64>::max();
        } else {
            encodedBatchBytes += entryBytes;
        }
    }

    if (!db.commit()) {
        qWarning() << "Failed to commit thumbnail cache write transaction:"
                   << db.lastError().text();
        return rollbackTransaction();
    }

    const bool growthMaintenance =
        maintenanceGrowthLimitReached(encodedBatchBytes);
    ThumbnailCacheMaintenance::Result inspection =
        maintenance.inspect(db, quota);
    if (!inspection.succeeded) {
        qWarning() << inspection.errorMessage;
        return false;
    }

    if (!inspection.withinQuota) {
        ThumbnailCacheMaintenance::Request maintenanceRequest;
        maintenanceRequest.quota = quota;
        maintenanceRequest.protectedEntryId =
            persistableEntries.constLast()->id;
        maintenanceRequest.scanStalePaths = growthMaintenance;
        if (!runMaintenanceLocked(db, maintenanceRequest)) {
            bool entriesRemoved = true;
            for (const WriteEntry *entry : persistableEntries) {
                if (!removeEntry(
                        db, entry->id,
                        QStringLiteral(
                            "cache quota could not be enforced"))) {
                    entriesRemoved = false;
                }
            }
            ThumbnailCacheMaintenance::Request recoveryRequest;
            recoveryRequest.quota = quota;
            recoveryRequest.reconcileUsage = true;
            if (!runMaintenanceLocked(db, recoveryRequest)) {
                qWarning() << "Thumbnail cache recovery maintenance failed";
            }
            if (!entriesRemoved) {
                qWarning() << "Some thumbnail cache batch entries could not "
                              "be removed after quota enforcement failed";
            }
            return false;
        }
    }

    if (growthMaintenance && inspection.withinQuota) {
        ThumbnailCacheMaintenance::Request maintenanceRequest;
        maintenanceRequest.quota = quota;
        maintenanceRequest.protectedEntryId =
            persistableEntries.constLast()->id;
        maintenanceRequest.scanStalePaths = true;
        if (!runMaintenanceLocked(db, maintenanceRequest)) {
            qWarning() << "Thumbnail cache growth maintenance will be retried";
        }
    }
    return true;
}

bool ThumbnailCache::applyAccessTouches(
    const QList<AccessTouch> &accessTouches)
{
    if (accessTouches.isEmpty()) {
        qWarning() << "Cannot apply an empty thumbnail access batch";
        return false;
    }
    for (const AccessTouch &accessTouch : accessTouches) {
        if (accessTouch.id.isEmpty() || accessTouch.accessedAt <= 0) {
            qWarning() << "Cannot apply an invalid thumbnail access touch";
            return false;
        }
    }

    QSqlDatabase db = getDatabaseConnection();
    if (!db.isOpen()) {
        qWarning() << "Cannot update a closed thumbnail cache database";
        return false;
    }
    if (!ensureStartupMaintenance(db)) {
        qWarning() << "Cannot update access times before maintenance succeeds";
        return false;
    }

    std::lock_guard lock(sDatabaseWriteMutex);
    if (!db.transaction()) {
        qWarning() << "Failed to start thumbnail access transaction:"
                   << db.lastError().text();
        return false;
    }

    const auto rollbackTransaction = [&db]() {
        if (!db.rollback()) {
            qWarning() << "Failed to roll back thumbnail access transaction:"
                       << db.lastError().text();
        }
        return false;
    };

    QSqlQuery query(db);
    if (!query.prepare(QStringLiteral(
            "UPDATE thumbnails "
            "SET last_accessed = MAX(last_accessed, :accessed_at) "
            "WHERE id = :id;"))) {
        qWarning() << "Failed to prepare thumbnail access batch:"
                   << query.lastError().text();
        return rollbackTransaction();
    }

    for (const AccessTouch &accessTouch : accessTouches) {
        query.bindValue(QStringLiteral(":accessed_at"),
                        accessTouch.accessedAt);
        query.bindValue(QStringLiteral(":id"), accessTouch.id);
        if (!query.exec()) {
            qWarning() << "Failed to update thumbnail access time:"
                       << query.lastError().text();
            return rollbackTransaction();
        }
        query.finish();
    }

    if (!db.commit()) {
        qWarning() << "Failed to commit thumbnail access transaction:"
                   << db.lastError().text();
        return rollbackTransaction();
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

ThumbnailCache::ReadResult ThumbnailCache::readThumbnail(
    const QString &id, const ThumbnailSourceStamp &sourceStamp)
{
    if (id.isEmpty() || sourceStamp.normalizedPath.isEmpty() ||
        sourceStamp.size < 0) {
        qWarning() << "Cannot read a thumbnail with an invalid source stamp";
        return {};
    }

    const qint64 now = QDateTime::currentSecsSinceEpoch();
    DecodedThumbnailCache::LookupResult decodedResult =
        decodedCache.lookup(id, sourceStamp, now,
                            kAccessTouchIntervalSeconds);
    if (decodedResult.image) {
        std::optional<AccessTouch> accessTouch;
        if (decodedResult.accessTouchRequired)
            accessTouch = AccessTouch{id, now, 0};
        return {
            std::move(decodedResult.image),
            decodedResult.requiresLinearColorSpace,
            std::move(accessTouch)};
    }

    QSqlDatabase db = getDatabaseConnection();
    if (!db.isOpen()) {
        qWarning() << "Cannot read from a closed thumbnail cache database";
        return {};
    }
    if (!ensureStartupMaintenance(db))
        qWarning() << "Thumbnail cache startup maintenance will be retried";

    int originalWidth = 0;
    int originalHeight = 0;
    QString label;
    QByteArray encodedThumbnail;
    qint64 lastAccessed = 0;
    bool requiresLinearColorSpace = false;

    QSqlQuery query(db);
    if (!query.prepare(QStringLiteral(
            "SELECT original_width, original_height, label, data, "
            "last_accessed, requires_linear_color_space "
            "FROM thumbnails "
            "WHERE id = :id "
            "  AND source_path = :source_path COLLATE NOCASE "
            "  AND source_mtime = :source_mtime "
            "  AND source_size = :source_size;"))) {
        qWarning() << "Failed to prepare thumbnail cache lookup:"
                   << query.lastError().text();
        return {};
    }
    query.bindValue(QStringLiteral(":id"), id);
    query.bindValue(QStringLiteral(":source_path"),
                    sourceStamp.normalizedPath);
    query.bindValue(QStringLiteral(":source_mtime"),
                    sourceStamp.modifiedTimeTicks);
    query.bindValue(QStringLiteral(":source_size"), sourceStamp.size);

    if (!query.exec()) {
        qWarning() << "Failed to read thumbnail cache entry:"
                   << query.lastError().text();
        return {};
    }
    if (!query.next()) {
        query.finish();
        return {};
    }

    originalWidth = query.value(0).toInt();
    originalHeight = query.value(1).toInt();
    label = query.value(2).toString();
    encodedThumbnail = query.value(3).toByteArray();
    lastAccessed = query.value(4).toLongLong();
    requiresLinearColorSpace = query.value(5).toBool();
    query.finish();

    auto thumbnail = std::make_unique<QImage>();
    if (!encodedThumbnail.isEmpty() &&
        thumbnail->loadFromData(encodedThumbnail)) {
        thumbnail->setText(QStringLiteral("originalWidth"),
                           QString::number(originalWidth));
        thumbnail->setText(QStringLiteral("originalHeight"),
                           QString::number(originalHeight));
        thumbnail->setText(QStringLiteral("label"), label);
        decodedCache.insert(id, sourceStamp, *thumbnail,
                            requiresLinearColorSpace, lastAccessed);
        std::optional<AccessTouch> accessTouch;
        if (lastAccessed < now - kAccessTouchIntervalSeconds)
            accessTouch = AccessTouch{id, now, 0};
        return {
            std::move(thumbnail),
            requiresLinearColorSpace,
            std::move(accessTouch)};
    }

    std::lock_guard lock(sDatabaseWriteMutex);
    if (!removeEntry(db, id,
                     QStringLiteral("thumbnail data is empty or corrupt"))) {
        qWarning() << "Corrupt thumbnail cache entry could not be removed";
    }
    return {};
}

void ThumbnailCache::storeDecodedThumbnail(
    const QString &id, const ThumbnailSourceStamp &sourceStamp,
    const QImage &image, bool requiresLinearColorSpace)
{
    decodedCache.insert(
        id, sourceStamp, image, requiresLinearColorSpace,
        QDateTime::currentSecsSinceEpoch());
}

bool ThumbnailCache::clear()
{
    decodedCache.clear();

    QSqlDatabase db = getDatabaseConnection();
    if (!db.isOpen()) {
        qWarning() << "Cannot clear a closed thumbnail cache database";
        return false;
    }

    std::lock_guard lock(sDatabaseWriteMutex);
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

    std::lock_guard databaseLock(sDatabaseWriteMutex);
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
