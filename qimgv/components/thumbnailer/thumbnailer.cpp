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
    // Task hasn't started yet (still queued in the pool) - actual file read
    // hasn't begun, so when it does start it will see the current mtime on
    // its own. No point duplicating the request, drop it like before.
    if(queuedTasks.contains(path, size))
        return;

    if(runningTasks.contains(path, size)) {
        auto key = qMakePair(path, size);
        auto it = pendingReruns.find(key);
        if(it != pendingReruns.end()) {
            it->force = it->force || force; // never downgrade true -> false
            it->crop = crop;
        } else {
            pendingReruns.insert(key, PendingRerun{crop, force});
        }
        return;
    }

    startThumbnailerThread(path, size, crop, force);
}

void Thumbnailer::startThumbnailerThread(QString filePath, int size, bool crop, bool force) {
    queuedTasks.insert(filePath, size);
    auto runnable = new ThumbnailerRunnable(settings->useThumbnailCache() ? cache.get() : nullptr, filePath, size, crop, force);
    connect(runnable, &ThumbnailerRunnable::taskStart, this, &Thumbnailer::onTaskStart);
    connect(runnable, &ThumbnailerRunnable::taskEnd, this, &Thumbnailer::onTaskEnd);
    runnable->setAutoDelete(true);
    pool->start(runnable);
}

void Thumbnailer::onTaskStart(QString filePath, int size) {
    runningTasks.insert(filePath, size);
    queuedTasks.remove(filePath, size);
}

void Thumbnailer::onTaskEnd(std::shared_ptr<Thumbnail> thumbnail, QString filePath) {
    if (thumbnail) {
        thumbnail->pixmap();
    }
    int size = thumbnail->size();
    runningTasks.remove(filePath, size);

    auto key = qMakePair(filePath, size);
    auto it = pendingReruns.find(key);
    if(it != pendingReruns.end()) {
        PendingRerun rerun = it.value();
        pendingReruns.erase(it);
        // rerun.force is passed through as-is (not hardcoded to true) - with
        // force=false, generate() checks mtime against the cache itself, so a
        // spurious duplicate request just returns the cached thumbnail
        // instead of triggering a real regeneration.
        startThumbnailerThread(filePath, size, rerun.crop, rerun.force);
        return;
    }

    emit thumbnailReady(thumbnail, filePath);
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
