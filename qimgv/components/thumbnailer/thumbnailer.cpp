#include "thumbnailer.h"
#include "settings.h"

Thumbnailer::Thumbnailer() {
    cache = std::make_unique<ThumbnailCache>();
    pool = std::make_unique<QThreadPool>();
    int threads = settings->thumbnailerThreadCount();
    int globalThreads = QThreadPool::globalInstance()->maxThreadCount();
    if(threads > globalThreads)
        threads = globalThreads;
    pool->setMaxThreadCount(threads);
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

void Thumbnailer::clearTasks() {
    pool->clear();
    queuedTasks.clear();
}

std::shared_ptr<Thumbnail> Thumbnailer::getThumbnail(QString filePath, int size) {
    return ThumbnailerRunnable::generate(nullptr, filePath, size, false, false);
}

void Thumbnailer::getThumbnailAsync(QString path, int size, bool crop, bool force) {
    if(!runningTasks.contains(path, size) && !queuedTasks.contains(path, size))
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
    runningTasks.remove(filePath, thumbnail->size());
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
