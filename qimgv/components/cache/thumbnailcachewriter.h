#pragma once

#include "thumbnailcache.h"

#include <QHash>
#include <QImage>
#include <QObject>
#include <QThread>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <memory>
#include <mutex>

struct ThumbnailCacheCandidate {
    QImage image;
    QString id;
    ThumbnailSourceStamp sourceStamp;
    bool requiresLinearColorSpace = false;
    quint64 generation = 0;
};

class ThumbnailCacheWriter final : public QObject
{
    Q_OBJECT
public:
    explicit ThumbnailCacheWriter(ThumbnailCache &cache,
                                  QObject *parent = nullptr);
    ~ThumbnailCacheWriter() override;

    ThumbnailCacheWriter(const ThumbnailCacheWriter &) = delete;
    ThumbnailCacheWriter &operator=(const ThumbnailCacheWriter &) = delete;

    [[nodiscard]] quint64 currentGeneration() const noexcept;
    [[nodiscard]] bool enqueue(ThumbnailCacheCandidate candidate);
    [[nodiscard]] bool
    enqueueAccessTouch(ThumbnailCache::AccessTouch accessTouch);
    void requestStartupMaintenance();
    void waitForDone();
    [[nodiscard]] bool clear();

private:
    static constexpr std::size_t kMaximumQueuedCandidates = 64;
    static constexpr std::size_t kMaximumBatchSize = 8;
    static constexpr qsizetype kMaximumPendingAccessTouches = 1024;
    static constexpr qsizetype kMaximumAccessTouchBatchSize = 256;
    static constexpr int kBatchCollectionDelayMilliseconds = 8;
    static constexpr int kThumbnailEncodingEffort = 2;
    static constexpr int kThumbnailEncodingQuality = 85;
    static constexpr char kThumbnailEncodingFormat[] = "JXL";

    void processQueue();
    void processBatch(std::deque<ThumbnailCacheCandidate> batch);
    void processAccessTouches(
        QList<ThumbnailCache::AccessTouch> accessTouches);
    void stop();

    ThumbnailCache &cache;
    std::unique_ptr<QThread> workerThread;
    mutable std::mutex queueMutex;
    std::condition_variable workAvailable;
    std::condition_variable stateChanged;
    std::deque<ThumbnailCacheCandidate> queue;
    QHash<QString, ThumbnailCache::AccessTouch> pendingAccessTouches;
    std::atomic<quint64> generation{0};
    bool startupMaintenanceRequested = false;
    bool clearRequested = false;
    bool clearResult = false;
    bool processing = false;
    bool stopping = false;
};
