#include "thumbnailcachewriter.h"

#include <QBuffer>
#include <QDebug>
#include <algorithm>
#include <chrono>
#include <utility>

ThumbnailCacheWriter::ThumbnailCacheWriter(ThumbnailCache &cache,
                                           QObject *parent)
    : QObject(parent),
      cache(cache),
      workerThread(QThread::create([this]() { processQueue(); }))
{
    workerThread->setObjectName(QStringLiteral("ThumbnailCacheWriter"));
    workerThread->start(QThread::LowPriority);
}

ThumbnailCacheWriter::~ThumbnailCacheWriter()
{
    stop();
}

quint64 ThumbnailCacheWriter::currentGeneration() const noexcept
{
    return generation.load(std::memory_order_acquire);
}

bool ThumbnailCacheWriter::enqueue(ThumbnailCacheCandidate candidate)
{
    if (candidate.image.isNull() || candidate.id.isEmpty()) {
        qWarning() << "Cannot enqueue an invalid thumbnail cache candidate";
        return false;
    }

    std::lock_guard lock(queueMutex);
    if (stopping)
        return false;
    if (candidate.generation !=
        generation.load(std::memory_order_relaxed)) {
        return false;
    }
    if (queue.size() >= kMaximumQueuedCandidates)
        return false;

    queue.push_back(std::move(candidate));
    workAvailable.notify_one();
    return true;
}

void ThumbnailCacheWriter::requestStartupMaintenance()
{
    std::lock_guard lock(queueMutex);
    if (stopping)
        return;

    startupMaintenanceRequested = true;
    workAvailable.notify_one();
}

void ThumbnailCacheWriter::waitForDone()
{
    std::unique_lock lock(queueMutex);
    stateChanged.wait(lock, [this]() {
        return queue.empty() && !processing && !clearRequested &&
               !startupMaintenanceRequested;
    });
}

bool ThumbnailCacheWriter::clear()
{
    std::unique_lock lock(queueMutex);
    if (stopping) {
        qWarning() << "Cannot clear a stopped thumbnail cache writer";
        return false;
    }

    generation.fetch_add(1, std::memory_order_acq_rel);
    queue.clear();
    startupMaintenanceRequested = false;
    clearRequested = true;
    workAvailable.notify_one();

    stateChanged.wait(lock, [this]() { return !clearRequested; });
    return clearResult;
}

void ThumbnailCacheWriter::processQueue()
{
    for (;;) {
        std::unique_lock lock(queueMutex);
        workAvailable.wait(lock, [this]() {
            return stopping || clearRequested ||
                   startupMaintenanceRequested || !queue.empty();
        });

        if (clearRequested) {
            processing = true;
            lock.unlock();
            const bool succeeded = cache.clear();
            lock.lock();

            clearResult = succeeded;
            clearRequested = false;
            processing = false;
            stateChanged.notify_all();
            continue;
        }

        if (startupMaintenanceRequested && !stopping) {
            startupMaintenanceRequested = false;
            processing = true;
            lock.unlock();
            const bool succeeded = cache.performStartupMaintenance();
            if (!succeeded)
                qWarning() << "Thumbnail cache startup maintenance failed";
            lock.lock();

            processing = false;
            stateChanged.notify_all();
            continue;
        }

        if (queue.empty()) {
            if (stopping)
                break;
            continue;
        }

        if (!stopping && queue.size() < kMaximumBatchSize) {
            workAvailable.wait_for(
                lock,
                std::chrono::milliseconds(
                    kBatchCollectionDelayMilliseconds),
                [this]() {
                    return stopping || clearRequested ||
                           queue.size() >= kMaximumBatchSize;
                });
            if (clearRequested)
                continue;
        }

        const std::size_t batchSize =
            std::min(queue.size(), kMaximumBatchSize);
        std::deque<ThumbnailCacheCandidate> batch;
        for (std::size_t index = 0; index < batchSize; ++index) {
            batch.push_back(std::move(queue.front()));
            queue.pop_front();
        }
        processing = true;
        lock.unlock();

        processBatch(std::move(batch));

        lock.lock();
        processing = false;
        stateChanged.notify_all();
    }

    processing = false;
    stateChanged.notify_all();
}

void ThumbnailCacheWriter::processBatch(
    std::deque<ThumbnailCacheCandidate> batch)
{
    QList<ThumbnailCache::WriteEntry> entries;
    entries.reserve(static_cast<qsizetype>(batch.size()));

    for (ThumbnailCacheCandidate &candidate : batch) {
        QByteArray encodedThumbnail;
        QBuffer buffer(&encodedThumbnail);
        if (!buffer.open(QIODevice::WriteOnly)) {
            qWarning() << "Failed to open thumbnail encoding buffer:"
                       << buffer.errorString();
            continue;
        }

        QImage thumbnailCopy = candidate.image;
        thumbnailCopy.setText(
            QStringLiteral("effort"),
            QString::number(kThumbnailEncodingEffort));
        const bool encoded =
            thumbnailCopy.save(&buffer, kThumbnailEncodingFormat,
                               kThumbnailEncodingQuality);
        buffer.close();
        if (!encoded || encodedThumbnail.isEmpty()) {
            qWarning() << "Failed to encode thumbnail as JXL for"
                       << candidate.sourcePath;
            continue;
        }

        ThumbnailCache::WriteEntry entry;
        entry.id = std::move(candidate.id);
        entry.lastModified =
            candidate.image.text(QStringLiteral("lastModified"));
        entry.originalWidth =
            candidate.image.text(QStringLiteral("originalWidth")).toInt();
        entry.originalHeight =
            candidate.image.text(QStringLiteral("originalHeight")).toInt();
        entry.label = candidate.image.text(QStringLiteral("label"));
        entry.encodedData = std::move(encodedThumbnail);
        entry.sourcePath = std::move(candidate.sourcePath);
        entries.push_back(std::move(entry));
    }

    if (!entries.isEmpty() && !cache.saveThumbnails(entries)) {
        qWarning() << "Thumbnail cache batch write failed for"
                   << entries.size() << "entries";
    }
}

void ThumbnailCacheWriter::stop()
{
    {
        std::lock_guard lock(queueMutex);
        if (stopping)
            return;
        stopping = true;
        startupMaintenanceRequested = false;
        workAvailable.notify_one();
    }

    if (workerThread && !workerThread->wait()) {
        qWarning() << "Thumbnail cache writer thread did not stop cleanly";
    }
}
