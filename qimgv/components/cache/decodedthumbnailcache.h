#pragma once

#include "thumbnailsourcestamp.h"

#include <QHash>
#include <QImage>
#include <QString>
#include <list>
#include <memory>
#include <mutex>
#include <optional>

class DecodedThumbnailCache final
{
public:
    struct LookupResult {
        std::unique_ptr<QImage> image;
        bool requiresLinearColorSpace = false;
        bool accessTouchRequired = false;
        // std::nullopt means "unknown" (entry predates tone-map tracking,
        // or the cache missed) - callers should treat that conservatively,
        // i.e. the same as true. See ThumbnailCache::ReadResult.
        std::optional<bool> toneMapDependent;
        bool toneMapEnabled = false;
        int toneMapOperator = 0;
        int toneMapWhiteLevel = 0;
    };

    explicit DecodedThumbnailCache(qint64 maximumBytes);

    [[nodiscard]] LookupResult
    lookup(const QString &id, const ThumbnailSourceStamp &sourceStamp,
           qint64 accessedAt, qint64 accessTouchInterval);
    void insert(const QString &id, const ThumbnailSourceStamp &sourceStamp,
                const QImage &image, bool requiresLinearColorSpace,
                qint64 lastAccessed, std::optional<bool> toneMapDependent,
                bool toneMapEnabled, int toneMapOperator,
                int toneMapWhiteLevel);
    void clear();

private:
    struct CachedThumbnail {
        QImage image;
        ThumbnailSourceStamp sourceStamp;
        std::list<QString>::iterator recencyPosition;
        qint64 byteCost = 0;
        qint64 lastAccessed = 0;
        bool requiresLinearColorSpace = false;
        std::optional<bool> toneMapDependent;
        bool toneMapEnabled = false;
        int toneMapOperator = 0;
        int toneMapWhiteLevel = 0;
    };

    [[nodiscard]] static bool
    stampsMatch(const ThumbnailSourceStamp &left,
                const ThumbnailSourceStamp &right);
    void removeEntry(QHash<QString, CachedThumbnail>::iterator entry);
    void evictToFit(qint64 incomingByteCost);

    const qint64 maximumBytes;
    qint64 currentBytes = 0;
    std::list<QString> recency;
    QHash<QString, CachedThumbnail> entries;
    std::mutex mutex;
};
