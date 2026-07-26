#pragma once

#include <QSqlDatabase>
#include <QString>

class ThumbnailCacheMaintenance final
{
public:
    struct Quota {
        qint64 maximumStorageBytes = 0;
        qint64 maximumEntries = 0;
    };

    struct Usage {
        qint64 entryCount = 0;
        qint64 payloadBytes = 0;
        qint64 storageBytes = 0;
    };

    struct Request {
        Quota quota;
        QString protectedEntryId;
        bool scanStalePaths = false;
        bool reconcileUsage = false;
    };

    struct Result {
        Usage usage;
        QString errorMessage;
        qint64 staleEntriesRemoved = 0;
        qint64 quotaEntriesRemoved = 0;
        bool succeeded = false;
        bool withinQuota = false;
    };

    explicit ThumbnailCacheMaintenance(QString databasePath);

    [[nodiscard]] Result inspect(QSqlDatabase &db, const Quota &quota) const;
    [[nodiscard]] Result run(QSqlDatabase &db, const Request &request) const;

private:
    struct PageUsage {
        qint64 pageCount = 0;
        qint64 freePageCount = 0;
    };

    struct StaleCandidate {
        QString id;
        QString sourcePath;
    };

    struct EnforcementState {
        Quota quota;
        Usage usage;
        QString protectedEntryId;
        QString errorMessage;
        qint64 removedEntries = 0;
    };

    static constexpr qint64 kEvictionBatchSize = 256;
    static constexpr qint64 kStaleCleanupBatchSize = 200;
    static constexpr qint64 kCompactionFreePagePercent = 25;
    static constexpr qint64 kPercentageScale = 100;
    static constexpr qint64 kStorageHeadroomDivisor = 20;

    [[nodiscard]] bool reconcileUsage(QSqlDatabase &db, QString &errorMessage) const;
    [[nodiscard]] bool loadUsage(QSqlDatabase &db, Usage &usage,
                                 QString &errorMessage) const;
    [[nodiscard]] bool loadPageUsage(QSqlDatabase &db, PageUsage &usage,
                                     QString &errorMessage) const;
    [[nodiscard]] bool checkpointWal(QSqlDatabase &db, QString &errorMessage) const;
    [[nodiscard]] bool compactDatabase(QSqlDatabase &db,
                                       QString &errorMessage) const;
    [[nodiscard]] bool cleanupStalePaths(QSqlDatabase &db,
                                         const QString &protectedEntryId,
                                         qint64 &removedEntries,
                                         QString &errorMessage) const;
    [[nodiscard]] bool deleteLruBatch(QSqlDatabase &db,
                                      const QString &protectedEntryId,
                                      qint64 &removedEntries,
                                      QString &errorMessage) const;
    [[nodiscard]] bool enforceEntryQuota(QSqlDatabase &db,
                                         EnforcementState &state) const;
    [[nodiscard]] bool enforceStorageQuota(QSqlDatabase &db,
                                           EnforcementState &state) const;
    [[nodiscard]] bool shouldCompact(QSqlDatabase &db,
                                     QString &errorMessage) const;
    [[nodiscard]] bool isWithinQuota(const Usage &usage,
                                     const Quota &quota) const;
    [[nodiscard]] qint64 databaseFootprintBytes() const;

    QString mDatabasePath;
};
