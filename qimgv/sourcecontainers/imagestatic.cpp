#include "imagestatic.h"
#include "settings.h"
#include "utils/colormanager.h"
#include <QPainter>
#include <time.h>


ImageStatic::ImageStatic(QString _path) : Image(_path) { load(); }

ImageStatic::ImageStatic(std::unique_ptr<DocumentInfo> _info)
    : Image(std::move(_info)) {
  load();
}

ImageStatic::~ImageStatic() {}

// load image data from disk
void ImageStatic::load() {
  if (isLoaded()) {
    return;
  }
  if (mDocInfo->mimeType().name() == "image/vnd.microsoft.icon")
    loadICO();
  else
    loadGeneric();
}

void ImageStatic::loadGeneric() {
  /* QImageReader::read() seems more reliable than just reading via QImage.
   * For example: "Invalid JPEG file structure: two SOF markers"
   * QImageReader::read() returns false, but still reads an image. Meanwhile
   * QImage just fails. I havent checked qimage's code, but it seems like it
   * sees an exception from libjpeg or whatever and just gives up on reading the
   * file.
   *
   * tldr: qimage bad
   */
  QImageReader r(mPath, mDocInfo->format().toStdString().c_str());
  r.setAllocationLimit(settings->memoryAllocationLimit());
  QSize sz = r.size();
  if (sz.isValid() && sz.width() > 0 && sz.height() > 0) {
    if (mDocInfo->format() == "pdf") {
      r.setScaledSize(sz * 5);
    } else {
      constexpr int kMaxDimension = 16384;
      if (sz.width() > kMaxDimension || sz.height() > kMaxDimension) {
        QSize scaledSize = sz;
        scaledSize.scale(kMaxDimension, kMaxDimension, Qt::KeepAspectRatio);
        r.setScaledSize(scaledSize);
        qWarning() << "ImageStatic: Image size" << sz << "exceeds limit. Downscaling to" << scaledSize << "for display.";
      }
    }
  }
  QImage *tmp = new QImage();
  if (!r.read(tmp)) {
    qWarning() << "ImageStatic: failed to load" << mPath
             << "Error:" << r.errorString();
    delete tmp;
    return;
  }
  if (mDocInfo->format() == "pdf") {
    QImage opaqueImg(tmp->size(), QImage::Format_RGB32);
    opaqueImg.fill(Qt::white);
    QPainter painter(&opaqueImg);
    painter.drawImage(0, 0, *tmp);
    painter.end();
    *tmp = opaqueImg;
  }
  std::unique_ptr<const QImage> img(tmp);
  img =
      ImageLib::exifRotated(std::move(img), mDocInfo.get()->exifOrientation());
  // scaling this format via qt results in transparent background
  // it rare enough so lets just convert it to the closest working thing
  if (img->format() == QImage::Format_Mono) {
    QImage *imgConverted = new QImage();
    *imgConverted = img->convertToFormat(QImage::Format_Grayscale8);
    image.reset(imgConverted);
  } else {
    // set image
    if (img->format() == QImage::Format_RGBX32FPx4 ||
        img->format() == QImage::Format_RGBX16FPx4) {
      QImage *imgConverted = new QImage();
      *imgConverted = img->convertToFormat(QImage::Format_RGBA64);
      image.reset(imgConverted);
    } else {
      image = std::move(img);
    }
  }
  if (image) {
    imageColorManaged = std::make_shared<const QImage>(ColorManager::applyColorManagement(*image));
  }
  mLoaded = true;
}

// TODO: move this out somewhere to use in other places
void ImageStatic::loadICO() {
  // Big brain code. It's mostly for small ico files so whatever. I'm not
  // patching Qt for this.
  QIcon icon(mPath);
  QList<QSize> sizes = icon.availableSizes();
  QSize maxSize(0, 0);
  for (auto sz : std::as_const(sizes))
    if (maxSize.width() < sz.width())
      maxSize = sz;
  QPixmap iconPix = icon.pixmap(maxSize);
  std::unique_ptr<const QImage> img(new QImage(iconPix.toImage()));
  image = std::move(img);
  if (image) {
    imageColorManaged = std::make_shared<const QImage>(ColorManager::applyColorManagement(*image));
  }
  mLoaded = true;
}

QString ImageStatic::generateHash(QString str) {
  return QString(
      QCryptographicHash::hash(str.toUtf8(), QCryptographicHash::Md5).toHex());
}

// TODO: move saving to directorymodel
bool ImageStatic::save(QString destPath) {
  QString tmpPath = destPath + "_" + generateHash(destPath);
  QFileInfo fi(destPath);
  QString ext = fi.suffix();
  // png compression note from libpng
  // Note that tests have shown that zlib compression levels 3-6 usually perform
  // as well as level 9 for PNG images, and do considerably fewer caclulations
  int quality = 95;
  if (ext.compare("png", Qt::CaseInsensitive) == 0)
    quality = settings->pngSaveQuality() * 10;
  else if (ext.compare("jpg", Qt::CaseInsensitive) == 0 ||
           ext.compare("jpeg", Qt::CaseInsensitive) == 0)
    quality = settings->JPEGSaveQuality();
  else if (ext.compare("jxl", Qt::CaseInsensitive) == 0 ||
           ext.compare("webp", Qt::CaseInsensitive) == 0 ||
           ext.compare("avif", Qt::CaseInsensitive) == 0)
    quality = settings->modernSaveQuality();

  bool backupExists = false, success = false, originalExists = false;

  if (QFile::exists(destPath))
    originalExists = true;

  // backup the original file if possible
  if (originalExists) {
    QFile::remove(tmpPath);
    if (!QFile::copy(destPath, tmpPath)) {
      qWarning() << "ImageStatic::save() - Could not create file backup.";
      return false;
    }
    backupExists = true;
  }
  bool isOverwrite = (destPath.compare(mPath, Qt::CaseInsensitive) == 0);

  // save file
  if (isEdited()) {
    success = imageEdited->save(destPath, ext.toStdString().c_str(), quality);
    // only replace the base image when overwriting the original file;
    // Save-As to a different path must keep the original image intact
    if (isOverwrite) {
      image.swap(imageEdited);
      discardEditedImage();
    }
  } else {
    success = image->save(destPath, ext.toStdString().c_str(), quality);
  }
  if (backupExists) {
    if (success) {
      // everything ok - remove the backup
      QFile file(tmpPath);
      file.remove();
    } else if (originalExists) {
      // revert on fail
      QFile::remove(mDocInfo->filePath());
      QFile::copy(tmpPath, mDocInfo->filePath());
      QFile::remove(tmpPath);
    }
  }
  if (isOverwrite && success)
    mDocInfo->refresh();
  return success;
}

bool ImageStatic::save() { return save(mPath); }

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

int ImageStatic::height() {
  return isEdited() ? imageEdited->height() : image->height();
}

int ImageStatic::width() {
  return isEdited() ? imageEdited->width() : image->width();
}

QSize ImageStatic::size() {
  return isEdited() ? imageEdited->size() : image->size();
}

bool ImageStatic::setEditedImage(std::unique_ptr<const QImage> imageEditedNew) {
  if (imageEditedNew && imageEditedNew->width() != 0) {
    discardEditedImage();
    if (image && *image == *imageEditedNew) {
      return true;
    }
    imageEdited = std::move(imageEditedNew);
    if (imageEdited) {
      imageColorManagedEdited = std::make_shared<const QImage>(ColorManager::applyColorManagement(*imageEdited));
    }
    mEdited = true;
    return true;
  }
  return false;
}

bool ImageStatic::discardEditedImage() {
  if (imageEdited) {
    imageEdited.reset();
    imageColorManagedEdited.reset();
    mEdited = false;
    return true;
  }
  return false;
}
