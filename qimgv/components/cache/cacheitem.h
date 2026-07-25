#pragma once

#include <QtTypes>
#include <memory>
#include "sourcecontainers/image.h"

class CacheItem {
public:
    explicit CacheItem(std::shared_ptr<Image> contents);

    std::shared_ptr<Image> contents() const;

    bool tryReserve();
    bool release();
    bool isReserved() const;

    void markForRemoval();

private:
    std::shared_ptr<Image> image;
    qsizetype reservationCount = 0;
    bool removalPending = false;
};

