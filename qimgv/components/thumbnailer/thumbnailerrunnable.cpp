#include "thumbnailerrunnable.h"
#include "utils/colormanager.h"
#include <QPainter>

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
    queryStr.append("s");
  queryStr = QString("%1").arg(QString(
      QCryptographicHash::hash(queryStr.toUtf8(), QCryptographicHash::Md5)
          .toHex()));
  return queryStr;
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
    image.reset(activeCache->readThumbnail(thumbnailId));
    if (image && image->text("lastModified") != time)
      image.reset(nullptr);
  }

  if (!image) {
    if (imgInfo.type() == DocumentType::NONE) {
      std::shared_ptr<Thumbnail> thumbnail(
          new Thumbnail(imgInfo.fileName(), "", size, nullptr));
      return thumbnail;
    }
    std::pair<QImage *, QSize> pair;
    pair = createThumbnail(imgInfo.filePath(),
                           imgInfo.format().toStdString().c_str(), 
                           settings->thumbnailResolution(), false);
    image.reset(pair.first);
    QSize originalSize = pair.second;

    if (image && imgInfo.format() == "pdf") {
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
      image->setText("originalWidth", QString::number(originalSize.width()));
      image->setText("originalHeight", QString::number(originalSize.height()));
      image->setText("lastModified", time);

      if (imgInfo.type() == ANIMATED)
        image->setText("label", " [a]");

      if (activeCache) {
        if (originalSize.width() > settings->thumbnailResolution() || originalSize.height() > settings->thumbnailResolution())
          activeCache->saveThumbnail(image.get(), thumbnailId);
      }
    }
  }

  if (!image) {
    std::shared_ptr<Thumbnail> thumbnail(
        new Thumbnail(imgInfo.fileName(), "error", size, nullptr));
    return thumbnail;
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
      
      std::unique_ptr<QImage> cropped(ImageLib::croppedRaw(&scaled, clip));
      if (cropped) {
        for (const QString &key : image->textKeys()) {
          cropped->setText(key, image->text(key));
        }
        image = std::move(cropped);
      } else {
        image.reset(new QImage(scaled));
      }
    } else {
      image.reset(new QImage(scaled));
    }
  }

  if (image && imgInfo.format() == "pdf" && image->hasAlphaChannel()) {
    QImage opaqueImg(image->size(), QImage::Format_RGB32);
    opaqueImg.fill(Qt::white);
    QPainter painter(&opaqueImg);
    painter.drawImage(0, 0, *image);
    painter.end();
    *image = opaqueImg;
  }
  auto &&tmpPixmap = new QPixmap(image->size());
  *tmpPixmap = QPixmap::fromImage(ColorManager::applyColorManagement(*image));
  tmpPixmap->setDevicePixelRatio(qApp->devicePixelRatio());

  QString label;
  if (tmpPixmap->width() == 0) {
    label = "error";
  } else {
    // put info into Thumbnail object
    label = image->text("originalWidth") + "x" + image->text("originalHeight") +
            image->text("label");
  }
  std::shared_ptr<QPixmap> pixmapPtr(tmpPixmap);
  std::shared_ptr<Thumbnail> thumbnail(
      new Thumbnail(imgInfo.fileName(), label, size, pixmapPtr));
  return thumbnail;
}

ThumbnailerRunnable::~ThumbnailerRunnable() {}

std::pair<QImage *, QSize>
ThumbnailerRunnable::createThumbnail(QString path, const char *format, int size,
                                     bool squared) {
  QImageReader *reader = new QImageReader(path, format);
  reader->setAllocationLimit(settings->memoryAllocationLimit());
  Qt::AspectRatioMode ARMode =
      squared ? (Qt::KeepAspectRatioByExpanding) : (Qt::KeepAspectRatio);
  QImage *result = nullptr;
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
    result = new QImage();
    if (!reader->read(result)) {
      // If read() returns false there's no guarantee that size conversion
      // worked properly. So we fallback to manual. Se far I've seen this happen
      // only on some weird (corrupted?) jpeg saved from camera
      manualResize = true;
      delete result;
      result = nullptr;
      // Force reset reader because it is really finicky
      // and can fail on the second read attempt (yeah wtf)
      reader->setFileName("");
      delete reader;
      reader = new QImageReader(path, format);
      reader->setAllocationLimit(settings->memoryAllocationLimit());
    }
  }
  if (manualResize) { // manual resize & crop. slower but should just work
    QImage *fullSize = new QImage();
    reader->read(fullSize);
    if (indexed) {
      auto newFmt = QImage::Format_RGB32;
      if (fullSize->hasAlphaChannel())
        newFmt = QImage::Format_ARGB32;
      auto tmp = new QImage(fullSize->convertToFormat(newFmt));
      delete fullSize;
      fullSize = tmp;
    }
    originalSize = fullSize->size();
    QSize scaledSize = fullSize->size().scaled(size, size, ARMode);
    if (squared) {
      QRect clip(0, 0, size, size);
      QRect scaledRect(QPoint(0, 0), scaledSize);
      clip.moveCenter(scaledRect.center());
      QImage scaled = QImage(fullSize->scaled(scaledSize, Qt::IgnoreAspectRatio,
                                              Qt::SmoothTransformation));
      result = ImageLib::croppedRaw(&scaled, clip);
    } else {
      result = new QImage(fullSize->scaled(scaledSize, Qt::IgnoreAspectRatio,
                                           Qt::SmoothTransformation));
    }
    delete fullSize;
  }
  // force reader to close file so it can be deleted later
  reader->setFileName("");
  delete reader;
  return std::make_pair(result, originalSize);
}
