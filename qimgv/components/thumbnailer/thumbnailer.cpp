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
    ThumbnailRequest request;
    request.cache = settings->useThumbnailCache() ? cache.get() : nullptr;
    request.path = filePath;
    request.size = size;
    request.crop = crop;
    request.force = force;
    request.cacheGeneration = cacheWriter->currentGeneration();
    auto runnable = new ThumbnailerRunnable(std::move(request));
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
