#include "cacheitem.h"

#include <utility>

CacheItem::CacheItem(std::shared_ptr<Image> contents)
    : image(std::move(contents)) {
}

std::shared_ptr<Image> CacheItem::contents() const {
    return image;
}

bool CacheItem::tryReserve() {
    if(removalPending) {
        return false;
    }
    ++reservationCount;
    return true;
}

bool CacheItem::release() {
    if(reservationCount == 0) {
        return false;
    }
    --reservationCount;
    return true;
}

bool CacheItem::isReserved() const {
    return reservationCount != 0;
}

void CacheItem::markForRemoval() {
    removalPending = true;
}
