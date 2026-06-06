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
  int maxDim = 12288;
  qint64 maxPixels = 100000000;
#ifdef USE_UPSCAYL
  if (settings->useUpscayl()) {
    maxDim = 16384;         // Cap to GPU max texture size / GDI memory safety limit (16384)
    maxPixels = 268435456;  // Cap to 256 Megapixels (~1.07 GB RAM) to prevent drawing allocations crashes
  }
#endif

  if (!source || destSize.width() > maxDim || destSize.height() > maxDim ||
      (qint64)destSize.width() * destSize.height() > maxPixels)
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
    cv::Mat kernel =
        (cv::Mat_<float>(3, 3) << 0.0f, -1.0f / 16.0f, 0.0f, -1.0f / 16.0f,
         20.0f / 16.0f, -1.0f / 16.0f, 0.0f, -1.0f / 16.0f, 0.0f);
    cv::Mat sharpenedMat;
    cv::filter2D(dstMat, sharpenedMat, -1, kernel, cv::Point(-1, -1), 0,
                 cv::BORDER_REPLICATE);

    *dest = QtOcv::mat2Image(sharpenedMat, order, source->format());
  } else {
    // Downscaling: use cv::INTER_AREA
    cv::Mat dstMat(destSizeCv, srcMat.type());
    cv::resize(srcMat, dstMat, destSizeCv, 0, 0, cv::INTER_AREA);

    // Gaussian Unsharp Mask (strength: 0.15) to restore textures without
    // introducing aliasing (no jagged edges)
    cv::Mat dstMat_blurred;
    cv::GaussianBlur(dstMat, dstMat_blurred, cv::Size(0, 0), 2.0);
    cv::Mat sharpenedMat;
    cv::addWeighted(dstMat, 1.15, dstMat_blurred, -0.15, 0.0, sharpenedMat);

    *dest = QtOcv::mat2Image(sharpenedMat, order, source->format());
  }

  return dest;
}
#endif

QImage *ImageLib::applyColorAdjustments(std::shared_ptr<const QImage> source, float brightness, float contrast, float saturation, float hue, float exposure, float temperature, float tint) {
  if (!source)
    return new QImage();

  QImage *dst = new QImage(source->convertToFormat(QImage::Format_ARGB32));
  float hueRad = hue * 3.14159265358979323846f / 180.0f;
  float cosAngle = std::cos(hueRad);
  float sinAngle = std::sin(hueRad);
  float k = 0.57735f;

  for (int y = 0; y < dst->height(); ++y) {
    QRgb *line = reinterpret_cast<QRgb*>(dst->scanLine(y));
    for (int x = 0; x < dst->width(); ++x) {
      QRgb pixel = line[x];
      int a = qAlpha(pixel);
      float r = qRed(pixel) / 255.0f;
      float g = qGreen(pixel) / 255.0f;
      float b = qBlue(pixel) / 255.0f;

      // 1. White balance (Temperature & Tint)
      if (std::abs(temperature) > 0.001f || std::abs(tint) > 0.001f) {
        r *= (1.0f + temperature + tint * 0.5f);
        g *= (1.0f - tint);
        b *= (1.0f - temperature + tint * 0.5f);
      }

      // 2. Exposure
      if (std::abs(exposure) > 0.001f) {
        float factor = std::pow(2.0f, exposure);
        r *= factor;
        g *= factor;
        b *= factor;
      }

      // 3. Hue rotate
      if (std::abs(hue) > 0.001f) {
        float cx = k * b - k * g;
        float cy = k * r - k * b;
        float cz = k * g - k * r;
        float dot = k * r + k * g + k * b;

        float nr = r * cosAngle + cx * sinAngle + k * dot * (1.0f - cosAngle);
        float ng = g * cosAngle + cy * sinAngle + k * dot * (1.0f - cosAngle);
        float nb = b * cosAngle + cz * sinAngle + k * dot * (1.0f - cosAngle);
        r = nr; g = ng; b = nb;
      }

      // 4. Saturation
      if (std::abs(saturation - 1.0f) > 0.001f) {
        float gray = 0.2126f * r + 0.7152f * g + 0.0722f * b;
        r = gray + (r - gray) * saturation;
        g = gray + (g - gray) * saturation;
        b = gray + (b - gray) * saturation;
      }

      // 5. Brightness
      r += brightness;
      g += brightness;
      b += brightness;

      // 6. Contrast
      r = (r - 0.5f) * contrast + 0.5f;
      g = (g - 0.5f) * contrast + 0.5f;
      b = (b - 0.5f) * contrast + 0.5f;

      // Clamp and convert back
      int nr = qBound(0, static_cast<int>(r * 255.0f + 0.5f), 255);
      int ng = qBound(0, static_cast<int>(g * 255.0f + 0.5f), 255);
      int nb = qBound(0, static_cast<int>(b * 255.0f + 0.5f), 255);

      line[x] = qRgba(nr, ng, nb, a);
    }
  }

  return dst;
}

