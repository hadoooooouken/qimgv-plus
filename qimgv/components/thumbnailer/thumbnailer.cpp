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
    pool->clear();
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
    pool->clear();
    queuedTasks.clear();
    pendingReruns.clear();
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
    if(queuedTask != queuedTasks.cend() && crop == queuedTask.value() &&
       !force)
        return;

    if(queuedTask != queuedTasks.cend() || runningTask != runningTasks.cend()) {
        const bool activeCrop = queuedTask != queuedTasks.cend()
                                    ? queuedTask.value()
                                    : runningTask.value();

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
    queuedTasks.insert(qMakePair(source.path, size), crop);
    ThumbnailRequest request;
    request.cache = settings->useThumbnailCache() ? cache.get() : nullptr;
    request.path = std::move(source.path);
    request.sourceStamp = std::move(source.stamp);
    request.size = size;
    request.crop = crop;
    request.force = force;
    request.cacheGeneration = cacheWriter->currentGeneration();
    auto runnable = new ThumbnailerRunnable(std::move(request));
    connect(runnable, &ThumbnailerRunnable::taskStart, this, &Thumbnailer::onTaskStart);
    connect(runnable, &ThumbnailerRunnable::taskEnd, this, &Thumbnailer::onTaskEnd);
    runnable->setAutoDelete(true);
    pool->start(runnable, priority);
}

void Thumbnailer::onTaskStart(QString filePath, int size, bool crop) {
    const TaskKey key = qMakePair(filePath, size);
    runningTasks.insert(key, crop);
    queuedTasks.remove(key);
}

void Thumbnailer::onTaskEnd(ThumbnailTaskResult result, QString filePath,
                            int size) {
    const std::shared_ptr<Thumbnail> &thumbnail = result.thumbnail;
    if (thumbnail) {
        thumbnail->pixmap();
    } else {
        qWarning() << "Thumbnail worker returned no result for" << filePath;
    }
    const TaskKey key = qMakePair(filePath, size);
    if (thumbnail) {
        emit thumbnailReady(thumbnail, filePath);
    }
    if (result.cacheCandidate &&
        !cacheWriter->enqueue(std::move(*result.cacheCandidate))) {
        qDebug() << "Dropped thumbnail cache write for" << filePath;
    }
    if (result.accessTouch &&
        !cacheWriter->enqueueAccessTouch(std::move(*result.accessTouch))) {
        qDebug() << "Dropped thumbnail cache access update for" << filePath;
    }
    runningTasks.remove(key);

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

    if(m_selfDestructOnFinished && runningTasks.isEmpty()) {
        deleteLater();
    }
}

void Thumbnailer::enableSelfDestruct() {
    m_selfDestructOnFinished = true;
    if(runningTasks.isEmpty()) {
        deleteLater();
    }
}
