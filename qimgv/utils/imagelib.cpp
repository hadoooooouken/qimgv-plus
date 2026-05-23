#include "imagelib.h"

void ImageLib::recolor(QPixmap &pixmap, QColor color) {
  QPainter p(&pixmap);
  p.setCompositionMode(QPainter::CompositionMode_SourceIn);
  p.setBrush(color);
  p.setPen(color);
  p.drawRect(pixmap.rect());
}

QImage *ImageLib::rotatedRaw(const QImage *src, int grad) {
  if (!src)
    return new QImage();
  QImage *img = new QImage();
  QTransform transform;
  transform.rotate(grad);
  *img = src->transformed(transform, Qt::SmoothTransformation);
  return img;
}
//------------------------------------------------------------------------------
QImage *ImageLib::rotated(std::shared_ptr<const QImage> src, int grad) {
  return rotatedRaw(src.get(), grad);
}
//------------------------------------------------------------------------------
QImage *ImageLib::croppedRaw(const QImage *src, QRect newRect) {
  if (src && src->rect().contains(newRect, false)) {
    QImage *img = new QImage(newRect.size(), src->format());
    *img = src->copy(newRect);
    return img;
  } else {
    return new QImage();
  }
}
//------------------------------------------------------------------------------
QImage *ImageLib::cropped(std::shared_ptr<const QImage> src, QRect newRect) {
  return croppedRaw(src.get(), newRect);
}
//------------------------------------------------------------------------------
QImage *ImageLib::flippedHRaw(const QImage *src) {
  if (!src)
    return new QImage();
  else
    return new QImage(src->mirrored(true, false));
}
//------------------------------------------------------------------------------
QImage *ImageLib::flippedH(std::shared_ptr<const QImage> src) {
  return flippedHRaw(src.get());
}
//------------------------------------------------------------------------------
QImage *ImageLib::flippedVRaw(const QImage *src) {
  if (!src)
    return new QImage();
  else
    return new QImage(src->mirrored(false, true));
}
//------------------------------------------------------------------------------
QImage *ImageLib::flippedV(std::shared_ptr<const QImage> src) {
  return flippedVRaw(src.get());
}
//------------------------------------------------------------------------------
std::unique_ptr<const QImage>
ImageLib::exifRotated(std::unique_ptr<const QImage> src, int orientation) {
  switch (orientation) {
  case 1: {
    src.reset(ImageLib::flippedHRaw(src.get()));
  } break;
  case 2: {
    src.reset(ImageLib::flippedVRaw(src.get()));
  } break;
  case 3: {
    src.reset(ImageLib::flippedHRaw(src.get()));
    src.reset(ImageLib::flippedVRaw(src.get()));
  } break;
  case 4: {
    src.reset(ImageLib::rotatedRaw(src.get(), 90));
  } break;
  case 5: {
    src.reset(ImageLib::flippedHRaw(src.get()));
    src.reset(ImageLib::rotatedRaw(src.get(), 90));
  } break;
  case 6: {
    src.reset(ImageLib::flippedVRaw(src.get()));
    src.reset(ImageLib::rotatedRaw(src.get(), 90));
  } break;
  case 7: {
    src.reset(ImageLib::rotatedRaw(src.get(), -90));
  } break;
  default: {
  } break;
  }
  return src;
}
//------------------------------------------------------------------------------
std::unique_ptr<QImage> ImageLib::exifRotated(std::unique_ptr<QImage> src,
                                              int orientation) {
  switch (orientation) {
  case 1: {
    src.reset(ImageLib::flippedHRaw(src.get()));
  } break;
  case 2: {
    src.reset(ImageLib::flippedVRaw(src.get()));
  } break;
  case 3: {
    src.reset(ImageLib::flippedHRaw(src.get()));
    src.reset(ImageLib::flippedVRaw(src.get()));
  } break;
  case 4: {
    src.reset(ImageLib::rotatedRaw(src.get(), 90));
  } break;
  case 5: {
    src.reset(ImageLib::flippedHRaw(src.get()));
    src.reset(ImageLib::rotatedRaw(src.get(), 90));
  } break;
  case 6: {
    src.reset(ImageLib::flippedVRaw(src.get()));
    src.reset(ImageLib::rotatedRaw(src.get(), 90));
  } break;
  case 7: {
    src.reset(ImageLib::rotatedRaw(src.get(), -90));
  } break;
  default: {
  } break;
  }
  return src;
}
//------------------------------------------------------------------------------
/*

QImage *ImageLib::cropped(QRect newRect, QRect targetRes, bool upscaled) {
    QImage *cropped = new QImage(targetRes.size(), image->format());
    if(upscaled) {
        QImage temp = image->copy(newRect);
        *cropped = temp.scaled(targetRes.size(), Qt::KeepAspectRatioByExpanding,
Qt::SmoothTransformation); QRect target(QPoint(0, 0), targetRes.size());
        target.moveCenter(cropped->rect().center());
        *cropped = cropped->copy(target);
    } else {
        newRect.moveCenter(image->rect().center());
        *cropped = image->copy(newRect);
    }
    return cropped;
}
*/

QImage *ImageLib::scaled(std::shared_ptr<const QImage> source, QSize destSize,
                         ScalingFilter filter) {
  if (!source || destSize.width() > 12288 || destSize.height() > 12288 ||
      (qint64)destSize.width() * destSize.height() > 100000000)
    return new QImage();
  auto scaleTarget = source;
  if (source->format() == QImage::Format_Indexed8) {
    auto newFmt = QImage::Format_RGB32;
    if (source->hasAlphaChannel())
      newFmt = QImage::Format_ARGB32;
    scaleTarget.reset(new QImage(source->convertToFormat(newFmt)));
  }
#ifdef USE_OPENCV
  if (filter > 1 && !QtOcv::isSupported(scaleTarget->format()))
    filter = QI_FILTER_BILINEAR;
#endif
  switch (filter) {
  case QI_FILTER_NEAREST:
    return scaled_Qt(scaleTarget, destSize, false);
  case QI_FILTER_BILINEAR:
    return scaled_Qt(scaleTarget, destSize, true);
#ifdef USE_OPENCV
  case QI_FILTER_CV_BILINEAR_SHARPEN:
    return scaled_CV(scaleTarget, destSize, cv::INTER_LINEAR, 0);
  case QI_FILTER_CV_CUBIC:
    return scaled_CV(scaleTarget, destSize, cv::INTER_CUBIC, 0);
  case QI_FILTER_CV_CUBIC_SHARPEN:
    return scaled_CV(scaleTarget, destSize, cv::INTER_CUBIC, 1);
  case QI_FILTER_CV_LANCZOS:
    return scaled_CV(scaleTarget, destSize, cv::INTER_LANCZOS4, 0);
  case QI_FILTER_CV_AREA:
    return scaled_CV(scaleTarget, destSize, cv::INTER_AREA, 0);
  case QI_FILTER_CV_SMART:
    return scaled_CV_Smart(scaleTarget, destSize);
#endif
  default:
    return scaled_Qt(scaleTarget, destSize, true);
  }
}

QImage *ImageLib::scaled_Qt(std::shared_ptr<const QImage> source,
                            QSize destSize, bool smooth) {
  if (!source)
    return new QImage();
  QImage *dest = new QImage();
  Qt::TransformationMode mode =
      smooth ? Qt::SmoothTransformation : Qt::FastTransformation;
  *dest = source->scaled(destSize.width(), destSize.height(),
                         Qt::IgnoreAspectRatio, mode);
  return dest;
}

#ifdef USE_OPENCV
// this probably leaks, needs checking
QImage *ImageLib::scaled_CV(std::shared_ptr<const QImage> source,
                            QSize destSize, cv::InterpolationFlags filter,
                            int sharpen) {
  if (!source)
    return new QImage();
  QtOcv::MatColorOrder order;
  cv::Mat srcMat = QtOcv::image2Mat_shared(*source.get(), &order);
  cv::Size destSizeCv(destSize.width(), destSize.height());
  QImage *dest = new QImage();
  if (destSize == source->size()) {
    // TODO: should this return a copy?
    // result.reset(new
    // StaticImageContainer(std::make_shared<cv::Mat>(srcMat)));
  } else if (destSize.width() > source.get()->width()) { // upscale
    cv::Mat dstMat(destSizeCv, srcMat.type());
    cv::resize(srcMat, dstMat, destSizeCv, 0, 0, filter);
    *dest = QtOcv::mat2Image(dstMat, order, source->format());
  } else { // downscale
    float scale = (float)destSize.width() / source->width();
    if (scale < 0.5f && filter != cv::INTER_NEAREST) {
      if (filter == cv::INTER_CUBIC)
        sharpen = 1;
      filter = cv::INTER_AREA;
    }
    cv::Mat dstMat(destSizeCv, srcMat.type());
    cv::resize(srcMat, dstMat, destSizeCv, 0, 0, filter);

    if (!sharpen || filter == cv::INTER_NEAREST) {
      *dest = QtOcv::mat2Image(dstMat, order, source->format());
    } else {
      // todo: tweak this
      double amount = 0.25 * sharpen;
      // unsharp mask
      cv::Mat dstMat_sharpened;
      cv::GaussianBlur(dstMat, dstMat_sharpened, cv::Size(0, 0), 2);
      cv::addWeighted(dstMat, 1.0 + amount, dstMat_sharpened, -amount, 0,
                      dstMat_sharpened);
      *dest = QtOcv::mat2Image(dstMat_sharpened, order, source->format());
    }
  }
  return dest;
}

QImage *ImageLib::scaled_CV_Smart(std::shared_ptr<const QImage> source,
                                  QSize destSize) {
  if (!source)
    return new QImage();
  QtOcv::MatColorOrder order;
  cv::Mat srcMat = QtOcv::image2Mat_shared(*source.get(), &order);
  cv::Size destSizeCv(destSize.width(), destSize.height());
  QImage *dest = new QImage();

  bool isUpscaling = (destSize.width() > source->width()) ||
                     (destSize.height() > source->height());

  if (isUpscaling) {
    // --- 1. Bicubic Upscaling ---
    cv::Mat dstMat(destSizeCv, srcMat.type());
    cv::resize(srcMat, dstMat, destSizeCv, 0, 0, cv::INTER_CUBIC);

    // --- 3. Cross-kernel sharpen ---
    // Custom 3x3 cross sharpen kernel: [ 0 -1 0; -1 20 -1; 0 -1 0 ] divisor 16
    cv::Mat kernel = (cv::Mat_<float>(3, 3) <<
         0.0f, -1.0f / 16.0f,  0.0f,
        -1.0f / 16.0f, 20.0f / 16.0f, -1.0f / 16.0f,
         0.0f, -1.0f / 16.0f,  0.0f
    );
    cv::Mat sharpenedMat;
    cv::filter2D(dstMat, sharpenedMat, -1, kernel, cv::Point(-1, -1), 0, cv::BORDER_REPLICATE);

    *dest = QtOcv::mat2Image(sharpenedMat, order, source->format());
  } else {
    // Downscaling: use cv::INTER_AREA
    cv::Mat dstMat(destSizeCv, srcMat.type());
    cv::resize(srcMat, dstMat, destSizeCv, 0, 0, cv::INTER_AREA);

    // Gaussian Unsharp Mask (strength: 0.15) to restore textures without introducing aliasing (no jagged edges)
    cv::Mat dstMat_blurred;
    cv::GaussianBlur(dstMat, dstMat_blurred, cv::Size(0, 0), 2.0);
    cv::Mat sharpenedMat;
    cv::addWeighted(dstMat, 1.15, dstMat_blurred, -0.15, 0.0, sharpenedMat);

    *dest = QtOcv::mat2Image(sharpenedMat, order, source->format());
  }

  return dest;
}
#endif