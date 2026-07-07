#include "thumbnailer.h"
#include "settings.h"

Thumbnailer::Thumbnailer() {
    cache = std::make_unique<ThumbnailCache>();
    pool = new QThreadPool(this);
    int threads = settings->thumbnailerThreadCount();
    int globalThreads = QThreadPool::globalInstance()->maxThreadCount();
    if(threads > globalThreads)
        threads = globalThreads;
    pool->setMaxThreadCount(threads);
}

Thumbnailer::~Thumbnailer() {
    pool->clear();
    pool->waitForDone();

    // Explicitly delete (rather than let the QObject parent-child mechanism
    // do it later): this joins every worker thread now. Each thread's
    // ThumbnailCache::ThreadLocalConnection is destroyed inside its own
    // owning thread as part of that thread's shutdown, before this call
    // returns and before `cache` is torn down below.
    delete pool;
    pool = nullptr;
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
