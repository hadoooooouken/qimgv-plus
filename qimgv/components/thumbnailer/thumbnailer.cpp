#include "thumbnailer.h"
#include "settings.h"
#include <QDebug>
#include <utility>

Thumbnailer::Thumbnailer() {
    cache = std::make_unique<ThumbnailCache>();
    cacheWriter = std::make_unique<ThumbnailCacheWriter>(*cache);
    pool = std::make_unique<QThreadPool>();
    qRegisterMetaType<ThumbnailTaskResult>();
    int threads = settings->thumbnailerThreadCount();
    int globalThreads = QThreadPool::globalInstance()->maxThreadCount();
    if(threads > globalThreads)
        threads = globalThreads;
    pool->setMaxThreadCount(threads);

    if (settings->useThumbnailCache()) {
        cacheWriter->requestStartupMaintenance();
    }
}

Thumbnailer::~Thumbnailer() {
    clearTasks();
    pool->waitForDone();
    cacheWriter->waitForDone();

    // Member destruction stops the cache writer before destroying the cache.
}

void Thumbnailer::waitForDone() {
    pool->waitForDone();
    cacheWriter->waitForDone();
}

bool Thumbnailer::clearCache() {
    clearTasks();
    if (!pool->waitForDone()) {
        qWarning() << "Failed to stop thumbnail workers before clearing cache";
        return false;
    }

    return cacheWriter->clear();
}

void Thumbnailer::clearTasks() {
    pendingReruns.clear();

    QList<quint64> removedTaskIds;
    for (auto it = tasks.begin(); it != tasks.end(); ++it) {
        const quint64 taskId = it.key();
        TaskRecord &record = it.value();
        record.cancellationSource.request_stop();
        removeLogicalTask(record, taskId);

        if (record.phase == TaskPhase::Queued && record.runnable &&
            pool->tryTake(record.runnable.data())) {
            record.runnable->deleteLater();
            removedTaskIds.append(taskId);
        } else {
            // tryTake() can lose a race with a worker that has started but
            // whose queued taskStart signal has not reached this object yet.
            // Keep that physical task registered until taskEnd arrives.
            record.phase = TaskPhase::CancellationRequested;
        }
    }

    for (const quint64 taskId : std::as_const(removedTaskIds))
        tasks.remove(taskId);
}

std::shared_ptr<Thumbnail> Thumbnailer::getThumbnail(QString filePath, int size) {
    ThumbnailRequest request;
    request.path = std::move(filePath);
    request.size = size;
    return ThumbnailerRunnable::generate(request).thumbnail;
}

void Thumbnailer::getThumbnailAsync(QString path, int size, bool crop,
                                    bool force, int priority) {
    getThumbnailAsync(ThumbnailSource{std::move(path), std::nullopt}, size,
                      crop, force, priority);
}

void Thumbnailer::getThumbnailAsync(ThumbnailSource source, int size,
                                    bool crop, bool force, int priority) {
    const TaskKey key = qMakePair(source.path, size);
    const auto queuedTask = queuedTasks.constFind(key);
    const auto runningTask = runningTasks.constFind(key);

    // Identical non-forced consumers can share the source revision carried
    // by the queued request. A forced request must retain its newer stamp.
    if(queuedTask != queuedTasks.cend() && crop == queuedTask->crop &&
       !force)
        return;

    if(queuedTask != queuedTasks.cend() || runningTask != runningTasks.cend()) {
        const bool activeCrop = queuedTask != queuedTasks.cend()
                                    ? queuedTask->crop
                                    : runningTask->crop;

        // All current consumers subscribe to thumbnailReady, so another
        // non-forced request for the same output variant shares that result.
        if(!force && crop == activeCrop)
            return;

        auto it = pendingReruns.find(key);
        if(it != pendingReruns.end()) {
            it->force = it->force || force;
            it->crop = crop;
            it->priority = std::max(it->priority, priority);
            if (source.stamp)
                it->sourceStamp = std::move(source.stamp);
        } else {
            pendingReruns.insert(
                key, PendingRerun{crop, force, priority,
                                  std::move(source.stamp)});
        }
        return;
    }

    startThumbnailerThread(std::move(source), size, crop, force, priority);
}

void Thumbnailer::startThumbnailerThread(ThumbnailSource source, int size,
                                         bool crop, bool force, int priority) {
    const TaskKey key = qMakePair(source.path, size);
    const quint64 taskId = nextTaskId();
    std::stop_source cancellationSource;

    ThumbnailRequest request;
    request.cache = settings->useThumbnailCache() ? cache.get() : nullptr;
    request.path = std::move(source.path);
    request.sourceStamp = std::move(source.stamp);
    request.size = size;
    request.crop = crop;
    request.force = force;
    request.cacheGeneration = cacheWriter->currentGeneration();
    request.taskId = taskId;
    request.decodeContext.cancellationToken = cancellationSource.get_token();

    auto *runnable = new ThumbnailerRunnable(std::move(request), this);
    runnable->setAutoDelete(false);
    connect(runnable, &ThumbnailerRunnable::taskStart, this,
            &Thumbnailer::onTaskStart, Qt::QueuedConnection);
    connect(runnable, &ThumbnailerRunnable::taskEnd, this,
            &Thumbnailer::onTaskEnd, Qt::QueuedConnection);

    queuedTasks.insert(key, IndexedTask{crop, taskId});
    tasks.insert(taskId,
                 TaskRecord{key, crop, std::move(cancellationSource),
                            QPointer<ThumbnailerRunnable>(runnable),
                            TaskPhase::Queued});
    pool->start(runnable, priority);
}

quint64 Thumbnailer::nextTaskId()
{
    do {
        ++mNextTaskId;
    } while (tasks.contains(mNextTaskId));
    return mNextTaskId;
}

void Thumbnailer::removeLogicalTask(const TaskRecord &record, quint64 taskId)
{
    const auto queued = queuedTasks.constFind(record.key);
    if (queued != queuedTasks.cend() && queued->taskId == taskId)
        queuedTasks.remove(record.key);

    const auto running = runningTasks.constFind(record.key);
    if (running != runningTasks.cend() && running->taskId == taskId)
        runningTasks.remove(record.key);
}

void Thumbnailer::onTaskStart(quint64 taskId) {
    auto task = tasks.find(taskId);
    if (task == tasks.end() ||
        task->phase == TaskPhase::CancellationRequested)
        return;

    const auto queued = queuedTasks.constFind(task->key);
    if (queued != queuedTasks.cend() && queued->taskId == taskId)
        queuedTasks.remove(task->key);
    runningTasks.insert(task->key, IndexedTask{task->crop, taskId});
    task->phase = TaskPhase::Running;
}

void Thumbnailer::onTaskEnd(quint64 taskId, ThumbnailTaskResult result) {
    auto task = tasks.find(taskId);
    if (task == tasks.end())
        return;

    const TaskKey key = task->key;
    const QString filePath = key.first;
    const int size = key.second;
    const bool cancelled = task->cancellationSource.stop_requested();
    QPointer<ThumbnailerRunnable> runnable = task->runnable;

    removeLogicalTask(*task, taskId);

    if (cancelled) {
        if (runnable)
            runnable->deleteLater();
        tasks.erase(task);
        if (m_selfDestructOnFinished && tasks.isEmpty())
            deleteLater();
        return;
    }

    const std::shared_ptr<Thumbnail> &thumbnail = result.thumbnail;
    if (thumbnail) {
        thumbnail->pixmap();
    } else {
        qWarning() << "Thumbnail worker returned no result for" << filePath;
    }
    if (thumbnail) {
        emit thumbnailReady(thumbnail, filePath);
    } else {
        emit thumbnailFailed(filePath, size);
    }
    if (result.cacheCandidate &&
        !cacheWriter->enqueue(std::move(*result.cacheCandidate))) {
        qDebug() << "Dropped thumbnail cache write for" << filePath;
    }
    if (result.accessTouch &&
        !cacheWriter->enqueueAccessTouch(std::move(*result.accessTouch))) {
        qDebug() << "Dropped thumbnail cache access update for" << filePath;
    }
    if (runnable)
        runnable->deleteLater();
    tasks.erase(task);

    auto it = pendingReruns.find(key);
    if(it != pendingReruns.end()) {
        PendingRerun rerun = it.value();
        pendingReruns.erase(it);

        // The completed result was published above. The follow-up produces
        // the requested variant or refreshes a changed source.
        startThumbnailerThread(
            ThumbnailSource{filePath, std::move(rerun.sourceStamp)}, size,
            rerun.crop, rerun.force, rerun.priority);
        return;
    }

    if(m_selfDestructOnFinished && tasks.isEmpty()) {
        deleteLater();
    }
}

void Thumbnailer::enableSelfDestruct() {
    m_selfDestructOnFinished = true;
    if(tasks.isEmpty()) {
        deleteLater();
    }
}
