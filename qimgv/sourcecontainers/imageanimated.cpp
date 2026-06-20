#include "imageanimated.h"
#include <QImageReader>
#include <time.h>

ImageAnimated::ImageAnimated(QString _path)
    : Image(_path)
{
    mSize.setWidth(0);
    mSize.setHeight(0);
    load();
}

ImageAnimated::ImageAnimated(std::unique_ptr<DocumentInfo> _info)
    : Image(std::move(_info))
{
    mSize.setWidth(0);
    mSize.setHeight(0);
    load();
}

ImageAnimated::~ImageAnimated() {
}

void ImageAnimated::load() {
    if(isLoaded())
        return;
    QImageReader reader(mPath, mDocInfo->format().toStdString().c_str());
    if(reader.canRead()) {
        mSize = reader.size();
        mFrameCount = reader.imageCount();
    } else {
        mSize = QSize(0, 0);
        mFrameCount = 0;
    }
    mLoaded = true;
}

int ImageAnimated::frameCount() const {
    return mFrameCount;
}

// in case of gif returns current frame
std::unique_ptr<QPixmap> ImageAnimated::getPixmap() {
    return std::unique_ptr<QPixmap>(new QPixmap(mPath, mDocInfo->format().toStdString().c_str()));
}

std::shared_ptr<const QImage> ImageAnimated::getImage() {
    std::shared_ptr<const QImage> img(new QImage(mPath, mDocInfo->format().toStdString().c_str()));
    return img;
}

std::shared_ptr<const QImage> ImageAnimated::getDisplayImage() {
    return getImage();
}



int ImageAnimated::height() {
    return mSize.height();
}

int ImageAnimated::width() {
    return mSize.width();
}

QSize ImageAnimated::size() {
    return mSize;
}
