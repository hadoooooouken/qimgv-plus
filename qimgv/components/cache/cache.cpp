#include "cache.h"

Cache::Cache() {
}

bool Cache::contains(QString path) const {
    return items.contains(path);
}

bool Cache::insert(std::shared_ptr<Image> img) {
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
    if(items.contains(path)) {
        items[path]->lock();
        items.remove(path);
    }
}

void Cache::clear() {
    items.clear();
}

std::shared_ptr<Image> Cache::get(QString path) {
    if(items.contains(path)) {
        auto item = items.value(path);
        return item->getContents();
    }
    return nullptr;
}

bool Cache::reserve(QString path) {
    if(items.contains(path)) {
        items[path]->lock();
        return true;
    }
    return false;
}

bool Cache::release(QString path) {
    if(items.contains(path)) {
        items[path]->unlock();
        return true;
    }
    return false;
}

// removes all items except the ones in list
void Cache::trimTo(QStringList pathList) {
    const auto keys = items.keys();
    for(const auto &path : keys) {
        if(!pathList.contains(path)) {
            items[path]->lock();
            items.remove(path);
        }
    }
}

const QList<QString> Cache::keys() const {
    return items.keys();
}
