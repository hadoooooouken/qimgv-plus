#include "cache.h"

#include <QMutexLocker>
#include <QSet>

Cache::Cache() {
}

bool Cache::contains(QString path) const {
    QMutexLocker locker(&mutex);
    return items.contains(path);
}

bool Cache::insert(std::shared_ptr<Image> img) {
    QMutexLocker locker(&mutex);
    if(img) {
        if(items.contains(img->filePath())) {
            return false;
        } else {
            items.insert(img->filePath(), std::make_shared<CacheItem>(img));
            return true;
        }
    }
    return true;
}

void Cache::remove(QString path) {
    removeMatching([&path](const QString &itemPath) {
        return itemPath == path;
    });
}

void Cache::clear() {
    removeMatching([](const QString &) {
        return true;
    });
}

std::shared_ptr<Image> Cache::get(QString path) {
    QMutexLocker locker(&mutex);
    if(items.contains(path)) {
        auto item = items.value(path);
        return item->contents();
    }
    return nullptr;
}

bool Cache::reserve(QString path) {
    QMutexLocker locker(&mutex);
    if(items.contains(path)) {
        return items.value(path)->tryReserve();
    }
    return false;
}

bool Cache::release(QString path) {
    QMutexLocker locker(&mutex);
    if(items.contains(path)) {
        const auto item = items.value(path);
        if(!item->release()) {
            return false;
        }
        if(!item->isReserved()) {
            reservationsReleased.wakeAll();
        }
        return true;
    }
    return false;
}

// removes all items except the ones in list
void Cache::trimTo(QStringList pathList) {
    const QSet<QString> retainedPaths(pathList.cbegin(), pathList.cend());
    removeMatching([&retainedPaths](const QString &path) {
        return !retainedPaths.contains(path);
    });
}

void Cache::removeMatching(const RemovalPredicate &shouldRemove) {
    QMutexLocker locker(&mutex);
    QList<QPair<QString, std::shared_ptr<CacheItem>>> targets;

    for(auto it = items.cbegin(); it != items.cend(); ++it) {
        if(shouldRemove(it.key())) {
            it.value()->markForRemoval();
            targets.append(qMakePair(it.key(), it.value()));
        }
    }

    bool hasReservations = true;
    while(hasReservations) {
        hasReservations = false;
        for(const auto &target : targets) {
            if(target.second->isReserved()) {
                hasReservations = true;
                break;
            }
        }
        if(hasReservations) {
            reservationsReleased.wait(&mutex);
        }
    }

    for(const auto &target : targets) {
        auto current = items.find(target.first);
        if(current != items.end() && current.value() == target.second) {
            items.erase(current);
        }
    }
}

const QList<QString> Cache::keys() const {
    QMutexLocker locker(&mutex);
    return items.keys();
}
