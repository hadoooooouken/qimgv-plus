#include "thumbnailcachemaintenance.h"

#include <QFileInfo>
#include <QList>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QVariant>
#include <algorithm>
#include <limits>
#include <utility>

namespace {
constexpr auto kCacheStateRowId = 1;

QString sqlError(const QString &operation, const QSqlQuery &query)
{
    return QStringLiteral("%1: %2").arg(operation, query.lastError().text());
}

qint64 saturatedAdd(qint64 left, qint64 right)
{
    if (right > 0 && left > std::numeric_limits<qint64>::max() - right)
        return std::numeric_limits<qint64>::max();
    return left + right;
}
}

ThumbnailCacheMaintenance::ThumbnailCacheMaintenance(QString databasePath)
    : mDatabasePath(std::move(databasePath))
{
}

ThumbnailCacheMaintenance::Result
ThumbnailCacheMaintenance::inspect(QSqlDatabase &db, const Quota &quota) const
{
    Result result;
    if (!loadUsage(db, result.usage, result.errorMessage))
        return result;

    result.withinQuota = isWithinQuota(result.usage, quota);
    result.succeeded = true;
    return result;
}

ThumbnailCacheMaintenance::Result
ThumbnailCacheMaintenance::run(QSqlDatabase &db, const Request &request) const
{
    Result result;

    if (request.reconcileUsage &&
        !reconcileUsage(db, result.errorMessage)) {
        return result;
    }

    if (request.scanStalePaths &&
        !cleanupStalePaths(db, request.protectedEntryId,
                           result.staleEntriesRemoved, result.errorMessage)) {
        return result;
    }

    EnforcementState state;
    state.quota = request.quota;
    state.protectedEntryId = request.protectedEntryId;
    if (!loadUsage(db, state.usage, state.errorMessage)) {
        result.errorMessage = state.errorMessage;
        return result;
    }

    if (!enforceEntryQuota(db, state)) {
        result.usage = state.usage;
        result.errorMessage = state.errorMessage;
        result.quotaEntriesRemoved = state.removedEntries;
        return result;
    }

    if (!enforceStorageQuota(db, state)) {
        result.usage = state.usage;
        result.errorMessage = state.errorMessage;
        result.quotaEntriesRemoved = state.removedEntries;
        return result;
    }

    bool compact = shouldCompact(db, state.errorMessage);
    if (!state.errorMessage.isEmpty()) {
        result.errorMessage = state.errorMessage;
        return result;
    }

    if (compact &&
        !compactDatabase(db, state.errorMessage)) {
        result.errorMessage = state.errorMessage;
        return result;
    }

    if (!loadUsage(db, state.usage, state.errorMessage)) {
        result.errorMessage = state.errorMessage;
        return result;
    }

    result.usage = state.usage;
    result.quotaEntriesRemoved = state.removedEntries;
    result.withinQuota = isWithinQuota(result.usage, state.quota);
    result.succeeded = result.withinQuota;
    if (!result.withinQuota && result.errorMessage.isEmpty()) {
        result.errorMessage =
            QStringLiteral("Thumbnail cache remains outside its configured quota");
    }
    return result;
}

bool ThumbnailCacheMaintenance::reconcileUsage(
    QSqlDatabase &db, QString &errorMessage) const
{
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "UPDATE thumbnail_cache_state "
        "SET entry_count = (SELECT COUNT(*) FROM thumbnails), "
        "    payload_bytes = ("
        "        SELECT COALESCE(SUM(LENGTH(data)), 0) FROM thumbnails"
        "    ) "
        "WHERE singleton_id = :singleton_id;"));
    query.bindValue(QStringLiteral(":singleton_id"), kCacheStateRowId);
    if (query.exec())
        return true;

    errorMessage = sqlError(
        QStringLiteral("Failed to reconcile thumbnail cache usage"), query);
    return false;
}

bool ThumbnailCacheMaintenance::loadUsage(
    QSqlDatabase &db, Usage &usage, QString &errorMessage) const
{
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT entry_count, payload_bytes "
        "FROM thumbnail_cache_state "
        "WHERE singleton_id = :singleton_id;"));
    query.bindValue(QStringLiteral(":singleton_id"), kCacheStateRowId);
    if (!query.exec()) {
        errorMessage =
            sqlError(QStringLiteral("Failed to read thumbnail cache usage"), query);
        return false;
    }
    if (!query.next()) {
        errorMessage =
            QStringLiteral("Thumbnail cache usage state row is missing");
        return false;
    }

    usage.entryCount = query.value(0).toLongLong();
    usage.payloadBytes = query.value(1).toLongLong();
    usage.storageBytes = databaseFootprintBytes();
    return true;
}

bool ThumbnailCacheMaintenance::loadPageUsage(
    QSqlDatabase &db, PageUsage &usage, QString &errorMessage) const
{
    QSqlQuery pageCountQuery(db);
    if (!pageCountQuery.exec(QStringLiteral("PRAGMA page_count;")) ||
        !pageCountQuery.next()) {
        errorMessage = sqlError(
            QStringLiteral("Failed to read thumbnail cache page count"),
            pageCountQuery);
        return false;
    }
    usage.pageCount = pageCountQuery.value(0).toLongLong();

    QSqlQuery freePageQuery(db);
    if (!freePageQuery.exec(QStringLiteral("PRAGMA freelist_count;")) ||
        !freePageQuery.next()) {
        errorMessage = sqlError(
            QStringLiteral("Failed to read thumbnail cache free-page count"),
            freePageQuery);
        return false;
    }
    usage.freePageCount = freePageQuery.value(0).toLongLong();
    return true;
}

bool ThumbnailCacheMaintenance::checkpointWal(
    QSqlDatabase &db, QString &errorMessage) const
{
    QSqlQuery query(db);
    if (!query.exec(QStringLiteral("PRAGMA wal_checkpoint(TRUNCATE);"))) {
        errorMessage = sqlError(
            QStringLiteral("Failed to checkpoint thumbnail cache WAL"), query);
        return false;
    }
    if (!query.next()) {
        errorMessage =
            QStringLiteral("Thumbnail cache WAL checkpoint returned no status");
        return false;
    }

    const bool checkpointBusy = query.value(0).toInt() != 0;
    if (checkpointBusy) {
        errorMessage =
            QStringLiteral("Thumbnail cache WAL checkpoint is busy");
        return false;
    }
    return true;
}

bool ThumbnailCacheMaintenance::compactDatabase(
    QSqlDatabase &db, QString &errorMessage) const
{
    QSqlQuery vacuumQuery(db);
    if (!vacuumQuery.exec(QStringLiteral("VACUUM;"))) {
        errorMessage = sqlError(
            QStringLiteral("Failed to compact thumbnail cache"), vacuumQuery);
        return false;
    }
    vacuumQuery.finish();

    return checkpointWal(db, errorMessage);
}

bool ThumbnailCacheMaintenance::cleanupStalePaths(
    QSqlDatabase &db, const QString &protectedEntryId, qint64 &removedEntries,
    QString &errorMessage) const
{
    QString cursor;
    {
        QSqlQuery cursorQuery(db);
        cursorQuery.prepare(QStringLiteral(
            "SELECT stale_cursor FROM thumbnail_cache_state "
            "WHERE singleton_id = :singleton_id;"));
        cursorQuery.bindValue(QStringLiteral(":singleton_id"), kCacheStateRowId);
        if (!cursorQuery.exec() || !cursorQuery.next()) {
            errorMessage = sqlError(
                QStringLiteral("Failed to read stale-cleanup cursor"),
                cursorQuery);
            return false;
        }
        cursor = cursorQuery.value(0).toString();
    }

    QList<StaleCandidate> candidates;
    const auto appendCandidates =
        [&db, &candidates, &protectedEntryId, &errorMessage](
            const QString &cursorComparison, const QString &cursorValue,
            qint64 limit) {
            if (limit <= 0)
                return true;

            QSqlQuery query(db);
            query.prepare(QStringLiteral(
                "SELECT id, source_path FROM thumbnails "
                "WHERE source_path IS NOT NULL "
                "  AND source_path != '' "
                "  AND id != :protected_id "
                "  AND id %1 :cursor "
                "ORDER BY id ASC LIMIT :batch_size;")
                              .arg(cursorComparison));
            query.bindValue(QStringLiteral(":protected_id"), protectedEntryId);
            query.bindValue(QStringLiteral(":cursor"), cursorValue);
            query.bindValue(QStringLiteral(":batch_size"), limit);
            if (!query.exec()) {
                errorMessage = sqlError(
                    QStringLiteral("Failed to select stale-cleanup batch"),
                    query);
                return false;
            }
            while (query.next()) {
                candidates.push_back(
                    {query.value(0).toString(), query.value(1).toString()});
            }
            return true;
        };

    if (!appendCandidates(QStringLiteral(">"), cursor,
                          kStaleCleanupBatchSize)) {
        return false;
    }

    const qint64 remaining =
        kStaleCleanupBatchSize - static_cast<qint64>(candidates.size());
    if (remaining > 0 &&
        !appendCandidates(QStringLiteral("<="), cursor, remaining)) {
        return false;
    }

    if (candidates.isEmpty())
        return true;

    QStringList staleIds;
    staleIds.reserve(candidates.size());
    for (const StaleCandidate &candidate : std::as_const(candidates)) {
        if (!QFileInfo::exists(candidate.sourcePath))
            staleIds.push_back(candidate.id);
    }

    if (!db.transaction()) {
        errorMessage = QStringLiteral(
            "Failed to start stale thumbnail cleanup transaction: %1")
                           .arg(db.lastError().text());
        return false;
    }

    if (!staleIds.isEmpty()) {
        QStringList placeholders;
        placeholders.reserve(staleIds.size());
        for (qsizetype index = 0; index < staleIds.size(); ++index)
            placeholders.push_back(QStringLiteral("?"));

        QSqlQuery deleteQuery(db);
        deleteQuery.prepare(
            QStringLiteral("DELETE FROM thumbnails WHERE id IN (%1);")
                .arg(placeholders.join(QLatin1Char(','))));
        for (const QString &id : std::as_const(staleIds))
            deleteQuery.addBindValue(id);

        if (!deleteQuery.exec()) {
            errorMessage = sqlError(
                QStringLiteral("Failed to remove stale thumbnail entries"),
                deleteQuery);
            if (!db.rollback()) {
                errorMessage +=
                    QStringLiteral("; rollback also failed: %1")
                        .arg(db.lastError().text());
            }
            return false;
        }
        removedEntries = std::max<qint64>(
            0, static_cast<qint64>(deleteQuery.numRowsAffected()));
    }

    QSqlQuery cursorUpdate(db);
    cursorUpdate.prepare(QStringLiteral(
        "UPDATE thumbnail_cache_state SET stale_cursor = :cursor "
        "WHERE singleton_id = :singleton_id;"));
    cursorUpdate.bindValue(QStringLiteral(":cursor"), candidates.constLast().id);
    cursorUpdate.bindValue(QStringLiteral(":singleton_id"), kCacheStateRowId);
    if (!cursorUpdate.exec()) {
        errorMessage = sqlError(
            QStringLiteral("Failed to advance stale-cleanup cursor"),
            cursorUpdate);
        if (!db.rollback()) {
            errorMessage += QStringLiteral("; rollback also failed: %1")
                                .arg(db.lastError().text());
        }
        return false;
    }
    cursorUpdate.finish();

    if (!db.commit()) {
        errorMessage =
            QStringLiteral("Failed to commit stale thumbnail cleanup: %1")
                .arg(db.lastError().text());
        if (!db.rollback()) {
            errorMessage += QStringLiteral("; rollback also failed: %1")
                                .arg(db.lastError().text());
        }
        return false;
    }
    return true;
}

bool ThumbnailCacheMaintenance::deleteLruBatch(
    QSqlDatabase &db, const QString &protectedEntryId, qint64 &removedEntries,
    QString &errorMessage) const
{
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "DELETE FROM thumbnails WHERE id IN ("
        "  SELECT id FROM thumbnails "
        "  WHERE id != :protected_id "
        "  ORDER BY last_accessed ASC, id ASC "
        "  LIMIT :batch_size"
        ");"));
    query.bindValue(QStringLiteral(":protected_id"), protectedEntryId);
    query.bindValue(QStringLiteral(":batch_size"), kEvictionBatchSize);
    if (!query.exec()) {
        errorMessage = sqlError(
            QStringLiteral("Failed to evict thumbnail cache entries"), query);
        return false;
    }

    removedEntries =
        std::max<qint64>(0, static_cast<qint64>(query.numRowsAffected()));
    return true;
}

bool ThumbnailCacheMaintenance::enforceEntryQuota(
    QSqlDatabase &db, EnforcementState &state) const
{
    if (state.quota.maximumEntries <= 0)
        return true;

    while (state.usage.entryCount > state.quota.maximumEntries) {
        qint64 removedBatch = 0;
        if (!deleteLruBatch(db, state.protectedEntryId, removedBatch,
                            state.errorMessage)) {
            return false;
        }
        if (removedBatch <= 0) {
            state.errorMessage = QStringLiteral(
                "Thumbnail entry quota cannot be met while preserving the "
                "newly written entry");
            return false;
        }
        state.removedEntries =
            saturatedAdd(state.removedEntries, removedBatch);
        if (!loadUsage(db, state.usage, state.errorMessage))
            return false;
    }
    return true;
}

bool ThumbnailCacheMaintenance::enforceStorageQuota(
    QSqlDatabase &db, EnforcementState &state) const
{
    if (!checkpointWal(db, state.errorMessage))
        return false;
    if (!loadUsage(db, state.usage, state.errorMessage))
        return false;

    if (state.quota.maximumStorageBytes <= 0 ||
        state.usage.storageBytes <= state.quota.maximumStorageBytes) {
        return true;
    }

    PageUsage pageUsage;
    if (!loadPageUsage(db, pageUsage, state.errorMessage))
        return false;
    if (pageUsage.freePageCount > 0) {
        if (!compactDatabase(db, state.errorMessage) ||
            !loadUsage(db, state.usage, state.errorMessage)) {
            return false;
        }
    }

    while (state.usage.storageBytes > state.quota.maximumStorageBytes) {
        if (state.usage.entryCount <= 0) {
            state.errorMessage = QStringLiteral(
                "Thumbnail database metadata exceeds the configured storage "
                "quota");
            return false;
        }

        const qint64 excessBytes =
            state.usage.storageBytes - state.quota.maximumStorageBytes;
        const qint64 headroomBytes =
            state.quota.maximumStorageBytes / kStorageHeadroomDivisor;
        const qint64 desiredReduction =
            saturatedAdd(excessBytes, headroomBytes);
        const qint64 targetPayloadBytes =
            std::max<qint64>(
                0, state.usage.payloadBytes - desiredReduction);

        bool deletedAny = false;
        while (state.usage.payloadBytes > targetPayloadBytes || !deletedAny) {
            qint64 removedBatch = 0;
            if (!deleteLruBatch(db, state.protectedEntryId, removedBatch,
                                state.errorMessage)) {
                return false;
            }
            if (removedBatch <= 0) {
                state.errorMessage = QStringLiteral(
                    "Thumbnail storage quota cannot be met while preserving "
                    "the newly written entry");
                return false;
            }
            deletedAny = true;
            state.removedEntries =
                saturatedAdd(state.removedEntries, removedBatch);
            if (!loadUsage(db, state.usage, state.errorMessage))
                return false;
        }

        if (!compactDatabase(db, state.errorMessage) ||
            !loadUsage(db, state.usage, state.errorMessage)) {
            return false;
        }
    }
    return true;
}

bool ThumbnailCacheMaintenance::shouldCompact(
    QSqlDatabase &db, QString &errorMessage) const
{
    PageUsage usage;
    if (!loadPageUsage(db, usage, errorMessage))
        return false;
    if (usage.pageCount <= 0 || usage.freePageCount <= 0)
        return false;

    return usage.freePageCount * kPercentageScale >=
           usage.pageCount * kCompactionFreePagePercent;
}

bool ThumbnailCacheMaintenance::isWithinQuota(
    const Usage &usage, const Quota &quota) const
{
    const bool entryQuotaSatisfied =
        quota.maximumEntries <= 0 ||
        usage.entryCount <= quota.maximumEntries;
    const bool storageQuotaSatisfied =
        quota.maximumStorageBytes <= 0 ||
        usage.storageBytes <= quota.maximumStorageBytes;
    return entryQuotaSatisfied && storageQuotaSatisfied;
}

qint64 ThumbnailCacheMaintenance::databaseFootprintBytes() const
{
    qint64 totalBytes = 0;
    const QStringList paths{
        mDatabasePath,
        mDatabasePath + QStringLiteral("-wal"),
        mDatabasePath + QStringLiteral("-shm"),
    };
    for (const QString &path : paths) {
        const QFileInfo info(path);
        if (info.exists())
            totalBytes = saturatedAdd(totalBytes, info.size());
    }
    return totalBytes;
}
