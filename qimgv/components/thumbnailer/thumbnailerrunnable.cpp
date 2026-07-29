#include "thumbnailerrunnable.h"
#include "settings.h"
#include "utils/colormanager.h"
#include "utils/imagelib.h"
#include <QDebug>
#include <QFileInfo>
#include <QImageReader>
#include <QPainter>
#include <memory>
#include <utility>

#include <QColorSpace>

ThumbnailerRunnable::ThumbnailerRunnable(ThumbnailRequest request)
    : request(std::move(request))
{
}

void ThumbnailerRunnable::run() {
  emit taskStart(request.path, request.size, request.crop);
  ThumbnailTaskResult result = generate(request);
  emit taskEnd(std::move(result), request.path, request.size);
}

QString ThumbnailerRunnable::generateIdString(QString path, int size,
                                              bool crop) {
  QString queryStr = path + QString::number(size);
  if (crop)
    queryStr.append(QLatin1Char('s'));
  return QString::fromLatin1(
      QCryptographicHash::hash(queryStr.toUtf8(), QCryptographicHash::Md5)
          .toHex());
}

ThumbnailTaskResult
ThumbnailerRunnable::generate(const ThumbnailRequest &request) {
  const QString &path = request.path;
  const QString fileName = QFileInfo(path).fileName();
  const int size = request.size;
  const bool crop = request.crop;
  QString thumbnailId = generateIdString(path, settings->thumbnailResolution(), false);
  std::unique_ptr<QImage> image;
  std::optional<ThumbnailCacheCandidate> cacheCandidate;
  std::optional<ThumbnailCache::AccessTouch> accessTouch;
  bool isPdf = false;

  ThumbnailCache *activeCache = request.cache;
  if (activeCache && settings->isPathExcludedFromCache(path)) {
    activeCache = nullptr;
  }

  std::optional<ThumbnailSourceStamp> sourceStamp = request.sourceStamp;
  if (activeCache && !sourceStamp)
    sourceStamp = ThumbnailSourceStamp::fromPath(path);

  if (!request.force && activeCache && sourceStamp) {
    ThumbnailCache::ReadResult cacheResult =
        activeCache->readThumbnail(thumbnailId, *sourceStamp);
    image = std::move(cacheResult.image);
    if (cacheResult.accessTouch) {
      cacheResult.accessTouch->generation = request.cacheGeneration;
      accessTouch = std::move(cacheResult.accessTouch);
    }
    if (image && cacheResult.requiresLinearColorSpace) {
      *image =
          image->convertedToColorSpace(QColorSpace(QColorSpace::SRgbLinear));
    }
  }

  if (!image) {
    DocumentInfo imgInfo(path);
    if (imgInfo.type() == DocumentType::NONE) {
      return {
          std::make_shared<Thumbnail>(fileName, QString(), size, nullptr),
          std::nullopt};
    }
    const QString format = imgInfo.format();
    const bool requiresLinearColorSpace =
        format == QLatin1String("hdr") ||
        format == QLatin1String("exr") ||
        format == QLatin1String("pfm");
    isPdf = format == QLatin1String("pdf");
    const QByteArray formatName = format.toLatin1();
    std::pair<QImage, QSize> pair;
    pair = createThumbnail(imgInfo.filePath(),
                           formatName.constData(),
                           settings->thumbnailResolution(), false);
    image = std::make_unique<QImage>(pair.first);
    QSize originalSize = pair.second;

    if (image && isPdf) {
      QImage opaqueImg(image->size(), QImage::Format_RGB32);
      opaqueImg.fill(Qt::white);
      QPainter painter(&opaqueImg);
      painter.drawImage(0, 0, *image);
      painter.end();
      *image = opaqueImg;
    }

    if (image) {
      image = ImageLib::exifRotated(std::move(image), imgInfo.exifOrientation());
    }

    if (image) {
      // put in image info
      image->setText(QStringLiteral("originalWidth"), QString::number(originalSize.width()));
      image->setText(QStringLiteral("originalHeight"), QString::number(originalSize.height()));

      if (imgInfo.type() == ANIMATED)
        image->setText(QStringLiteral("label"), QStringLiteral(" [a]"));

      if (activeCache && sourceStamp) {
        if (originalSize.width() > settings->thumbnailResolution() ||
            originalSize.height() > settings->thumbnailResolution()) {
          cacheCandidate = ThumbnailCacheCandidate{
              *image, thumbnailId, *sourceStamp,
              requiresLinearColorSpace, request.cacheGeneration};
          activeCache->storeDecodedThumbnail(
              thumbnailId, *sourceStamp, *image,
              requiresLinearColorSpace);
        }
      }
    }
  }

  if (!image) {
    return {
        std::make_shared<Thumbnail>(fileName, QStringLiteral("error"), size,
                                    nullptr),
        std::nullopt};
  }

  // scale and crop to the requested grid size
  Qt::AspectRatioMode ARMode = crop ? (Qt::KeepAspectRatioByExpanding) : (Qt::KeepAspectRatio);
  QSize targetSize = noUpscaleScaledSize(image->size(), size, ARMode);
  bool needsScaling = (image->size() != targetSize);
  if (needsScaling) {
    QImage scaled = image->scaled(targetSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    for (const QString &key : image->textKeys()) {
      scaled.setText(key, image->text(key));
    }
    
    if (crop) {
      QRect clip(0, 0, size, size);
      QRect scaledRect(QPoint(0, 0), scaled.size());
      clip.moveCenter(scaledRect.center());
      
      QImage croppedVal = ImageLib::croppedRaw(&scaled, clip);
      if (!croppedVal.isNull()) {
        std::unique_ptr<QImage> cropped = std::make_unique<QImage>(croppedVal);
        for (const QString &key : image->textKeys()) {
          cropped->setText(key, image->text(key));
        }
        image = std::move(cropped);
      } else {
        // source too small to fill the size x size crop box without
        // upscaling - keep it uncropped at native resolution instead
        image = std::make_unique<QImage>(scaled);
      }
    } else {
      image = std::make_unique<QImage>(scaled);
    }
  }


  if (image && isPdf && image->hasAlphaChannel()) {
    QImage opaqueImg(image->size(), QImage::Format_RGB32);
    opaqueImg.fill(Qt::white);
    QPainter painter(&opaqueImg);
    painter.drawImage(0, 0, *image);
    painter.end();
    *image = opaqueImg;
  }
  QImage colorManaged = ColorManager::applyColorManagement(*image);

  QString label;
  if (colorManaged.width() == 0) {
    label = QStringLiteral("error");
  } else {
    // put info into Thumbnail object
    label = image->text(QStringLiteral("originalWidth")) + QLatin1Char('x') + image->text(QStringLiteral("originalHeight")) +
            image->text(QStringLiteral("label"));
  }
  return {
      std::make_shared<Thumbnail>(fileName, label, size, colorManaged),
      std::move(cacheCandidate),
      std::move(accessTouch)};
}

QSize ThumbnailerRunnable::noUpscaleScaledSize(QSize originalSize, int size,
                                               Qt::AspectRatioMode mode) {
  if (!originalSize.isValid())
    return originalSize;

  if (mode == Qt::KeepAspectRatioByExpanding) {
    // "Expanding" mode always wants to cover the size x size box completely,
    // which forces an upscale once the source is smaller than that box in
    // both dimensions. In that case just keep native resolution instead.
    if (originalSize.width() < size && originalSize.height() < size)
      return originalSize;
  } else {
    // KeepAspectRatio / IgnoreAspectRatio: no scaling is needed at all (and
    // no upscaling happens) once the source already fits within the box.
    if (originalSize.width() <= size && originalSize.height() <= size)
      return originalSize;
  }
  return originalSize.scaled(size, size, mode);
}

std::pair<QImage, QSize>
ThumbnailerRunnable::createThumbnail(QString path, const char *format, int size,
                                     bool squared) {
  bool isIco = (format && QString::compare(QString::fromLatin1(format), QStringLiteral("ico"), Qt::CaseInsensitive) == 0);
  if (isIco) {
    QImage fullSize = ImageLib::loadICO(path);
    if (!fullSize.isNull()) {
      QSize originalSize = fullSize.size();
      Qt::AspectRatioMode ARMode =
          squared ? (Qt::KeepAspectRatioByExpanding) : (Qt::KeepAspectRatio);
      QSize scaledSize = noUpscaleScaledSize(originalSize, size, ARMode);
      QImage result;
      if (squared) {
        QRect clip(0, 0, size, size);
        QRect scaledRect(QPoint(0, 0), scaledSize);
        clip.moveCenter(scaledRect.center());
        QImage scaled = (scaledSize == fullSize.size())
                            ? fullSize
                            : fullSize.scaled(scaledSize, Qt::IgnoreAspectRatio,
                                              Qt::SmoothTransformation);
        result = ImageLib::croppedRaw(&scaled, clip);
        if (result.isNull())
          result = scaled; // source too small to fill the crop box - keep it uncropped rather than upscale
      } else {
        result = (scaledSize == fullSize.size())
                     ? fullSize
                     : fullSize.scaled(scaledSize, Qt::IgnoreAspectRatio,
                                       Qt::SmoothTransformation);
      }
      return std::make_pair(result, originalSize);
    }
  }

  auto reader = std::make_unique<QImageReader>(path, format);
  reader->setAllocationLimit(settings->memoryAllocationLimit());

  // Select the optimal frame for multi-image formats like ICO
  int bestIndex = 0;
  int imageCount = reader->imageCount();
  if (imageCount > 1) {
    int bestDiff = 999999;
    for (int i = 0; i < imageCount; ++i) {
      if (reader->jumpToImage(i)) {
        QSize frameSize = reader->size();
        if (frameSize.isValid()) {
          int diff = qAbs(frameSize.width() - size);
          if (diff < bestDiff) {
            bestDiff = diff;
            bestIndex = i;
          }
        }
      }
    }
    reader->jumpToImage(bestIndex);
  }

  Qt::AspectRatioMode ARMode =
      squared ? (Qt::KeepAspectRatioByExpanding) : (Qt::KeepAspectRatio);
  QImage result;
  QSize originalSize;
  bool indexed = (reader->imageFormat() == QImage::Format_Indexed8);
  bool manualResize = indexed || !reader->supportsOption(QImageIOHandler::Size);
  if (!manualResize) { // resize during read via QImageReader (faster)
    QSize scaledSize = noUpscaleScaledSize(reader->size(), size, ARMode);
    if (scaledSize != reader->size())
      reader->setScaledSize(scaledSize);
    if (squared) {
      QRect clip(0, 0, size, size);
      QRect scaledRect(QPoint(0, 0), scaledSize);
      clip.moveCenter(scaledRect.center());
      reader->setScaledClipRect(clip);
    }
    originalSize = reader->size();
    if (!reader->read(&result)) {
      // If read() returns false there's no guarantee that size conversion
      // worked properly. So we fallback to manual. Se far I've seen this happen
      // only on some weird (corrupted?) jpeg saved from camera
      manualResize = true;
      result = QImage();
      // Force reset reader because it is really finicky
      // and can fail on the second read attempt (yeah wtf)
      reader->setFileName("");
      reader = std::make_unique<QImageReader>(path, format);
      reader->setAllocationLimit(settings->memoryAllocationLimit());
      if (imageCount > 1) {
        reader->jumpToImage(bestIndex);
      }
    }
  }
  if (manualResize) { // manual resize & crop. slower but should just work
    QImage fullSize;
    reader->read(&fullSize);
    if (indexed) {
      auto newFmt = QImage::Format_RGB32;
      if (fullSize.hasAlphaChannel())
        newFmt = QImage::Format_ARGB32;
      QImage tmp = fullSize.convertToFormat(newFmt);
      fullSize = tmp;
    }
    originalSize = fullSize.size();
    QSize scaledSize = noUpscaleScaledSize(fullSize.size(), size, ARMode);
    if (squared) {
      QRect clip(0, 0, size, size);
      QRect scaledRect(QPoint(0, 0), scaledSize);
      clip.moveCenter(scaledRect.center());
      QImage scaled = (scaledSize == fullSize.size())
                          ? fullSize
                          : QImage(fullSize.scaled(scaledSize, Qt::IgnoreAspectRatio,
                                                    Qt::SmoothTransformation));
      result = ImageLib::croppedRaw(&scaled, clip);
      if (result.isNull())
        result = scaled; // source too small to fill the crop box - keep it uncropped rather than upscale
    } else {
      result = (scaledSize == fullSize.size())
                   ? fullSize
                   : fullSize.scaled(scaledSize, Qt::IgnoreAspectRatio,
                                     Qt::SmoothTransformation);
    }
  }
  // force reader to close file so it can be deleted later
  reader->setFileName("");
  return std::make_pair(result, originalSize);
}
