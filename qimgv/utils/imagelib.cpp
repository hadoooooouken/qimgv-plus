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
#endif
#ifdef USE_CUDA_NPP
  case QI_FILTER_CUDA_CUBIC:
  case QI_FILTER_CUDA_LANCZOS:
  case QI_FILTER_CUDA_ULTRA:
    return scaled_CUDA(scaleTarget, destSize, filter);
#else
  case QI_FILTER_CUDA_CUBIC:
  case QI_FILTER_CUDA_LANCZOS:
  case QI_FILTER_CUDA_ULTRA:
    return scaled_Qt(scaleTarget, destSize, true);
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
#endif

#ifdef USE_CUDA_NPP
#include <QMutex>
#include <QMutexLocker>
static QMutex cudaMutex;

QImage *ImageLib::scaled_CUDA(std::shared_ptr<const QImage> source,
                              QSize destSize, ScalingFilter filter) {
  if (!source || source->isNull())
    return new QImage();

  QMutexLocker locker(&cudaMutex);

  // Cap CUDA scaling at 12288x12288
  if (destSize.width() > 12288 || destSize.height() > 12288) {
#ifdef USE_OPENCV
    return scaled_CV(source, destSize, cv::INTER_LANCZOS4, 0);
#else
    return scaled_Qt(source, destSize, true);
#endif
  }

  // NPP works best with 32-bit formats
  const QImage *srcPtr = source.get();
  QImage converted;
  if (srcPtr->format() != QImage::Format_ARGB32) {
    converted = srcPtr->convertToFormat(QImage::Format_ARGB32);
    srcPtr = &converted;
  }
  QImage *dest = new QImage(destSize, QImage::Format_ARGB32);

  Npp8u *d_src = nullptr, *d_dst = nullptr;
  int srcStep = 0, dstStep = 0;

  // Allocate GPU memory
  d_src = nppiMalloc_8u_C4(srcPtr->width(), srcPtr->height(), &srcStep);
  d_dst = nppiMalloc_8u_C4(destSize.width(), destSize.height(), &dstStep);

  if (!d_src || !d_dst) {
    if (d_src)
      nppiFree(d_src);
    if (d_dst)
      nppiFree(d_dst);
    delete dest;
    // Fallback to CPU if CUDA memory allocation fails
#ifdef USE_OPENCV
    return scaled_CV(source, destSize, cv::INTER_LANCZOS4, 0);
#else
    return scaled_Qt(source, destSize, true);
#endif
  }

  // Copy to GPU
  cudaMemcpy2D(d_src, srcStep, srcPtr->bits(), srcPtr->bytesPerLine(),
               srcPtr->width() * 4, srcPtr->height(), cudaMemcpyHostToDevice);

  NppiSize srcSize = {srcPtr->width(), srcPtr->height()};
  NppiRect srcRect = {0, 0, srcPtr->width(), srcPtr->height()};
  NppiSize dstSizeNpp = {destSize.width(), destSize.height()};
  NppiRect dstRect = {0, 0, destSize.width(), destSize.height()};

  // Scale on GPU using the modern Context-aware function
  NppStreamContext nppStreamCtx;
  memset(&nppStreamCtx, 0, sizeof(nppStreamCtx));

  // Fill context with current device properties
  cudaGetDevice(&nppStreamCtx.nCudaDeviceId);
  cudaDeviceProp props;
  cudaGetDeviceProperties(&props, nppStreamCtx.nCudaDeviceId);

  nppStreamCtx.hStream = 0; // Default stream
  nppStreamCtx.nMultiProcessorCount = props.multiProcessorCount;
  nppStreamCtx.nMaxThreadsPerMultiProcessor = props.maxThreadsPerMultiProcessor;
  nppStreamCtx.nMaxThreadsPerBlock = props.maxThreadsPerBlock;
  nppStreamCtx.nSharedMemPerBlock = props.sharedMemPerBlock;

  cudaDeviceGetAttribute(&nppStreamCtx.nCudaDevAttrComputeCapabilityMajor,
                         cudaDevAttrComputeCapabilityMajor,
                         nppStreamCtx.nCudaDeviceId);
  cudaDeviceGetAttribute(&nppStreamCtx.nCudaDevAttrComputeCapabilityMinor,
                         cudaDevAttrComputeCapabilityMinor,
                         nppStreamCtx.nCudaDeviceId);
  cudaStreamGetFlags(nppStreamCtx.hStream, &nppStreamCtx.nStreamFlags);

  // Determine interpolation mode
  NppiInterpolationMode interMode = NPPI_INTER_CUBIC;
  if (filter == QI_FILTER_CUDA_LANCZOS)
    interMode = NPPI_INTER_LANCZOS;

  // Correctly detect upscaling (both dimensions)
  bool isUpscaling = (destSize.width() > source->width()) ||
                     (destSize.height() > source->height());

  if (filter == QI_FILTER_CUDA_ULTRA) {
    if (isUpscaling) {
      // --- 1. Denoise (Median 3x3) on SOURCE ---
      NppiSize maskSize = {3, 3};
      NppiPoint anchor = {1, 1};
      Npp32u nBufferSize = 0;
      nppiFilterMedianBorderGetBufferSize_8u_C4R_Ctx(
          srcSize, maskSize, &nBufferSize, NPP_BORDER_REPLICATE, nppStreamCtx);

      Npp8u *pBuffer = nullptr;
      cudaMalloc(&pBuffer, nBufferSize);
      Npp8u *d_tmp = nullptr;
      int tmpStep = 0;
      d_tmp = nppiMalloc_8u_C4(srcPtr->width(), srcPtr->height(), &tmpStep);

      if (d_tmp && pBuffer) {
        nppiFilterMedianBorder_8u_C4R_Ctx(
            d_src, srcStep, srcSize, {0, 0}, d_tmp, tmpStep, srcSize, maskSize,
            anchor, pBuffer, NPP_BORDER_REPLICATE, nppStreamCtx);
        cudaMemcpy2D(d_src, srcStep, d_tmp, tmpStep, srcPtr->width() * 4,
                     srcPtr->height(), cudaMemcpyDeviceToDevice);
      }
      if (d_tmp)
        nppiFree(d_tmp);
      if (pBuffer)
        cudaFree(pBuffer);

      interMode = NPPI_INTER_LANCZOS;
    } else {
      // Downscaling: use high-quality SUPER interpolation (analogue of OpenCV
      // INTER_AREA)
      interMode = NPPI_INTER_SUPER;
    }
  }

  NppStatus resStatus =
      nppiResize_8u_C4R_Ctx(d_src, srcStep, srcSize, srcRect, d_dst, dstStep,
                            dstSizeNpp, dstRect, interMode, nppStreamCtx);

  if (resStatus != NPP_SUCCESS) {
    // Resize failed – fallback to CPU
    nppiFree(d_src);
    nppiFree(d_dst);
    delete dest;
#ifdef USE_OPENCV
    return scaled_CV(source, destSize, cv::INTER_LANCZOS4, 0);
#else
    return scaled_Qt(source, destSize, true);
#endif
  }

  if (filter == QI_FILTER_CUDA_ULTRA && isUpscaling) {
    // --- 2. Final Sharpen only when UPSCALING ---
    // Custom 3x3 cross sharpen kernel: [ 0 -1 0; -1 20 -1; 0 -1 0 ] divisor 16
    // This gives a softer 0.25 strength sharpen.
    Npp32s h_kernel[9] = {0, -1, 0, -1, 20, -1, 0, -1, 0};
    Npp32s *d_kernel = nullptr;
    cudaMalloc(&d_kernel, 9 * sizeof(Npp32s));
    cudaMemcpy(d_kernel, h_kernel, 9 * sizeof(Npp32s), cudaMemcpyHostToDevice);

    NppiSize maskSize = {3, 3};
    NppiPoint anchor = {1, 1};

    if (d_kernel) {
      // Allocate temporary buffer AND capture its actual step
      int sharpenStep = 0;
      Npp8u *d_sharpened =
          nppiMalloc_8u_C4(dstSizeNpp.width, dstSizeNpp.height, &sharpenStep);
      if (d_sharpened) {
        nppiFilterBorder_8u_C4R_Ctx(
            d_dst, dstStep, dstSizeNpp, {0, 0}, d_sharpened, sharpenStep,
            dstSizeNpp, // use the correct step
            d_kernel, maskSize, anchor, 16, NPP_BORDER_REPLICATE, nppStreamCtx);

        cudaMemcpy2D(d_dst, dstStep, d_sharpened, sharpenStep,
                     dstSizeNpp.width * 4, dstSizeNpp.height,
                     cudaMemcpyDeviceToDevice);
        nppiFree(d_sharpened);
      }
      cudaFree(d_kernel);
    }
  }

  // Copy back to CPU
  cudaMemcpy2D(dest->bits(), dest->bytesPerLine(), d_dst, dstStep,
               destSize.width() * 4, destSize.height(), cudaMemcpyDeviceToHost);

  // Free GPU memory
  nppiFree(d_src);
  nppiFree(d_dst);

  return dest;
}
#endif