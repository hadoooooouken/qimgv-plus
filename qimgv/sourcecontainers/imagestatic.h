#pragma once

#include <QHash>
#include <QImage>
#include <QImageWriter>
#include <QMutex>
#include <QSemaphore>
#include <QCryptographicHash>
#include "image.h"
#include "sourcecontainers/decodedpixels.h"
#include "utils/decodecontext.h"
#include "utils/imagelib.h"
#include <settings.h>
#include <QIcon>

class ImageStatic : public Image {
public:
    explicit ImageStatic(QString path, DecodeContext context = {});
    explicit ImageStatic(std::unique_ptr<DocumentInfo> info,
                         DecodeContext context = {});
    ~ImageStatic();

    std::unique_ptr<QPixmap> getPixmap();
    std::shared_ptr<const QImage> getSourceImage();
    std::shared_ptr<const QImage> getImage();
    // Cpu pipeline: the tone-mapped, colour-managed image. Gpu pipeline: the
    // decoded image (HDR stays HDR), which the viewer converts itself.
    std::shared_ptr<const QImage> getDisplayImage() override;
    std::shared_ptr<const QImage> getDecodedImage() override;
    // Changes whenever the effective image content is replaced or discarded.
    quint64 contentRevision() const noexcept;

    int height();
    int width();
    QSize size();

    bool setEditedImage(std::unique_ptr<const QImage> imageEditedNew);
    bool discardEditedImage();
    void commitEdits();

    static int pageOverrideForPath(const QString &path);
    static void setPageOverrideForPath(const QString &path, int pageIndex);
    int frameCount() const override;
    int pageIndex() const noexcept;

public slots:
    void crop(QRect newRect);

private:
    void load();
    void loadPdf();
    void loadDjvu();
    [[nodiscard]] static QImage sdrFromHdr(const QImage &hdr);
    [[nodiscard]] DisplayPipeline displayPipeline() const;
    void setDecoded(std::shared_ptr<const QImage> decoded);

    DecodeContext mDecodeContext;
    // The unedited pixels; their SDR copy is getSourceImage().
    DecodedPixels pixels;
    std::shared_ptr<const QImage> imageEdited;
    mutable std::shared_ptr<const QImage> imageColorManaged;
    mutable std::shared_ptr<const QImage> imageColorManagedEdited;
    quint64 mContentRevision = 0;
    void clearEditedImageState() noexcept;
    void loadGeneric();
    void loadICO();
    int mPageCount = 1;
    int mPageIndex = 0;
    static QHash<QString, int> pageOverrides;
    static QMutex pageOverridesMutex;
};
