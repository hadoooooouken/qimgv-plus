#include "decodedpixels.h"

#include <QDebug>
#include <QMutexLocker>

#include <utility>

void DecodedPixels::assign(std::shared_ptr<const QImage> decoded, bool hdr,
                           SdrConversion toSdr) {
    QMutexLocker locker(&mMutex);
    mHdr.reset();
    mSdr.reset();
    mToSdr = std::move(toSdr);
    mConversionFailed = false;
    if (!decoded)
        return;
    if (!hdr) {
        mSdr = std::move(decoded);
        return;
    }
    mHdr = std::move(decoded);
}

std::shared_ptr<const QImage> DecodedPixels::sdr() {
    QMutexLocker locker(&mMutex);
    if (!mSdr && mHdr && !mConversionFailed)
        convertLocked();
    return mSdr;
}

std::shared_ptr<const QImage> DecodedPixels::decoded() const {
    QMutexLocker locker(&mMutex);
    return mHdr ? mHdr : mSdr;
}

bool DecodedPixels::hasHdrSource() const {
    QMutexLocker locker(&mMutex);
    return mHdr != nullptr;
}

QSize DecodedPixels::size() const {
    QMutexLocker locker(&mMutex);
    if (mHdr)
        return mHdr->size();
    return mSdr ? mSdr->size() : QSize();
}

void DecodedPixels::replace(std::shared_ptr<const QImage> sdrPixels) {
    QMutexLocker locker(&mMutex);
    mHdr.reset();
    mSdr = std::move(sdrPixels);
    mConversionFailed = false;
}

void DecodedPixels::convertLocked() {
    if (!mToSdr) {
        qWarning() << "DecodedPixels: an HDR image has no SDR conversion";
        mConversionFailed = true;
        return;
    }
    QImage converted = mToSdr(*mHdr);
    if (converted.isNull()) {
        qWarning() << "DecodedPixels: the HDR image could not be converted to SDR";
        mConversionFailed = true;
        return;
    }
    mSdr = std::make_shared<const QImage>(std::move(converted));
}
