#pragma once

#include "settings_types.h"

#include <QHash>
#include <QList>
#include <QMetaType>
#include <QObject>
#include <QPair>
#include <QSet>
#include <QString>
#include <QThreadPool>
#include <QtGlobal>

#include <memory>

struct FolderCoverRequest {
    QString folderPath;
    SortingMode sortingMode = SORT_TIME_DESC;
    int thumbnailSize = 0;
    quint64 generation = 0;
};

enum class FolderCoverStatus {
    CoverFound,
    NoCover,
    ReadError
};

struct FolderCoverResult {
    FolderCoverRequest request;
    FolderCoverStatus status = FolderCoverStatus::NoCover;
    QString coverPath;
    QString diagnostic;
    bool fromCache = false;
};

Q_DECLARE_METATYPE(FolderCoverResult)

class FolderCoverResolver final : public QObject {
    Q_OBJECT

public:
    explicit FolderCoverResolver(QObject *parent = nullptr);
    ~FolderCoverResolver() override;

    void resolve(FolderCoverRequest request);
    void invalidate(const QString &folderPath);

signals:
    void resultReady(FolderCoverResult result);

private slots:
    void onSearchFinished(FolderCoverResult result, quint64 folderRevision);

private:
    using CacheKey = QPair<QString, int>;

    struct PendingResolve {
        quint64 folderRevision = 0;
        QList<FolderCoverRequest> requests;
    };

    static CacheKey cacheKey(const QString &folderPath, SortingMode sortingMode);
    void emitQueued(FolderCoverResult result);

    QThreadPool threadPool;
    std::shared_ptr<const QSet<QString>> supportedExtensions;
    QHash<CacheKey, QString> coverCache;
    QHash<QString, quint64> folderRevisions;
    QHash<CacheKey, PendingResolve> pendingRequests;
};
