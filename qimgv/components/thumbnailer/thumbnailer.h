#pragma once

#include <memory>
#include <optional>
#include <stop_token>
#include <QHash>
#include <QMap>
#include <QPair>
#include <QPointer>
#include <QThreadPool>
#include "components/cache/thumbnailcache.h"
#include "components/cache/thumbnailcachewriter.h"
#include "components/thumbnailer/thumbnailerrunnable.h"

// QThreadPool::start() dequeues higher-priority runnables first.
// File thumbnails use the default (0); folder cover decodes use an
// elevated value so they are not starved by bulk file tasks that
// were submitted earlier.
constexpr int kDefaultThumbnailPriority = 0;
constexpr int kFolderCoverThumbnailPriority = 1;

struct ThumbnailSource {
    QString path;
    std::optional<ThumbnailSourceStamp> stamp;
};

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
    void getThumbnailAsync(QString path, int size, bool crop, bool force,
                           int priority = kDefaultThumbnailPriority);
    void getThumbnailAsync(ThumbnailSource source, int size, bool crop,
                           bool force,
                           int priority = kDefaultThumbnailPriority);

private:
    using TaskKey = QPair<QString, int>;

    struct IndexedTask {
        bool crop = false;
        quint64 taskId = 0;
    };

    enum class TaskPhase {
        Queued,
        Running,
        CancellationRequested,
    };

    struct TaskRecord {
        TaskKey key;
        bool crop = false;
        std::stop_source cancellationSource;
        QPointer<ThumbnailerRunnable> runnable;
        TaskPhase phase = TaskPhase::Queued;
    };

    std::unique_ptr<ThumbnailCache> cache;
    std::unique_ptr<ThumbnailCacheWriter> cacheWriter;
    std::unique_ptr<QThreadPool> pool;
    void startThumbnailerThread(ThumbnailSource source, int size, bool crop,
                                bool force, int priority);
    [[nodiscard]] quint64 nextTaskId();
    void removeLogicalTask(const TaskRecord &record, quint64 taskId);
    QMap<TaskKey, IndexedTask> runningTasks;
    QMap<TaskKey, IndexedTask> queuedTasks;
    QHash<quint64, TaskRecord> tasks;
    quint64 mNextTaskId = 0;
    bool m_selfDestructOnFinished = false;

    // Repeated non-forced requests share the active task's result via
    // thumbnailReady. Only a different output variant or explicit source
    // refresh is deferred.
    struct PendingRerun {
        bool crop = false;
        bool force = false;
        int priority = kDefaultThumbnailPriority;
        std::optional<ThumbnailSourceStamp> sourceStamp;
    };
    QMap<TaskKey, PendingRerun> pendingReruns;

private slots:
    void onTaskStart(quint64 taskId);
    void onTaskEnd(quint64 taskId, ThumbnailTaskResult result);

signals:
    void thumbnailReady(std::shared_ptr<Thumbnail> thumbnail, QString filePath);
    void thumbnailFailed(QString filePath, int size);
};
