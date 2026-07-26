#pragma once

#include <memory>
#include <QThreadPool>
#include <QPair>
#include "components/thumbnailer/thumbnailerrunnable.h"
#include "components/cache/thumbnailcache.h"
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
    std::unique_ptr<ThumbnailCache> cache;
    std::unique_ptr<QThreadPool> pool;
    void startThumbnailerThread(QString filePath, int size, bool crop, bool force);
    QMultiMap<QString, int> runningTasks;
    QMultiMap<QString, int> queuedTasks;
    bool m_selfDestructOnFinished = false;

    // A request for (path,size) that arrives while a task for it is already
    // running is not dropped anymore, it's deferred and restarted right
    // after onTaskEnd() for the current run. force is kept as last requested,
    // but never downgraded from true to false - otherwise a "spurious"
    // duplicate request (e.g. simply re-entering the same folder) could
    // clobber a genuine force=true that came from an actual on-disk file
    // change (onFileModified).
    struct PendingRerun {
        bool crop = false;
        bool force = false;
    };
    QMap<QPair<QString, int>, PendingRerun> pendingReruns;

private slots:
    void onTaskStart(QString filePath, int size);
    void onTaskEnd(std::shared_ptr<Thumbnail> thumbnail, QString filePath);

signals:
    void thumbnailReady(std::shared_ptr<Thumbnail> thumbnail, QString filePath);
};
