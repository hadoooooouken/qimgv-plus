#pragma once

#include <QImage>
#include <QImageWriter>
#include <QSemaphore>
#include <QCryptographicHash>
#include "image.h"
#include "utils/imagelib.h"
#include <settings.h>
#include <QIcon>

class ImageStatic : public Image {
public:
    ImageStatic(QString _path);
    ImageStatic(std::unique_ptr<DocumentInfo> _info);
    ~ImageStatic();

    std::unique_ptr<QPixmap> getPixmap();
    std::shared_ptr<const QImage> getSourceImage();
    std::shared_ptr<const QImage> getImage();
    std::shared_ptr<const QImage> getDisplayImage() override;
    // Changes whenever the effective image content is replaced or discarded.
    quint64 contentRevision() const noexcept;

    int height();
    int width();
    QSize size();

    bool setEditedImage(std::unique_ptr<const QImage> imageEditedNew);
    bool discardEditedImage();
    void commitEdits();

    static QHash<QString,int> pageOverride;
    int frameCount() const override;

public slots:
    void crop(QRect newRect);

private:
    void load();
    void loadPdf();
    void loadDjvu();
    std::shared_ptr<const QImage> image, imageEdited;
    mutable std::shared_ptr<const QImage> imageColorManaged;
    mutable std::shared_ptr<const QImage> imageColorManagedEdited;
    quint64 mContentRevision = 0;
    void clearEditedImageState() noexcept;
    void loadGeneric();
    void loadICO();
    int mPageCount = 1;
};
