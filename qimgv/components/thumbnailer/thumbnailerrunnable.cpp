#include "thumbnailerrunnable.h"
#include "settings.h"
#include "utils/colormanager.h"
#include "utils/imagelib.h"
#include <QPainter>
#include <memory>

#include <QColorSpace>

ThumbnailerRunnable::ThumbnailerRunnable(ThumbnailCache *_cache, QString _path,
                                         int _size, bool _crop, bool _force)
    : path(_path), size(_size), crop(_crop), force(_force), cache(_cache) {}

void ThumbnailerRunnable::run() {
  emit taskStart(path, size);
  std::shared_ptr<Thumbnail> thumbnail =
      generate(cache, path, size, crop, force);
  emit taskEnd(thumbnail, path);
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

std::shared_ptr<Thumbnail> ThumbnailerRunnable::generate(ThumbnailCache *cache,
                                                         QString path, int size,
                                                         bool crop,
                                                         bool force) {
  DocumentInfo imgInfo(path);
  QString thumbnailId = generateIdString(path, settings->thumbnailResolution(), false);
  std::unique_ptr<QImage> image;

  QString time = QString::number(imgInfo.lastModified().toMSecsSinceEpoch());

  ThumbnailCache *activeCache = cache;
  if (activeCache && settings->isPathExcludedFromCache(path)) {
    activeCache = nullptr;
  }

  if (!force && activeCache) {
    image = activeCache->readThumbnail(thumbnailId);
    if (image && image->text(QStringLiteral("lastModified")) != time)
      image.reset(nullptr);

    if (image) {
      bool isHdrFile = (imgInfo.format() == QLatin1String("hdr") ||
                        imgInfo.format() == QLatin1String("exr") ||
                        imgInfo.format() == QLatin1String("pfm"));
      if (isHdrFile) {
        *image = image->convertedToColorSpace(QColorSpace(QColorSpace::SRgbLinear));
      }
    }
  }

  if (!image) {
    if (imgInfo.type() == DocumentType::NONE) {
      return std::make_shared<Thumbnail>(imgInfo.fileName(), QString(), size, nullptr);
    }
    std::pair<QImage, QSize> pair;
    pair = createThumbnail(imgInfo.filePath(),
                           imgInfo.format().toStdString().c_str(), 
                           settings->thumbnailResolution(), false);
    image = std::make_unique<QImage>(pair.first);
    QSize originalSize = pair.second;

    if (image && imgInfo.format() == QLatin1String("pdf")) {
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
      image->setText(QStringLiteral("lastModified"), time);

      if (imgInfo.type() == ANIMATED)
        image->setText(QStringLiteral("label"), QStringLiteral(" [a]"));

      if (activeCache) {
        if (originalSize.width() > settings->thumbnailResolution() || originalSize.height() > settings->thumbnailResolution())
          activeCache->saveThumbnail(image.get(), thumbnailId);
      }
    }
  }

  if (!image) {
    return std::make_shared<Thumbnail>(imgInfo.fileName(), QStringLiteral("error"), size, nullptr);
  }

  // scale and crop to the requested grid size
  bool needsScaling = crop ? (image->width() != size || image->height() != size)
                           : (std::max(image->width(), image->height()) != size);
  if (needsScaling) {
    Qt::AspectRatioMode ARMode = crop ? (Qt::KeepAspectRatioByExpanding) : (Qt::KeepAspectRatio);
    QImage scaled = image->scaled(size, size, ARMode, Qt::SmoothTransformation);
    
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
        image = std::make_unique<QImage>(scaled);
      }
    } else {
      image = std::make_unique<QImage>(scaled);
    }
  }

  if (image && imgInfo.format() == QLatin1String("pdf") && image->hasAlphaChannel()) {
    QImage opaqueImg(image->size(), QImage::Format_RGB32);
    opaqueImg.fill(Qt::white);
    QPainter painter(&opaqueImg);
    painter.drawImage(0, 0, *image);
    painter.end();
    *image = opaqueImg;
  }
  auto pixmapPtr = std::make_shared<QPixmap>(image->size());
  *pixmapPtr = QPixmap::fromImage(ColorManager::applyColorManagement(*image));
  pixmapPtr->setDevicePixelRatio(qApp->devicePixelRatio());

  QString label;
  if (pixmapPtr->width() == 0) {
    label = QStringLiteral("error");
  } else {
    // put info into Thumbnail object
    label = image->text(QStringLiteral("originalWidth")) + QLatin1Char('x') + image->text(QStringLiteral("originalHeight")) +
            image->text(QStringLiteral("label"));
  }
  return std::make_shared<Thumbnail>(imgInfo.fileName(), label, size, pixmapPtr);
}

ThumbnailerRunnable::~ThumbnailerRunnable() {}

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
      QSize scaledSize = originalSize.scaled(size, size, ARMode);
      QImage result;
      if (squared) {
        QRect clip(0, 0, size, size);
        QRect scaledRect(QPoint(0, 0), scaledSize);
        clip.moveCenter(scaledRect.center());
        QImage scaled = fullSize.scaled(scaledSize, Qt::IgnoreAspectRatio,
                                        Qt::SmoothTransformation);
        result = ImageLib::croppedRaw(&scaled, clip);
      } else {
        result = fullSize.scaled(scaledSize, Qt::IgnoreAspectRatio,
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
    QSize scaledSize = reader->size().scaled(size, size, ARMode);
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
    QSize scaledSize = fullSize.size().scaled(size, size, ARMode);
    if (squared) {
      QRect clip(0, 0, size, size);
      QRect scaledRect(QPoint(0, 0), scaledSize);
      clip.moveCenter(scaledRect.center());
      QImage scaled = QImage(fullSize.scaled(scaledSize, Qt::IgnoreAspectRatio,
                                              Qt::SmoothTransformation));
      result = ImageLib::croppedRaw(&scaled, clip);
    } else {
      result = fullSize.scaled(scaledSize, Qt::IgnoreAspectRatio,
                               Qt::SmoothTransformation);
    }
  }
  // force reader to close file so it can be deleted later
  reader->setFileName("");
  return std::make_pair(result, originalSize);
}
