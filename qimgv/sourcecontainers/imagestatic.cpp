#include "imagestatic.h"
#include "settings.h"
#include "utils/blendreader.h"
#include "utils/colormanager.h"
#include "utils/djvureader.h"
#include "utils/fontpreview.h"
#include "utils/hdrtonemapper.h"
#include <QMutexLocker>
#include <QPainter>
#include <QPdfDocument>
#include <time.h>


ImageStatic::ImageStatic(QString path, DecodeContext context)
    : Image(path), mDecodeContext(std::move(context))
{
  load();
}

ImageStatic::ImageStatic(std::unique_ptr<DocumentInfo> info,
                         DecodeContext context)
    : Image(std::move(info)), mDecodeContext(std::move(context)) {
  load();
}

ImageStatic::~ImageStatic() {}

QHash<QString, int> ImageStatic::pageOverrides;
QMutex ImageStatic::pageOverridesMutex;

int ImageStatic::pageOverrideForPath(const QString &path) {
  const QMutexLocker locker(&pageOverridesMutex);
  return pageOverrides.value(path, 0);
}

void ImageStatic::setPageOverrideForPath(const QString &path, int pageIndex) {
  const QMutexLocker locker(&pageOverridesMutex);
  pageOverrides.insert(path, pageIndex);
}

int ImageStatic::frameCount() const {
  return mPageCount;
}

int ImageStatic::pageIndex() const noexcept {
  return mPageIndex;
}

// load image data from disk
void ImageStatic::load() {
  if (isLoaded() || mDecodeContext.isCancellationRequested()) {
    return;
  }
  if (mDocInfo->mimeType().name() == "image/vnd.microsoft.icon")
    loadICO();
  else if (mDocInfo->format() == "pdf")
    loadPdf();
  else if (mDocInfo->format() == "djvu")
    loadDjvu();
  else if (mDocInfo->format() == "font") {
    QImage loaded = FontPreview::render(mPath);
    if (loaded.isNull()) {
      qWarning() << "ImageStatic: failed to render font preview" << mPath;
      return;
    }
    image = std::make_shared<const QImage>(std::move(loaded));
    imageColorManaged =
        std::make_shared<const QImage>(ColorManager::applyColorManagement(*image));
    mLoaded = true;
  }
  else if (mDocInfo->format() == "blend") {
    QString error;
    QImage loaded = BlendReader::readPreview(mPath, mDecodeContext, &error);
    if (loaded.isNull()) {
      if (!mDecodeContext.isCancellationRequested() && !error.isEmpty()) {
        qWarning() << "ImageStatic: failed to load Blender preview" << mPath
                   << "Error:" << error;
      }
      return;
    }
    image = std::make_shared<const QImage>(std::move(loaded));
    imageColorManaged =
        std::make_shared<const QImage>(ColorManager::applyColorManagement(*image));
    mLoaded = true;
  }
  else
    loadGeneric();
}

void ImageStatic::loadGeneric() {
  QImageReader r(mPath, mDocInfo->format().toStdString().c_str());
  r.setAllocationLimit(settings->memoryAllocationLimit());

  // The DDS plugin reports mip levels through imageCount(), but those are
  // downscaled copies of the same texture, not separate pages/frames like
  // in a multi-page TIFF/PDF. Treat DDS as single-page and always load the
  // base (largest) mip level, so it doesn't get shown as "Page 1/N".
  if (mDocInfo->format() == "dds") {
    mPageCount = 1;
  } else {
    int count = r.imageCount();
    mPageCount = count > 0 ? count : 1;

    int page = pageOverrideForPath(mPath);
    if (page > 0 && page < mPageCount && r.jumpToImage(page))
      mPageIndex = page;
  }

  QSize sz = r.size();
  if (sz.isValid() && sz.width() > 0 && sz.height() > 0) {
    constexpr int kMaxDimension = 16384;
    if (sz.width() > kMaxDimension || sz.height() > kMaxDimension) {
      QSize scaledSize = sz;
      scaledSize.scale(kMaxDimension, kMaxDimension, Qt::KeepAspectRatio);
      r.setScaledSize(scaledSize);
      qWarning() << "ImageStatic: Image size" << sz << "exceeds limit. Downscaling to" << scaledSize << "for display.";
    }
  }
  QImage *tmp = new QImage();
  if (!r.read(tmp)) {
    qWarning() << "ImageStatic: failed to load" << mPath
             << "Error:" << r.errorString();
    delete tmp;
    return;
  }
  std::unique_ptr<const QImage> img(tmp);
  img =
      ImageLib::exifRotated(std::move(img), mDocInfo.get()->exifOrientation());

  if (HdrToneMapper::isHdr(*img)) {
    // Guarantees that every isHdr()==true image leaves this function as an
    // integer sRGB QImage, whether tone-mapping succeeds, is disabled, or
    // HdrToneMapper fails to produce an image (e.g. an allocation failure).
    auto sdrFallbackConvert = [](const QImage &src) {
      QImage::Format fallbackFmt = src.hasAlphaChannel() ? QImage::Format_ARGB32 : QImage::Format_RGB32;
      QImage converted = src.convertToFormat(fallbackFmt);
      converted.setColorSpace(QColorSpace(QColorSpace::SRgb));
      for (const QString &key : src.textKeys()) {
        if (!key.startsWith(QStringLiteral("HDR_"))) {
          converted.setText(key, src.text(key));
        }
      }
      return converted;
    };

    if (settings && settings->hdrToneMappingEnabled()) {
      HdrToneMapParams params = {
          .enabled = true,
          .op = static_cast<ToneMapOperator>(settings->hdrToneMappingOperator()),
          .targetWhiteNits = static_cast<float>(settings->hdrTargetWhiteLevel())
      };
      QImage toneMapped = HdrToneMapper::applyToneMapping(*img, params);
      if (!toneMapped.isNull()) {
        img = std::make_unique<const QImage>(std::move(toneMapped));
      } else {
        img = std::make_unique<const QImage>(sdrFallbackConvert(*img));
      }
    } else {
      // Fallback SDR conversion when HDR tone-mapping is disabled
      img = std::make_unique<const QImage>(sdrFallbackConvert(*img));
    }
  }

  // scaling this format via qt results in transparent background
  // it rare enough so lets just convert it to the closest working thing
  if (img->format() == QImage::Format_Mono) {
    QImage *imgConverted = new QImage();
    *imgConverted = img->convertToFormat(QImage::Format_ARGB32);
    image.reset(imgConverted);
  } else {
    // set image
    image = std::move(img);
  }
  if (image) {
    imageColorManaged = std::make_shared<const QImage>(ColorManager::applyColorManagement(*image));
  }
  mLoaded = true;
}

void ImageStatic::loadICO() {
  QImage loaded = ImageLib::loadICO(mPath);
  if (!loaded.isNull()) {
    image = std::make_shared<const QImage>(std::move(loaded));
    imageColorManaged = std::make_shared<const QImage>(ColorManager::applyColorManagement(*image));
    mLoaded = true;
  } else {
    qWarning() << "ImageStatic: failed to load ico" << mPath;
  }
}

void ImageStatic::loadDjvu() {
  int page = pageOverrideForPath(mPath);
  constexpr int kMaxDisplayDimension = 16384;
  const DjvuDecodeLimits limits = DjvuDecodeLimits::fromMemoryLimitMiB(
      settings->memoryAllocationLimit(), kMaxDisplayDimension);
  DjvuRenderResult rendered =
      DjvuReader::renderPage(mPath, page, limits, mDecodeContext);

  if (rendered.pageCount <= 0 || rendered.image.isNull()) {
    if (mDecodeContext.isCancellationRequested())
      return;
    qWarning() << "ImageStatic: failed to load DjVu" << mPath;
    return;
  }

  mPageIndex = rendered.pageIndex;
  mPageCount = rendered.pageCount;
  image = std::make_shared<const QImage>(std::move(rendered.image));
  imageColorManaged =
      std::make_shared<const QImage>(ColorManager::applyColorManagement(*image));
  mLoaded = true;
}

void ImageStatic::loadPdf() {
  QPdfDocument doc;
  if (doc.load(mPath) != QPdfDocument::Error::None) {
    qWarning() << "ImageStatic: failed to load pdf" << mPath;
    return;
  }
  int pageCount = doc.pageCount();
  if (pageCount < 1) {
    qWarning() << "ImageStatic: pdf has no pages" << mPath;
    return;
  }
  mPageCount = pageCount;

  int page = pageOverrideForPath(mPath);
  if (page < 0 || page >= pageCount)
    page = 0;

  constexpr qreal kDpi = 5.0 * 72.0;
  QSizeF ptSize = doc.pagePointSize(page);
  QSize pixelSize = (ptSize * kDpi / 72.0).toSize();

  QImage rendered = doc.render(page, pixelSize);
  if (rendered.isNull()) {
    qWarning() << "ImageStatic: failed to render pdf page" << mPath;
    return;
  }

  mPageIndex = page;
  QImage opaqueImg(rendered.size(), QImage::Format_RGB32);
  opaqueImg.fill(Qt::white);
  QPainter painter(&opaqueImg);
  painter.drawImage(0, 0, rendered);
  painter.end();

  std::unique_ptr<const QImage> img(new QImage(std::move(opaqueImg)));
  img = ImageLib::exifRotated(std::move(img), mDocInfo.get()->exifOrientation());
  image = std::move(img);

  if (image)
    imageColorManaged = std::make_shared<const QImage>(ColorManager::applyColorManagement(*image));
  mLoaded = true;
}

void ImageStatic::commitEdits() {
  if (isEdited()) {
    image.swap(imageEdited);
    // The effective pixels stay unchanged, so committing does not advance the
    // content revision.
    clearEditedImageState();
    mDocInfo->refresh();
  }
}

std::unique_ptr<QPixmap> ImageStatic::getPixmap() {
  std::unique_ptr<QPixmap> pix(new QPixmap());
  if (settings && settings->colorManagementEnabled()) {
    QColorSpace targetSpace = ColorManager::getTargetColorSpace();
    if (isEdited() && imageEdited) {
      if (!imageColorManagedEdited || imageColorManagedEdited->colorSpace() != targetSpace) {
        imageColorManagedEdited = std::make_shared<const QImage>(ColorManager::applyColorManagement(*imageEdited));
      }
      pix->convertFromImage(*imageColorManagedEdited);
    } else if (image) {
      if (!imageColorManaged || imageColorManaged->colorSpace() != targetSpace) {
        imageColorManaged = std::make_shared<const QImage>(ColorManager::applyColorManagement(*image));
      }
      pix->convertFromImage(*imageColorManaged);
    }
  } else {
    if (isEdited() && imageEdited) {
      pix->convertFromImage(*imageEdited);
    } else if (image) {
      pix->convertFromImage(*image);
    }
  }
  return pix;
}

std::shared_ptr<const QImage> ImageStatic::getDisplayImage() {
  if (settings && settings->colorManagementEnabled()) {
    QColorSpace targetSpace = ColorManager::getTargetColorSpace();
    if (isEdited() && imageEdited) {
      if (!imageColorManagedEdited || imageColorManagedEdited->colorSpace() != targetSpace) {
        imageColorManagedEdited = std::make_shared<const QImage>(ColorManager::applyColorManagement(*imageEdited));
      }
      return imageColorManagedEdited;
    } else if (image) {
      if (!imageColorManaged || imageColorManaged->colorSpace() != targetSpace) {
        imageColorManaged = std::make_shared<const QImage>(ColorManager::applyColorManagement(*image));
      }
      return imageColorManaged;
    }
  } else {
    if (isEdited() && imageEdited) {
      return imageEdited;
    } else if (image) {
      return image;
    }
  }
  return nullptr;
}

std::shared_ptr<const QImage> ImageStatic::getSourceImage() { return image; }

std::shared_ptr<const QImage> ImageStatic::getImage() {
  return isEdited() ? imageEdited : image;
}

quint64 ImageStatic::contentRevision() const noexcept {
  return mContentRevision;
}

int ImageStatic::height() {
  const QImage *img = (isEdited() ? imageEdited : image).get();
  return img ? img->height() : 0;
}

int ImageStatic::width() {
  const QImage *img = (isEdited() ? imageEdited : image).get();
  return img ? img->width() : 0;
}

QSize ImageStatic::size() {
  const QImage *img = (isEdited() ? imageEdited : image).get();
  return img ? img->size() : QSize();
}

bool ImageStatic::setEditedImage(std::unique_ptr<const QImage> imageEditedNew) {
  if (imageEditedNew && imageEditedNew->width() != 0) {
    const std::shared_ptr<const QImage> currentImage =
        isEdited() ? imageEdited : image;
    if (currentImage && *currentImage == *imageEditedNew)
      return true;

    clearEditedImageState();
    if (image && *image == *imageEditedNew) {
      ++mContentRevision;
      return true;
    }
    imageEdited = std::move(imageEditedNew);
    if (imageEdited) {
      imageColorManagedEdited = std::make_shared<const QImage>(ColorManager::applyColorManagement(*imageEdited));
    }
    mEdited = true;
    ++mContentRevision;
    return true;
  }
  return false;
}

bool ImageStatic::discardEditedImage() {
  if (imageEdited) {
    clearEditedImageState();
    ++mContentRevision;
    return true;
  }
  return false;
}

void ImageStatic::clearEditedImageState() noexcept {
  imageEdited.reset();
  imageColorManagedEdited.reset();
  mEdited = false;
}
