#pragma once

#include <memory>
#include <QMap>
#include <QPair>
#include <QThreadPool>
#include "components/cache/thumbnailcache.h"
#include "components/cache/thumbnailcachewriter.h"
#include "components/thumbnailer/thumbnailerrunnable.h"

class Thumbnailer : public QObject
{
    Q_OBJECT
public:
    explicit Thumbnailer();
    ~Thumbnailer();
    static std::shared_ptr<Thumbnail> getThumbnail(QString filePath, int size);
    [[nodiscard]] bool clearCache();
    void clearTasks();
    void waitForDone();
    void enableSelfDestruct();

public slots:
    void getThumbnailAsync(QString path, int size, bool crop, bool force);

private:
    using TaskKey = QPair<QString, int>;

    std::unique_ptr<ThumbnailCache> cache;
    std::unique_ptr<ThumbnailCacheWriter> cacheWriter;
    std::unique_ptr<QThreadPool> pool;
    void startThumbnailerThread(QString filePath, int size, bool crop, bool force);
    QMap<TaskKey, bool> runningTasks;
    QMap<TaskKey, bool> queuedTasks;
    bool m_selfDestructOnFinished = false;

    // Repeated non-forced requests share the active task's result via
    // thumbnailReady. Only a different output variant or explicit source
    // refresh is deferred.
    struct PendingRerun {
        bool crop = false;
        bool force = false;
    };
    QMap<TaskKey, PendingRerun> pendingReruns;

private slots:
    void onTaskStart(QString filePath, int size, bool crop);
    void onTaskEnd(ThumbnailTaskResult result, QString filePath, int size);

signals:
    void thumbnailReady(std::shared_ptr<Thumbnail> thumbnail, QString filePath);
};
