#pragma once

#include "components/cache/thumbnailcachewriter.h"
#include "sourcecontainers/thumbnail.h"

#include <QCryptographicHash>
#include <QMetaType>
#include <QRunnable>
#include <memory>
#include <optional>
#include <utility>

struct ThumbnailRequest {
    ThumbnailCache *cache = nullptr;
    QString path;
    std::optional<ThumbnailSourceStamp> sourceStamp;
    int size = 0;
    bool crop = false;
    bool force = false;
    quint64 cacheGeneration = 0;
};

struct ThumbnailTaskResult {
    std::shared_ptr<Thumbnail> thumbnail;
    std::optional<ThumbnailCacheCandidate> cacheCandidate;
    std::optional<ThumbnailCache::AccessTouch> accessTouch;
};

Q_DECLARE_METATYPE(ThumbnailTaskResult)

class ThumbnailerRunnable : public QObject, public QRunnable {
    Q_OBJECT
public:
    explicit ThumbnailerRunnable(ThumbnailRequest request);
    ~ThumbnailerRunnable() override = default;
    void run();
    [[nodiscard]] static ThumbnailTaskResult
    generate(const ThumbnailRequest &request);

private:
    static QString generateIdString(QString path, int size, bool crop);
    static std::pair<QImage, QSize> createThumbnail(QString path, const char* format, int size, bool crop);
    // Same as QSize::scaled(size, size, mode), but never enlarges an image
    // that is already smaller than the target box - avoids blurry upscaled
    // thumbnails for tiny source images (icons, small screenshots, etc).
    static QSize noUpscaleScaledSize(QSize originalSize, int size, Qt::AspectRatioMode mode);
    ThumbnailRequest request;

signals:
    void taskStart(QString, int, bool);
    void taskEnd(ThumbnailTaskResult, QString, int);
};
