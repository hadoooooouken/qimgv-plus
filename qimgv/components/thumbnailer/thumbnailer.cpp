#include "thumbnailer.h"
#include "settings.h"
#include <QDebug>

Thumbnailer::Thumbnailer() {
    cache = std::make_unique<ThumbnailCache>();
    pool = std::make_unique<QThreadPool>();
    int threads = settings->thumbnailerThreadCount();
    int globalThreads = QThreadPool::globalInstance()->maxThreadCount();
    if(threads > globalThreads)
        threads = globalThreads;
    pool->setMaxThreadCount(threads);

    if (settings->useThumbnailCache()) {
        ThumbnailCache *cacheForMaintenance = cache.get();
        pool->start([cacheForMaintenance]() {
            if (!cacheForMaintenance->performStartupMaintenance()) {
                qWarning() << "Thumbnail cache startup maintenance failed";
            }
        });
    }
}

Thumbnailer::~Thumbnailer() {
    pool->clear();
    pool->waitForDone();

    // No manual delete: `pool` is std::unique_ptr<QThreadPool>, declared
    // after `cache` in the header, so ordinary C++ member destruction
    // (reverse of declaration order) destroys it here, joining every
    // worker thread and running each ThreadLocalConnection's destructor
    // on its owning thread, before `cache` (and its QThreadStorage) is
    // destroyed below.
}

void Thumbnailer::waitForDone() {
    pool->waitForDone();
}

bool Thumbnailer::clearCache() {
    clearTasks();
    if (!pool->waitForDone()) {
        qWarning() << "Failed to stop thumbnail workers before clearing cache";
        return false;
    }

    bool cacheCleared = false;
    ThumbnailCache *cacheForClear = cache.get();
    // ThumbnailCache owns one SQL connection per calling thread. Run the
    // reset on an owned worker and make that thread exit before returning so
    // QThreadStorage closes and removes the connection on the same thread.
    pool->start([cacheForClear, &cacheCleared]() {
        cacheCleared = cacheForClear->clear();
    });

    if (!pool->waitForDone()) {
        qWarning() << "Failed to stop thumbnail cache clear worker";
        return false;
    }
    return cacheCleared;
}

void Thumbnailer::clearTasks() {
    pool->clear();
    queuedTasks.clear();
    pendingReruns.clear();
}

std::shared_ptr<Thumbnail> Thumbnailer::getThumbnail(QString filePath, int size) {
    return ThumbnailerRunnable::generate(nullptr, filePath, size, false, false);
}

void Thumbnailer::getThumbnailAsync(QString path, int size, bool crop, bool force) {
    const TaskKey key = qMakePair(path, size);
    const auto queuedTask = queuedTasks.constFind(key);
    const auto runningTask = runningTasks.constFind(key);

    // A queued task has not sampled the source yet. It will observe the
    // current revision when it starts, so an identical request needs no
    // follow-up even when it is forced.
    if(queuedTask != queuedTasks.cend() && crop == queuedTask.value())
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
        } else {
            pendingReruns.insert(key, PendingRerun{crop, force});
        }
        return;
    }

    startThumbnailerThread(path, size, crop, force);
}

void Thumbnailer::startThumbnailerThread(QString filePath, int size, bool crop, bool force) {
    queuedTasks.insert(qMakePair(filePath, size), crop);
    auto runnable = new ThumbnailerRunnable(settings->useThumbnailCache() ? cache.get() : nullptr, filePath, size, crop, force);
    connect(runnable, &ThumbnailerRunnable::taskStart, this, &Thumbnailer::onTaskStart);
    connect(runnable, &ThumbnailerRunnable::taskEnd, this, &Thumbnailer::onTaskEnd);
    runnable->setAutoDelete(true);
    pool->start(runnable);
}

void Thumbnailer::onTaskStart(QString filePath, int size, bool crop) {
    const TaskKey key = qMakePair(filePath, size);
    runningTasks.insert(key, crop);
    queuedTasks.remove(key);
}

void Thumbnailer::onTaskEnd(std::shared_ptr<Thumbnail> thumbnail, QString filePath, int size) {
    if (thumbnail) {
        thumbnail->pixmap();
    } else {
        qWarning() << "Thumbnail worker returned no result for" << filePath;
    }
    const TaskKey key = qMakePair(filePath, size);
    if (thumbnail) {
        emit thumbnailReady(thumbnail, filePath);
    }
    runningTasks.remove(key);

    auto it = pendingReruns.find(key);
    if(it != pendingReruns.end()) {
        PendingRerun rerun = it.value();
        pendingReruns.erase(it);

        // The completed result was published above. The follow-up produces
        // the requested variant or refreshes a changed source.
        startThumbnailerThread(filePath, size, rerun.crop, rerun.force);
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
