#pragma once

#include "image.h"

class ImageAnimated : public Image {
public:
    ImageAnimated(QString _path);
    ImageAnimated(std::unique_ptr<DocumentInfo> _info);
    ~ImageAnimated();

    std::unique_ptr<QPixmap> getPixmap();
    std::shared_ptr<const QImage> getImage();
    std::shared_ptr<const QImage> getDisplayImage() override;
    int height();
    int width();
    QSize size();

    int frameCount() const override;

signals:
    void frameChanged(QPixmap*);

private:
    void load();
    QSize mSize;
    int mFrameCount;
};
