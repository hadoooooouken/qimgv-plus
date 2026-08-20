#include "thumbnailer.h"
#include "settings.h"
#include <QDebug>
#include <utility>

Thumbnailer::Thumbnailer() {
    cache = std::make_unique<ThumbnailCache>();
    cacheWriter = std::make_unique<ThumbnailCacheWriter>(*cache);
    pool = std::make_unique<QThreadPool>();
    qRegisterMetaType<ThumbnailTaskResult>();
    qRegisterMetaType<ThumbnailTaskCompletion>();
    connect(&taskNotifier, &ThumbnailTaskNotifier::taskCompleted, this,
            &Thumbnailer::onTaskCompleted, Qt::QueuedConnection);
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

    for (TaskRecord &record : tasks)
        record.cancellationRequested = true;
    activeTasks.clear();

    mCancellationSource.request_stop();
    pool->clear();
    mCancellationSource = std::stop_source{};
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
    if (m_selfDestructOnFinished) {
        qWarning() << "Ignoring thumbnail request after self-destruct was enabled"
                   << source.path;
        return;
    }

    const TaskKey key = qMakePair(source.path, size);
    const auto activeTask = activeTasks.constFind(key);

    // Identical non-forced consumers can share the source revision carried
    // by the active request. A forced request must retain its newer stamp.
    if(activeTask != activeTasks.cend() && crop == activeTask->crop && !force)
        return;

    if(activeTask != activeTasks.cend()) {
        // All current consumers subscribe to thumbnailReady, so another
        // non-forced request for the same output variant shares that result.
        if(!force && crop == activeTask->crop)
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

    ThumbnailRequest request;
    request.cache = settings->useThumbnailCache() ? cache.get() : nullptr;
    request.path = std::move(source.path);
    request.sourceStamp = std::move(source.stamp);
    request.size = size;
    request.crop = crop;
    request.force = force;
    request.cacheGeneration = cacheWriter->currentGeneration();
    request.taskId = taskId;
    request.decodeContext.cancellationToken =
        mCancellationSource.get_token();

    auto runnable = std::make_unique<ThumbnailerRunnable>(
        std::move(request), taskNotifier);

    activeTasks.insert(key, IndexedTask{crop, taskId});
    tasks.insert(taskId, TaskRecord{key});
    pool->start(runnable.release(), priority);
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
    const auto active = activeTasks.constFind(record.key);
    if (active != activeTasks.cend() && active->taskId == taskId)
        activeTasks.remove(record.key);
}

void Thumbnailer::onTaskCompleted(ThumbnailTaskCompletion completion) {
    auto task = tasks.find(completion.taskId);
    if (task == tasks.end())
        return;

    const TaskKey key = task->key;
    const QString filePath = key.first;
    const int size = key.second;
    const bool cancelled = task->cancellationRequested;

    removeLogicalTask(*task, completion.taskId);
    tasks.erase(task);

    if (cancelled) {
        scheduleSelfDestructIfIdle();
        return;
    }

    if (completion.status != ThumbnailTaskCompletionStatus::Finished) {
        qWarning() << "Thumbnail task was removed before execution"
                   << filePath;
        emit thumbnailFailed(filePath, size);
        scheduleSelfDestructIfIdle();
        return;
    }

    auto rerunIt = pendingReruns.find(key);
    if(rerunIt != pendingReruns.end()) {
        PendingRerun rerun = rerunIt.value();
        pendingReruns.erase(rerunIt);

        // Register the follow-up before publishing the completed result so a
        // synchronous receiver cannot start a duplicate task for this key.
        startThumbnailerThread(
            ThumbnailSource{filePath, std::move(rerun.sourceStamp)}, size,
            rerun.crop, rerun.force, rerun.priority);
    }

    ThumbnailTaskResult result = std::move(completion.result);
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

    scheduleSelfDestructIfIdle();
}

void Thumbnailer::scheduleSelfDestructIfIdle() {
    if(m_selfDestructOnFinished && tasks.isEmpty() &&
       pendingReruns.isEmpty()) {
        deleteLater();
    }
}

void Thumbnailer::enableSelfDestruct() {
    m_selfDestructOnFinished = true;
    scheduleSelfDestructIfIdle();
}
