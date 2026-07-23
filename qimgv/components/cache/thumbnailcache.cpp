#include "thumbnailcache.h"
#include "settings.h"
#include <QBuffer>
#include <QThread>
#include <QFileInfo>
#include <QDateTime>
#include <QSet>

ThumbnailCache::ThumbnailCache() {
    cacheDirPath = settings->thumbnailCacheDir();
}

QSqlDatabase ThumbnailCache::getDatabaseConnection() {
    if (threadConnections.hasLocalData()) {
        QSqlDatabase &db = threadConnections.localData()->db;
        if (db.isOpen() || db.open())
            return db;
        return db;
    }

    const QString connectionName = QStringLiteral("thumbnail_db_%1")
        .arg(reinterpret_cast<quintptr>(QThread::currentThreadId()));

    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    db.setDatabaseName(cacheDirPath + QStringLiteral("thumbnails.db"));
    if (db.open()) {
        QSqlQuery query(db);
        query.exec(QStringLiteral("PRAGMA journal_mode=WAL;"));
        query.exec(QStringLiteral("PRAGMA synchronous=NORMAL;"));
        query.exec(QStringLiteral("PRAGMA busy_timeout=2000;"));
        query.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS thumbnails ("
                   "id TEXT PRIMARY KEY, "
                   "last_modified TEXT, "
                   "original_width INTEGER, "
                   "original_height INTEGER, "
                   "label TEXT, "
                   "data BLOB);"));

        // Schema migration: older databases predate these columns.
        // source_path enables stale-entry cleanup; last_accessed enables
        // LRU-based quota eviction. Check first since SQLite errors on
        // ALTER TABLE ADD COLUMN if the column already exists.
        QSet<QString> existingColumns;
        QSqlQuery pragma(db);
        if (pragma.exec(QStringLiteral("PRAGMA table_info(thumbnails);"))) {
            while (pragma.next())
                existingColumns.insert(pragma.value(1).toString());
        }
        if (!existingColumns.contains(QStringLiteral("source_path")))
            query.exec(QStringLiteral("ALTER TABLE thumbnails ADD COLUMN source_path TEXT;"));
        if (!existingColumns.contains(QStringLiteral("last_accessed")))
            query.exec(QStringLiteral("ALTER TABLE thumbnails ADD COLUMN last_accessed INTEGER DEFAULT 0;"));

        query.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_thumbnails_last_accessed "
                   "ON thumbnails(last_accessed);"));
    }

    // QThreadStorage takes ownership; ThreadLocalConnection's destructor
    // will run on this same thread (on exit or replacement), closing and
    // removing the connection with correct thread affinity.
    threadConnections.setLocalData(new ThreadLocalConnection(db, connectionName));
    return db;
}

QString ThumbnailCache::thumbnailPath(QString id) {
    Q_UNUSED(id)
    return cacheDirPath + "thumbnails.db";
}

bool ThumbnailCache::exists(QString id) {
    QSqlDatabase db = getDatabaseConnection();
    if (!db.isOpen()) return false;
    
    QSqlQuery query(db);
    query.prepare("SELECT 1 FROM thumbnails WHERE id = :id");
    query.bindValue(":id", id);
    if (query.exec() && query.next()) {
        return true;
    }
    return false;
}

void ThumbnailCache::saveThumbnail(const QImage *image, QString id, const QString &sourcePath) {
    if (!image) return;
    
    QSqlDatabase db = getDatabaseConnection();
    if (!db.isOpen()) return;
    
    QByteArray ba;
    QBuffer buffer(&ba);
    buffer.open(QIODevice::WriteOnly);
    
    QImage thumbCopy = *image; // cheap shallow copy
    thumbCopy.setText(QStringLiteral("effort"), QStringLiteral("2"));
    thumbCopy.save(&buffer, "JXL", 85);
    
    QSqlQuery query(db);
    query.prepare("INSERT OR REPLACE INTO thumbnails "
                  "(id, last_modified, original_width, original_height, label, data, source_path, last_accessed) "
                  "VALUES (:id, :last_modified, :original_width, :original_height, :label, :data, :source_path, :last_accessed)");
    query.bindValue(":id", id);
    query.bindValue(":last_modified", image->text(QStringLiteral("lastModified")));
    query.bindValue(":original_width", image->text(QStringLiteral("originalWidth")).toInt());
    query.bindValue(":original_height", image->text(QStringLiteral("originalHeight")).toInt());
    query.bindValue(":label", image->text(QStringLiteral("label")));
    query.bindValue(":data", ba);
    query.bindValue(":source_path", sourcePath);
    query.bindValue(":last_accessed", QDateTime::currentSecsSinceEpoch());
    
    if (!query.exec()) {
        qWarning() << "Failed to save thumbnail to DB:" << query.lastError().text();
    }

    maybeRunMaintenance();
}

std::unique_ptr<QImage> ThumbnailCache::readThumbnail(QString id) {
    QSqlDatabase db = getDatabaseConnection();
    if (!db.isOpen()) return nullptr;
    
    QSqlQuery query(db);
    query.prepare("SELECT last_modified, original_width, original_height, label, data, last_accessed "
                  "FROM thumbnails WHERE id = :id");
    query.bindValue(":id", id);
    
    if (query.exec() && query.next()) {
        QString lastModified = query.value(0).toString();
        int originalWidth = query.value(1).toInt();
        int originalHeight = query.value(2).toInt();
        QString label = query.value(3).toString();
        QByteArray ba = query.value(4).toByteArray();
        qint64 lastAccessed = query.value(5).toLongLong();

        // Throttle the "touch" write: only update last_accessed once per
        // hour per entry. This keeps LRU ordering meaningful for quota
        // eviction without turning every cache hit into a disk write
        // during normal browsing.
        qint64 now = QDateTime::currentSecsSinceEpoch();
        if (now - lastAccessed > 3600) {
            QSqlQuery touchQuery(db);
            touchQuery.prepare("UPDATE thumbnails SET last_accessed = :now WHERE id = :id");
            touchQuery.bindValue(":now", now);
            touchQuery.bindValue(":id", id);
            touchQuery.exec();
        }
        
        auto thumb = std::make_unique<QImage>();
        if (thumb->loadFromData(ba)) {
            thumb->setText(QStringLiteral("lastModified"), lastModified);
            thumb->setText(QStringLiteral("originalWidth"), QString::number(originalWidth));
            thumb->setText(QStringLiteral("originalHeight"), QString::number(originalHeight));
            thumb->setText(QStringLiteral("label"), label);
            return thumb;
        }
    }
    return nullptr;
}

void ThumbnailCache::clear() {
    QSqlDatabase db = getDatabaseConnection();
    if (db.isOpen()) {
        QSqlQuery query(db);
        query.exec("DELETE FROM thumbnails;");
        query.exec("VACUUM;");
    }
}

void ThumbnailCache::enforceQuota(qint64 maxBytes) {
    if (maxBytes <= 0)
        return;

    QSqlDatabase db = getDatabaseConnection();
    if (!db.isOpen()) return;

    QSqlQuery totalQuery(db);
    qint64 total = 0;
    if (totalQuery.exec(QStringLiteral("SELECT COALESCE(SUM(LENGTH(data)), 0) FROM thumbnails;")) &&
        totalQuery.next()) {
        total = totalQuery.value(0).toLongLong();
    }

    if (total <= maxBytes)
        return;

    // Delete oldest-accessed rows first (LRU), in small batches, until
    // we're back under quota. Batching avoids a single huge DELETE while
    // still converging quickly for normal quota overshoots.
    const int batchSize = 50;
    QSqlQuery deleteQuery(db);
    deleteQuery.prepare(
        "DELETE FROM thumbnails WHERE id IN ("
        "  SELECT id FROM thumbnails ORDER BY last_accessed ASC, id ASC LIMIT :n"
        ");");

    int safetyIterations = 0;
    while (total > maxBytes && safetyIterations < 10000) {
        deleteQuery.bindValue(":n", batchSize);
        if (!deleteQuery.exec()) {
            qWarning() << "Failed to enforce thumbnail cache quota:" << deleteQuery.lastError().text();
            break;
        }
        if (deleteQuery.numRowsAffected() <= 0)
            break; // nothing left to delete

        if (totalQuery.exec(QStringLiteral("SELECT COALESCE(SUM(LENGTH(data)), 0) FROM thumbnails;")) &&
            totalQuery.next()) {
            total = totalQuery.value(0).toLongLong();
        } else {
            break;
        }
        ++safetyIterations;
    }
}

int ThumbnailCache::cleanupStalePaths(int maxChecks) {
    QSqlDatabase db = getDatabaseConnection();
    if (!db.isOpen()) return 0;

    // Check the least-recently-used entries first: they're both the
    // cheapest to lose and the most likely to point at a file that has
    // since been moved or deleted.
    QSqlQuery selectQuery(db);
    selectQuery.prepare(
        "SELECT id, source_path FROM thumbnails "
        "WHERE source_path IS NOT NULL AND source_path != '' "
        "ORDER BY last_accessed ASC LIMIT :n");
    selectQuery.bindValue(":n", maxChecks);
    if (!selectQuery.exec())
        return 0;

    QStringList staleIds;
    while (selectQuery.next()) {
        QString id = selectQuery.value(0).toString();
        QString path = selectQuery.value(1).toString();
        if (!QFileInfo::exists(path))
            staleIds << id;
    }

    if (staleIds.isEmpty())
        return 0;

    QStringList placeholders;
    for (int i = 0; i < staleIds.size(); ++i)
        placeholders << QStringLiteral("?");

    QSqlQuery deleteQuery(db);
    deleteQuery.prepare(QStringLiteral("DELETE FROM thumbnails WHERE id IN (%1);").arg(placeholders.join(',')));
    for (const QString &id : staleIds)
        deleteQuery.addBindValue(id);

    if (!deleteQuery.exec()) {
        qWarning() << "Failed to clean up stale thumbnail entries:" << deleteQuery.lastError().text();
        return 0;
    }

    return staleIds.size();
}

void ThumbnailCache::maybeRunMaintenance() {
    int count = mSaveCounter.fetch_add(1, std::memory_order_relaxed) + 1;
    if (count % kMaintenanceInterval != 0)
        return;

    qint64 maxBytes = static_cast<qint64>(settings->thumbnailCacheMaxSizeMB()) * 1024 * 1024;
    enforceQuota(maxBytes);
    cleanupStalePaths(200);
}
