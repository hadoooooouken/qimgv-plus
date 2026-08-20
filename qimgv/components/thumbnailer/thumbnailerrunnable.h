#pragma once

#include "components/cache/thumbnailcachewriter.h"
#include "sourcecontainers/thumbnail.h"

#include <QCryptographicHash>
#include <QMetaType>
#include <QObject>
#include <QRunnable>
#include <memory>
#include <optional>
#include <utility>

#include "utils/decodecontext.h"

struct ThumbnailRequest {
    ThumbnailCache *cache = nullptr;
    QString path;
    std::optional<ThumbnailSourceStamp> sourceStamp;
    int size = 0;
    bool crop = false;
    bool force = false;
    quint64 cacheGeneration = 0;
    quint64 taskId = 0;
    DecodeContext decodeContext;
};

struct ThumbnailTaskResult {
    std::shared_ptr<Thumbnail> thumbnail;
    std::optional<ThumbnailCacheCandidate> cacheCandidate;
    std::optional<ThumbnailCache::AccessTouch> accessTouch;
};

Q_DECLARE_METATYPE(ThumbnailTaskResult)

enum class ThumbnailTaskCompletionStatus {
    RemovedBeforeRun,
    Finished,
};

struct ThumbnailTaskCompletion {
    quint64 taskId = 0;
    ThumbnailTaskCompletionStatus status =
        ThumbnailTaskCompletionStatus::RemovedBeforeRun;
    ThumbnailTaskResult result;
};

Q_DECLARE_METATYPE(ThumbnailTaskCompletion)

class ThumbnailTaskNotifier final : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    // May be called by pool threads; receivers must use queued connections.
    void reportCompletion(ThumbnailTaskCompletion completion);

signals:
    void taskCompleted(ThumbnailTaskCompletion completion);
};

class ThumbnailerRunnable final : public QRunnable {
public:
    ThumbnailerRunnable(ThumbnailRequest request,
                        ThumbnailTaskNotifier &notifier);
    ~ThumbnailerRunnable() override;
    void run() override;
    [[nodiscard]] static ThumbnailTaskResult
    generate(const ThumbnailRequest &request);

private:
    static QString generateIdString(QString path, int size, bool crop);
    static std::pair<QImage, QSize>
    createThumbnail(QString path, const char *format, int size, bool crop,
                    const DecodeContext &context);
    // Same as QSize::scaled(size, size, mode), but never enlarges an image
    // that is already smaller than the target box - avoids blurry upscaled
    // thumbnails for tiny source images (icons, small screenshots, etc).
    static QSize noUpscaleScaledSize(QSize originalSize, int size, Qt::AspectRatioMode mode);
    ThumbnailRequest request;
    ThumbnailTaskNotifier &notifier;
    ThumbnailTaskCompletion completion;
};
