#pragma once

#include <QMap>
#include <QMutex>
#include <QWaitCondition>
#include <functional>
#include <memory>
#include "sourcecontainers/image.h"
#include "components/cache/cacheitem.h"

class Cache {
public:
    explicit Cache();
    bool contains(QString path) const;
    void remove(QString path);
    void clear();

    bool insert(std::shared_ptr<Image> img);
    void trimTo(QStringList list);

    std::shared_ptr<Image> get(QString path);
    bool release(QString path);
    bool reserve(QString path);
    const QList<QString> keys() const;

private:
    using RemovalPredicate = std::function<bool(const QString &)>;

    void removeMatching(const RemovalPredicate &shouldRemove);

    QMap<QString, std::shared_ptr<CacheItem>> items;
    // Guards the map and every CacheItem state transition. Removal waits use
    // reservationsReleased, which releases this mutex while blocked so scaler
    // threads can always complete release().
    mutable QMutex mutex;
    QWaitCondition reservationsReleased;
};
