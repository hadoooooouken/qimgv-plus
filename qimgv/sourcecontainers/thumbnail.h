#pragma once

#include <QString>
#include <QPixmap>
#include <QImage>
#include <memory>

class Thumbnail {
public:
    Thumbnail(QString _name, QString _info, int _size, std::shared_ptr<QPixmap> _pixmap);
    Thumbnail(QString _name, QString _info, int _size, QImage _image);
    QString name();
    QString info();
    int size();
    bool hasAlphaChannel();
    std::shared_ptr<QPixmap> pixmap();
private:
    QString mName, mInfo;
    std::shared_ptr<QPixmap> mPixmap;
    QImage mImage;
    int mSize;
    bool mHasAlphaChannel = false;
};

