#pragma once

#include "thumbnailsourcestamp.h"

#include <QHash>
#include <QImage>
#include <QString>
#include <list>
#include <memory>
#include <mutex>

class DecodedThumbnailCache final
{
public:
    struct LookupResult {
        std::unique_ptr<QImage> image;
        bool requiresLinearColorSpace = false;
        bool accessTouchRequired = false;
    };

    explicit DecodedThumbnailCache(qint64 maximumBytes);

    [[nodiscard]] LookupResult
    lookup(const QString &id, const ThumbnailSourceStamp &sourceStamp,
           qint64 accessedAt, qint64 accessTouchInterval);
    void insert(const QString &id, const ThumbnailSourceStamp &sourceStamp,
                const QImage &image, bool requiresLinearColorSpace,
                qint64 lastAccessed);
    void clear();

private:
    struct CachedThumbnail {
        QImage image;
        ThumbnailSourceStamp sourceStamp;
        std::list<QString>::iterator recencyPosition;
        qint64 byteCost = 0;
        qint64 lastAccessed = 0;
        bool requiresLinearColorSpace = false;
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
