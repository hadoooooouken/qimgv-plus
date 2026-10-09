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
    setDecoded(std::make_shared<const QImage>(std::move(loaded)));
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
    setDecoded(std::make_shared<const QImage>(std::move(loaded)));
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

  // scaling this format via qt results in transparent background
  // it rare enough so lets just convert it to the closest working thing
  if (img->format() == QImage::Format_Mono) {
    setDecoded(std::make_shared<const QImage>(
        img->convertToFormat(QImage::Format_ARGB32)));
  } else {
    setDecoded(std::move(img));
  }
  mLoaded = true;
}

void ImageStatic::loadICO() {
  QImage loaded = ImageLib::loadICO(mPath);
  if (!loaded.isNull()) {
    setDecoded(std::make_shared<const QImage>(std::move(loaded)));
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
  setDecoded(std::make_shared<const QImage>(std::move(rendered.image)));
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
  setDecoded(std::move(img));
  mLoaded = true;
}

// HDR -> SDR as the CPU viewer shows it: tone mapped with the current
// settings, or (tone mapping off or failed) converted to integer sRGB. Either
// way the result is an integer sRGB image without the HDR_* metadata text.
QImage ImageStatic::sdrFromHdr(const QImage &hdr) {
  if (settings && settings->hdrToneMappingEnabled()) {
    const HdrToneMapParams params = {
        .enabled = true,
        .op = static_cast<ToneMapOperator>(settings->hdrToneMappingOperator()),
        .targetWhiteNits = static_cast<float>(settings->hdrTargetWhiteLevel())};
    QImage toneMapped = HdrToneMapper::applyToneMapping(hdr, params);
    if (!toneMapped.isNull())
      return toneMapped;
  }
  const QImage::Format fallbackFormat =
      hdr.hasAlphaChannel() ? QImage::Format_ARGB32 : QImage::Format_RGB32;
  QImage converted = hdr.convertToFormat(fallbackFormat);
  converted.setColorSpace(QColorSpace(QColorSpace::SRgb));
  for (const QString &key : hdr.textKeys()) {
    if (!key.startsWith(QStringLiteral("HDR_")))
      converted.setText(key, hdr.text(key));
  }
  return converted;
}

DisplayPipeline ImageStatic::displayPipeline() const {
  return mDecodeContext.displayPipeline;
}

// The CPU pipeline prepares the colour-managed display copy at load; the GPU
// viewer colour manages (and tone maps) the decoded pixels itself.
void ImageStatic::setDecoded(std::shared_ptr<const QImage> decoded) {
  const bool hdr = decoded && HdrToneMapper::isHdr(*decoded);
  pixels.assign(std::move(decoded), hdr, displayPipeline(), &ImageStatic::sdrFromHdr);
  imageColorManaged.reset();
  if (displayPipeline() == DisplayPipeline::Cpu) {
    if (const std::shared_ptr<const QImage> sdr = pixels.sdr())
      imageColorManaged = std::make_shared<const QImage>(ColorManager::applyColorManagement(*sdr));
  }
}

void ImageStatic::commitEdits() {
  if (isEdited()) {
    pixels.replace(imageEdited);
    // The display copy of the edited pixels becomes the current one.
    imageColorManaged = std::move(imageColorManagedEdited);
    // The effective pixels stay unchanged, so committing does not advance the
    // content revision.
    clearEditedImageState();
    mDocInfo->refresh();
  }
}

std::unique_ptr<QPixmap> ImageStatic::getPixmap() {
  std::unique_ptr<QPixmap> pix(new QPixmap());
  const std::shared_ptr<const QImage> current = getImage();
  if (!current)
    return pix;
  if (settings && settings->colorManagementEnabled())
    pix->convertFromImage(ColorManager::applyColorManagement(*current));
  else
    pix->convertFromImage(*current);
  return pix;
}

std::shared_ptr<const QImage> ImageStatic::getDisplayImage() {
  if (displayPipeline() == DisplayPipeline::Gpu)
    return getDecodedImage();

  if (settings && settings->colorManagementEnabled()) {
    QColorSpace targetSpace = ColorManager::getTargetColorSpace();
    if (isEdited() && imageEdited) {
      if (!imageColorManagedEdited || imageColorManagedEdited->colorSpace() != targetSpace) {
        imageColorManagedEdited = std::make_shared<const QImage>(ColorManager::applyColorManagement(*imageEdited));
      }
      return imageColorManagedEdited;
    } else if (const std::shared_ptr<const QImage> sdr = pixels.sdr()) {
      if (!imageColorManaged || imageColorManaged->colorSpace() != targetSpace) {
        imageColorManaged = std::make_shared<const QImage>(ColorManager::applyColorManagement(*sdr));
      }
      return imageColorManaged;
    }
  } else {
    if (isEdited() && imageEdited) {
      return imageEdited;
    }
    return pixels.sdr();
  }
  return nullptr;
}

std::shared_ptr<const QImage> ImageStatic::getDecodedImage() {
  return isEdited() && imageEdited ? imageEdited : pixels.decoded();
}

std::shared_ptr<const QImage> ImageStatic::getSourceImage() { return pixels.sdr(); }

std::shared_ptr<const QImage> ImageStatic::getImage() {
  return isEdited() ? imageEdited : pixels.sdr();
}

quint64 ImageStatic::contentRevision() const noexcept {
  return mContentRevision;
}

int ImageStatic::height() {
  return size().height();
}

int ImageStatic::width() {
  return size().width();
}

QSize ImageStatic::size() {
  if (isEdited())
    return imageEdited ? imageEdited->size() : QSize();
  return pixels.size();
}

bool ImageStatic::setEditedImage(std::unique_ptr<const QImage> imageEditedNew) {
  if (imageEditedNew && imageEditedNew->width() != 0) {
    const std::shared_ptr<const QImage> original = pixels.sdr();
    const std::shared_ptr<const QImage> currentImage =
        isEdited() ? imageEdited : original;
    if (currentImage && *currentImage == *imageEditedNew)
      return true;

    clearEditedImageState();
    if (original && *original == *imageEditedNew) {
      ++mContentRevision;
      return true;
    }
    imageEdited = std::move(imageEditedNew);
    if (imageEdited && displayPipeline() == DisplayPipeline::Cpu) {
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
