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
            items.insert(img->filePath(), img);
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
    return items.value(path);
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
    for(auto it = items.begin(); it != items.end();) {
        if(shouldRemove(it.key())) {
            it = items.erase(it);
        } else {
            ++it;
        }
    }
}

const QList<QString> Cache::keys() const {
    QMutexLocker locker(&mutex);
    return items.keys();
}
