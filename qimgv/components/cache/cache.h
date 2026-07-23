#pragma once

#include <QDebug>
#include <QMap>
#include <QSemaphore>
#include <QMutex>
#include <QMutexLocker>
#include <memory>
#include "sourcecontainers/image.h"
#include "components/cache/cacheitem.h"
#include "utils/imagefactory.h"

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
    QMap<QString, std::shared_ptr<CacheItem>> items;
    // Guards all access to `items`. Cache is shared between the GUI thread
    // (DirectoryModel's insert/remove/clear/get/trimTo/contains calls) and
    // the scaler's worker thread (reserve/release called from
    // Scaler::onTaskStart/onTaskFinish via a DirectConnection). Every public
    // method must take this lock before touching `items`.
    mutable QMutex mutex;
};
