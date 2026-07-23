#include "thumbnail.h"
#include <QGuiApplication>

Thumbnail::Thumbnail(QString _name, QString _info, int _size, std::shared_ptr<QPixmap> _pixmap)
    : mName(_name),
      mInfo(_info),
      mPixmap(_pixmap),
      mSize(_size)
{
    if(_pixmap)
        mHasAlphaChannel = _pixmap->hasAlphaChannel();
}

Thumbnail::Thumbnail(QString _name, QString _info, int _size, QImage _image)
    : mName(_name),
      mInfo(_info),
      mImage(_image),
      mSize(_size),
      mHasAlphaChannel(_image.hasAlphaChannel())
{
}

QString Thumbnail::name() {
    return mName;
}

QString Thumbnail::info() {
    return mInfo;
}

int Thumbnail::size() {
    return mSize;
}

bool Thumbnail::hasAlphaChannel() {
    return mHasAlphaChannel;
}

std::shared_ptr<QPixmap> Thumbnail::pixmap() {
    if (!mPixmap && !mImage.isNull()) {
        mPixmap = std::make_shared<QPixmap>(QPixmap::fromImage(mImage));
        mPixmap->setDevicePixelRatio(qApp->devicePixelRatio());
    }
    return mPixmap;
}

