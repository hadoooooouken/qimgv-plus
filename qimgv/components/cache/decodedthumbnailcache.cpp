#include "decodedthumbnailcache.h"

#include <algorithm>

DecodedThumbnailCache::DecodedThumbnailCache(qint64 maximumBytes)
    : maximumBytes(std::max<qint64>(0, maximumBytes))
{
}

DecodedThumbnailCache::LookupResult DecodedThumbnailCache::lookup(
    const QString &id, const ThumbnailSourceStamp &sourceStamp,
    qint64 accessedAt, qint64 accessTouchInterval)
{
    if (id.isEmpty() || sourceStamp.normalizedPath.isEmpty() ||
        sourceStamp.size < 0 || accessedAt <= 0 || accessTouchInterval <= 0) {
        return {};
    }

    std::lock_guard lock(mutex);
    auto entry = entries.find(id);
    if (entry == entries.end())
        return {};

    if (!stampsMatch(entry->sourceStamp, sourceStamp)) {
        removeEntry(entry);
        return {};
    }

    recency.splice(recency.begin(), recency, entry->recencyPosition);
    entry->recencyPosition = recency.begin();

    const bool accessTouchRequired =
        entry->lastAccessed < accessedAt - accessTouchInterval;
    if (accessTouchRequired)
        entry->lastAccessed = accessedAt;

    return {
        std::make_unique<QImage>(entry->image),
        entry->requiresLinearColorSpace,
        accessTouchRequired};
}

void DecodedThumbnailCache::insert(
    const QString &id, const ThumbnailSourceStamp &sourceStamp,
    const QImage &image, bool requiresLinearColorSpace, qint64 lastAccessed)
{
    if (id.isEmpty() || sourceStamp.normalizedPath.isEmpty() ||
        sourceStamp.size < 0 || image.isNull() || lastAccessed < 0) {
        return;
    }

    const qsizetype imageByteCost = image.sizeInBytes();
    if (imageByteCost <= 0) {
        return;
    }
    const qint64 byteCost = static_cast<qint64>(imageByteCost);

    std::lock_guard lock(mutex);
    auto existingEntry = entries.find(id);
    if (existingEntry != entries.end())
        removeEntry(existingEntry);

    if (byteCost > maximumBytes)
        return;

    evictToFit(byteCost);
    recency.push_front(id);
    entries.insert(
        id,
        CachedThumbnail{
            image,
            sourceStamp,
            recency.begin(),
            byteCost,
            lastAccessed,
            requiresLinearColorSpace});
    currentBytes += byteCost;
}

void DecodedThumbnailCache::clear()
{
    std::lock_guard lock(mutex);
    entries.clear();
    recency.clear();
    currentBytes = 0;
}

bool DecodedThumbnailCache::stampsMatch(
    const ThumbnailSourceStamp &left,
    const ThumbnailSourceStamp &right)
{
    return left.normalizedPath == right.normalizedPath &&
           left.modifiedTimeTicks == right.modifiedTimeTicks &&
           left.size == right.size;
}

void DecodedThumbnailCache::removeEntry(
    QHash<QString, CachedThumbnail>::iterator entry)
{
    currentBytes = std::max<qint64>(0, currentBytes - entry->byteCost);
    recency.erase(entry->recencyPosition);
    entries.erase(entry);
}

void DecodedThumbnailCache::evictToFit(qint64 incomingByteCost)
{
    while (!recency.empty() &&
           currentBytes > maximumBytes - incomingByteCost) {
        auto entry = entries.find(recency.back());
        if (entry == entries.end()) {
            recency.pop_back();
            continue;
        }
        removeEntry(entry);
    }
}
