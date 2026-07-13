#include "imagelib.h"
#include "settings.h"
#include <thread>
#include <vector>
#include <cmath>
#include <algorithm>
#include <ranges>
#include <span>
#include <immintrin.h>
#include <QThreadPool>
#include <QRunnable>
#include <QSemaphore>
#include <QCoreApplication>
#include <functional>
#include <QIcon>
#include <QPixmap>

namespace {

// Unsharp mask constants
constexpr float kUnsharpSharpStrength = 1.15f;
constexpr float kUnsharpBlurStrength = 0.15f;
constexpr float kColorMax = 255.0f;
constexpr float kRoundOffset = 0.5f;

// Sliding window size
constexpr int kSlidingWindowSize = 3;

// Gaussian blur constants
constexpr int kGaussianKernelSize = 9;
constexpr int kGaussianHalfWidth = 4;
constexpr float kGaussianWeights[kGaussianKernelSize] = {
  0.02763f, 0.06628f, 0.12384f, 0.18017f, 0.20416f, 0.18017f, 0.12384f, 0.06628f, 0.02763f
};

// Task runner helper
class ScalerTask : public QRunnable {
    std::function<void()> m_func;
public:
    ScalerTask(std::function<void()> func) : m_func(func) {
        setAutoDelete(true);
    }
    void run() override {
        m_func();
    }
};

// Returns a writable span over one scan-line of a 32-bpp image.
inline std::span<uint32_t> scanlineSpan(QImage &img, int y) {
    return { reinterpret_cast<uint32_t *>(img.scanLine(y)),
             static_cast<size_t>(img.width()) };
}

// Returns a read-only span.
inline std::span<const uint32_t> constScanlineSpan(const QImage &img, int y) {
    return { reinterpret_cast<const uint32_t *>(img.constScanLine(y)),
             static_cast<size_t>(img.width()) };
}

// Thread pool helper childed to QCoreApplication to avoid static destruction issues
QThreadPool* getScalingThreadPool() {
    static QThreadPool* pool = []() {
        QThreadPool* p = new QThreadPool(QCoreApplication::instance());
        int threads = std::thread::hardware_concurrency();
        if (threads <= 0) threads = 4;
        p->setMaxThreadCount(threads);
        return p;
    }();
    return pool;
}
}

void ImageLib::recolor(QPixmap &pixmap, QColor color) {
  QPainter p(&pixmap);
  p.setCompositionMode(QPainter::CompositionMode_SourceIn);
  p.setBrush(color);
  p.setPen(color);
  p.drawRect(pixmap.rect());
}

QImage ImageLib::rotatedRaw(const QImage *src, int grad) {
  if (!src)
    return QImage();
  QImage img;
  QTransform transform;
  transform.rotate(grad);
  img = src->transformed(transform, Qt::SmoothTransformation);
  return img;
}
//------------------------------------------------------------------------------
QImage ImageLib::rotated(std::shared_ptr<const QImage> src, int grad) {
  return rotatedRaw(src.get(), grad);
}
//------------------------------------------------------------------------------
QImage ImageLib::croppedRaw(const QImage *src, QRect newRect) {
  if (src && src->rect().contains(newRect, false)) {
    return src->copy(newRect);
  } else {
    return QImage();
  }
}
//------------------------------------------------------------------------------
QImage ImageLib::cropped(std::shared_ptr<const QImage> src, QRect newRect) {
  return croppedRaw(src.get(), newRect);
}
//------------------------------------------------------------------------------
QImage ImageLib::flippedHRaw(const QImage *src) {
  if (!src)
    return QImage();
  else
    return src->mirrored(true, false);
}
//------------------------------------------------------------------------------
QImage ImageLib::flippedH(std::shared_ptr<const QImage> src) {
  return flippedHRaw(src.get());
}
//------------------------------------------------------------------------------
QImage ImageLib::flippedVRaw(const QImage *src) {
  if (!src)
    return QImage();
  else
    return src->mirrored(false, true);
}
//------------------------------------------------------------------------------
QImage ImageLib::flippedV(std::shared_ptr<const QImage> src) {
  return flippedVRaw(src.get());
}
//------------------------------------------------------------------------------
std::unique_ptr<const QImage>
ImageLib::exifRotated(std::unique_ptr<const QImage> src, int orientation) {
  switch (orientation) {
  case 1: {
    src.reset(new QImage(ImageLib::flippedHRaw(src.get())));
  } break;
  case 2: {
    src.reset(new QImage(ImageLib::flippedVRaw(src.get())));
  } break;
  case 3: {
    src.reset(new QImage(ImageLib::flippedHRaw(src.get())));
    src.reset(new QImage(ImageLib::flippedVRaw(src.get())));
  } break;
  case 4: {
    src.reset(new QImage(ImageLib::rotatedRaw(src.get(), 90)));
  } break;
  case 5: {
    src.reset(new QImage(ImageLib::flippedHRaw(src.get())));
    src.reset(new QImage(ImageLib::rotatedRaw(src.get(), 90)));
  } break;
  case 6: {
    src.reset(new QImage(ImageLib::flippedVRaw(src.get())));
    src.reset(new QImage(ImageLib::rotatedRaw(src.get(), 90)));
  } break;
  case 7: {
    src.reset(new QImage(ImageLib::rotatedRaw(src.get(), -90)));
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
    src.reset(new QImage(ImageLib::flippedHRaw(src.get())));
  } break;
  case 2: {
    src.reset(new QImage(ImageLib::flippedVRaw(src.get())));
  } break;
  case 3: {
    src.reset(new QImage(ImageLib::flippedHRaw(src.get())));
    src.reset(new QImage(ImageLib::flippedVRaw(src.get())));
  } break;
  case 4: {
    src.reset(new QImage(ImageLib::rotatedRaw(src.get(), 90)));
  } break;
  case 5: {
    src.reset(new QImage(ImageLib::flippedHRaw(src.get())));
    src.reset(new QImage(ImageLib::rotatedRaw(src.get(), 90)));
  } break;
  case 6: {
    src.reset(new QImage(ImageLib::flippedVRaw(src.get())));
    src.reset(new QImage(ImageLib::rotatedRaw(src.get(), 90)));
  } break;
  case 7: {
    src.reset(new QImage(ImageLib::rotatedRaw(src.get(), -90)));
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

QImage ImageLib::scaled(std::shared_ptr<const QImage> source, QSize destSize,
                        ScalingFilter filter) {
  int maxDim = 12288;
  qint64 maxPixels = 100000000;
#ifdef USE_UPSCAYL
  if (settings->useUpscayl() || settings->resizeUseUpscayl()) {
    maxDim = 16384;         // Cap to GPU max texture size / GDI memory safety limit (16384)
    maxPixels = 268435456;  // Cap to 256 Megapixels (~1.07 GB RAM) to prevent drawing allocations crashes
  }
#endif

  if (!source || destSize.width() > maxDim || destSize.height() > maxDim ||
      (qint64)destSize.width() * destSize.height() > maxPixels)
    return QImage();
  auto scaleTarget = source;
  if (source->format() == QImage::Format_Indexed8) {
    auto newFmt = QImage::Format_RGB32;
    if (source->hasAlphaChannel())
      newFmt = QImage::Format_ARGB32;
    scaleTarget.reset(new QImage(source->convertToFormat(newFmt)));
  }
  switch (filter) {
  case QI_FILTER_NEAREST:
    return scaled_Qt(scaleTarget, destSize, false);
  case QI_FILTER_BILINEAR:
    return scaled_Qt(scaleTarget, destSize, true);
  case QI_FILTER_SMART:
    return scaled_Smart(scaleTarget, destSize);
  default:
    return scaled_Qt(scaleTarget, destSize, true);
  }
}

QImage ImageLib::scaled_Qt(std::shared_ptr<const QImage> source,
                           QSize destSize, bool smooth) {
  if (!source)
    return QImage();
  QImage dest;
  Qt::TransformationMode mode =
      smooth ? Qt::SmoothTransformation : Qt::FastTransformation;
  dest = source->scaled(destSize.width(), destSize.height(),
                        Qt::IgnoreAspectRatio, mode);
  return dest;
}



QImage ImageLib::scaled_Smart(std::shared_ptr<const QImage> source,
                               QSize destSize) {
  if (!source || source->isNull())
    return QImage();

  int W_src = source->width();
  int H_src = source->height();
  int W_dst = destSize.width();
  int H_dst = destSize.height();

  if (W_dst <= 0 || H_dst <= 0)
    return QImage();

  // Convert source to Format_ARGB32 or Format_RGB32 if it's not already 32-bit
  QImage srcImg = *source.get();
  if (srcImg.format() != QImage::Format_ARGB32 && srcImg.format() != QImage::Format_RGB32) {
    srcImg = srcImg.convertToFormat(QImage::Format_ARGB32);
  }

  bool isUpscaling = (W_dst > W_src) || (H_dst > H_src);

  if (isUpscaling) {
    // --- 1. Custom Separable 1D Bicubic Scaling ---
    double s_x = (double)W_src / W_dst;
    double s_y = (double)H_src / H_dst;

    // Precompute horizontal weights and clamp indices
    struct HorizWeight {
      int x0, x1, x2, x3;
      float w0, w1, w2, w3;
    };
    std::vector<HorizWeight> hWeights(W_dst);
    for (int x = 0; x < W_dst; ++x) {
      double u = x * s_x;
      int xin = std::floor(u);
      float dx = u - xin;
      hWeights[x].x0 = std::clamp(xin - 1, 0, W_src - 1);
      hWeights[x].x1 = std::clamp(xin,     0, W_src - 1);
      hWeights[x].x2 = std::clamp(xin + 1, 0, W_src - 1);
      hWeights[x].x3 = std::clamp(xin + 2, 0, W_src - 1);
      hWeights[x].w0 = BicubicWeights::w0(dx);
      hWeights[x].w1 = BicubicWeights::w1(dx);
      hWeights[x].w2 = BicubicWeights::w2(dx);
      hWeights[x].w3 = BicubicWeights::w3(dx);
    }

    // Precompute vertical weights and clamp indices
    struct VertWeight {
      int y0, y1, y2, y3;
      float w0, w1, w2, w3;
    };
    std::vector<VertWeight> vWeights(H_dst);
    for (int y = 0; y < H_dst; ++y) {
      double v = y * s_y;
      int yin = std::floor(v);
      float dy = v - yin;
      vWeights[y].y0 = std::clamp(yin - 1, 0, H_src - 1);
      vWeights[y].y1 = std::clamp(yin,     0, H_src - 1);
      vWeights[y].y2 = std::clamp(yin + 1, 0, H_src - 1);
      vWeights[y].y3 = std::clamp(yin + 2, 0, H_src - 1);
      vWeights[y].w0 = BicubicWeights::w0(dy);
      vWeights[y].w1 = BicubicWeights::w1(dy);
      vWeights[y].w2 = BicubicWeights::w2(dy);
      vWeights[y].w3 = BicubicWeights::w3(dy);
    }

    // Horizontal pass: W_src x H_src -> W_dst x H_src
    QImage interImg(W_dst, H_src, srcImg.format());
    
    // Determine thread count
    int numThreads = std::thread::hardware_concurrency();
    if (numThreads <= 0) numThreads = 4;

    QThreadPool* pool = getScalingThreadPool();

    {
      QSemaphore semaphore;
      int rowsPerThread = H_src / numThreads;
      if (rowsPerThread == 0) rowsPerThread = 1;

      int numTasks = 0;
      for (int i = 0; i < numThreads; ++i) {
        int y_start = i * rowsPerThread;
        int y_end = (i == numThreads - 1) ? H_src : (i + 1) * rowsPerThread;
        if (y_start >= H_src) break;

        numTasks++;
        pool->start(new ScalerTask([&srcImg, &interImg, &hWeights, W_dst, y_start, y_end, &semaphore]() {
          for (int y = y_start; y < y_end; ++y) {
            auto srcRow   = constScanlineSpan(srcImg, y);
            auto interRow = scanlineSpan(interImg, y);

            int x = 0;
            int limit = W_dst - 1;
            for (; x < limit; x += 2) {
              const auto& hw_a = hWeights[x];
              const auto& hw_b = hWeights[x + 1];

              uint32_t p0_a = srcRow[hw_a.x0];
              uint32_t p1_a = srcRow[hw_a.x1];
              uint32_t p2_a = srcRow[hw_a.x2];
              uint32_t p3_a = srcRow[hw_a.x3];

              uint32_t p0_b = srcRow[hw_b.x0];
              uint32_t p1_b = srcRow[hw_b.x1];
              uint32_t p2_b = srcRow[hw_b.x2];
              uint32_t p3_b = srcRow[hw_b.x3];

              __m256 v_p0 = _mm256_cvtepi32_ps(_mm256_cvtepu8_epi32(_mm_set_epi32(0, 0, p0_b, p0_a)));
              __m256 v_p1 = _mm256_cvtepi32_ps(_mm256_cvtepu8_epi32(_mm_set_epi32(0, 0, p1_b, p1_a)));
              __m256 v_p2 = _mm256_cvtepi32_ps(_mm256_cvtepu8_epi32(_mm_set_epi32(0, 0, p2_b, p2_a)));
              __m256 v_p3 = _mm256_cvtepi32_ps(_mm256_cvtepu8_epi32(_mm_set_epi32(0, 0, p3_b, p3_a)));

              __m256 w0 = _mm256_setr_ps(hw_a.w0, hw_a.w0, hw_a.w0, hw_a.w0, hw_b.w0, hw_b.w0, hw_b.w0, hw_b.w0);
              __m256 w1 = _mm256_setr_ps(hw_a.w1, hw_a.w1, hw_a.w1, hw_a.w1, hw_b.w1, hw_b.w1, hw_b.w1, hw_b.w1);
              __m256 w2 = _mm256_setr_ps(hw_a.w2, hw_a.w2, hw_a.w2, hw_a.w2, hw_b.w2, hw_b.w2, hw_b.w2, hw_b.w2);
              __m256 w3 = _mm256_setr_ps(hw_a.w3, hw_a.w3, hw_a.w3, hw_a.w3, hw_b.w3, hw_b.w3, hw_b.w3, hw_b.w3);

              __m256 res = _mm256_mul_ps(v_p0, w0);
              res = _mm256_fmadd_ps(v_p1, w1, res);
              res = _mm256_fmadd_ps(v_p2, w2, res);
              res = _mm256_fmadd_ps(v_p3, w3, res);

              __m256 rounded = _mm256_add_ps(res, _mm256_set1_ps(kRoundOffset));
              rounded = _mm256_min_ps(_mm256_max_ps(rounded, _mm256_setzero_ps()), _mm256_set1_ps(kColorMax));

              __m256i res_i = _mm256_cvtps_epi32(rounded);

              __m256i packed_16 = _mm256_packus_epi32(res_i, res_i);
              __m256i packed_8 = _mm256_packus_epi16(packed_16, packed_16);

              interRow[x] = _mm_cvtsi128_si32(_mm256_castsi256_si128(packed_8));
              interRow[x + 1] = _mm_cvtsi128_si32(_mm256_extractf128_si256(packed_8, 1));
            }
            if (x < W_dst) {
              const auto& hw = hWeights[x];
              uint32_t p0 = srcRow[hw.x0];
              uint32_t p1 = srcRow[hw.x1];
              uint32_t p2 = srcRow[hw.x2];
              uint32_t p3 = srcRow[hw.x3];

              float b = hw.w0 * (p0 & 0xFF) + hw.w1 * (p1 & 0xFF) + hw.w2 * (p2 & 0xFF) + hw.w3 * (p3 & 0xFF);
              float g = hw.w0 * ((p0 >> 8) & 0xFF) + hw.w1 * ((p1 >> 8) & 0xFF) + hw.w2 * ((p2 >> 8) & 0xFF) + hw.w3 * ((p3 >> 8) & 0xFF);
              float r = hw.w0 * ((p0 >> 16) & 0xFF) + hw.w1 * ((p1 >> 16) & 0xFF) + hw.w2 * ((p2 >> 16) & 0xFF) + hw.w3 * ((p3 >> 16) & 0xFF);
              float a = hw.w0 * ((p0 >> 24) & 0xFF) + hw.w1 * ((p1 >> 24) & 0xFF) + hw.w2 * ((p2 >> 24) & 0xFF) + hw.w3 * ((p3 >> 24) & 0xFF);

              uint8_t ub = std::clamp(b + kRoundOffset, 0.0f, kColorMax);
              uint8_t ug = std::clamp(g + kRoundOffset, 0.0f, kColorMax);
              uint8_t ur = std::clamp(r + kRoundOffset, 0.0f, kColorMax);
              uint8_t ua = std::clamp(a + kRoundOffset, 0.0f, kColorMax);

              interRow[x] = (ua << 24) | (ur << 16) | (ug << 8) | ub;
            }
          }
          semaphore.release(1);
        }));
      }
      semaphore.acquire(numTasks);
    }

    // Vertical pass + Cross-kernel Sharpening combined in one step
    QImage destImg(W_dst, H_dst, srcImg.format());

    {
      QSemaphore semaphore;
      int rowsPerThread = H_dst / numThreads;
      if (rowsPerThread == 0) rowsPerThread = 1;

      int numTasks = 0;
      for (int i = 0; i < numThreads; ++i) {
        int y_start = i * rowsPerThread;
        int y_end = (i == numThreads - 1) ? H_dst : (i + 1) * rowsPerThread;
        if (y_start >= H_dst) break;

        numTasks++;
        pool->start(new ScalerTask([&interImg, &destImg, &vWeights, W_dst, H_dst, y_start, y_end, &semaphore]() {
          // Thread-local sliding window buffer
          std::vector<std::vector<uint32_t>> rowBuffers(kSlidingWindowSize, std::vector<uint32_t>(W_dst));

          auto fillRowBuffer = [&](int yd, int bufIdx) {
            int yd_clamped = std::clamp(yd, 0, H_dst - 1);
            const auto& vw = vWeights[yd_clamped];
            auto r0 = constScanlineSpan(interImg, vw.y0);
            auto r1 = constScanlineSpan(interImg, vw.y1);
            auto r2 = constScanlineSpan(interImg, vw.y2);
            auto r3 = constScanlineSpan(interImg, vw.y3);
            uint32_t* out = rowBuffers[bufIdx].data();

            __m256 w0 = _mm256_set1_ps(vw.w0);
            __m256 w1 = _mm256_set1_ps(vw.w1);
            __m256 w2 = _mm256_set1_ps(vw.w2);
            __m256 w3 = _mm256_set1_ps(vw.w3);

            int x = 0;
            int limit = W_dst - 1;
            for (; x < limit; x += 2) {
              uint32_t p0_a = r0[x];
              uint32_t p0_b = r0[x + 1];
              uint32_t p1_a = r1[x];
              uint32_t p1_b = r1[x + 1];
              uint32_t p2_a = r2[x];
              uint32_t p2_b = r2[x + 1];
              uint32_t p3_a = r3[x];
              uint32_t p3_b = r3[x + 1];

              __m256 v_p0 = _mm256_cvtepi32_ps(_mm256_cvtepu8_epi32(_mm_set_epi32(0, 0, p0_b, p0_a)));
              __m256 v_p1 = _mm256_cvtepi32_ps(_mm256_cvtepu8_epi32(_mm_set_epi32(0, 0, p1_b, p1_a)));
              __m256 v_p2 = _mm256_cvtepi32_ps(_mm256_cvtepu8_epi32(_mm_set_epi32(0, 0, p2_b, p2_a)));
              __m256 v_p3 = _mm256_cvtepi32_ps(_mm256_cvtepu8_epi32(_mm_set_epi32(0, 0, p3_b, p3_a)));

              __m256 res = _mm256_mul_ps(v_p0, w0);
              res = _mm256_fmadd_ps(v_p1, w1, res);
              res = _mm256_fmadd_ps(v_p2, w2, res);
              res = _mm256_fmadd_ps(v_p3, w3, res);

              __m256 rounded = _mm256_add_ps(res, _mm256_set1_ps(kRoundOffset));
              rounded = _mm256_min_ps(_mm256_max_ps(rounded, _mm256_setzero_ps()), _mm256_set1_ps(kColorMax));

              __m256i res_i = _mm256_cvtps_epi32(rounded);

              __m256i packed_16 = _mm256_packus_epi32(res_i, res_i);
              __m256i packed_8 = _mm256_packus_epi16(packed_16, packed_16);

              out[x] = _mm_cvtsi128_si32(_mm256_castsi256_si128(packed_8));
              out[x + 1] = _mm_cvtsi128_si32(_mm256_extractf128_si256(packed_8, 1));
            }
            if (x < W_dst) {
              uint32_t p0 = r0[x];
              uint32_t p1 = r1[x];
              uint32_t p2 = r2[x];
              uint32_t p3 = r3[x];

              float b = vw.w0 * (p0 & 0xFF) + vw.w1 * (p1 & 0xFF) + vw.w2 * (p2 & 0xFF) + vw.w3 * (p3 & 0xFF);
              float g = vw.w0 * ((p0 >> 8) & 0xFF) + vw.w1 * ((p1 >> 8) & 0xFF) + vw.w2 * ((p2 >> 8) & 0xFF) + vw.w3 * ((p3 >> 8) & 0xFF);
              float r = vw.w0 * ((p0 >> 16) & 0xFF) + vw.w1 * ((p1 >> 16) & 0xFF) + vw.w2 * ((p2 >> 16) & 0xFF) + vw.w3 * ((p3 >> 16) & 0xFF);
              float a = vw.w0 * ((p0 >> 24) & 0xFF) + vw.w1 * ((p1 >> 24) & 0xFF) + vw.w2 * ((p2 >> 24) & 0xFF) + vw.w3 * ((p3 >> 24) & 0xFF);

              uint8_t ub = std::clamp(b + kRoundOffset, 0.0f, kColorMax);
              uint8_t ug = std::clamp(g + kRoundOffset, 0.0f, kColorMax);
              uint8_t ur = std::clamp(r + kRoundOffset, 0.0f, kColorMax);
              uint8_t ua = std::clamp(a + kRoundOffset, 0.0f, kColorMax);

              out[x] = (ua << 24) | (ur << 16) | (ug << 8) | ub;
            }
          };

          // Prime window
          fillRowBuffer(y_start - 1, 0);
          fillRowBuffer(y_start, 1);
          fillRowBuffer(y_start + 1, 2);

          for (int y = y_start; y < y_end; ++y) {
            auto dstRow = scanlineSpan(destImg, y);

            int idxT = (y - y_start) % kSlidingWindowSize;
            int idxC = (y - y_start + 1) % kSlidingWindowSize;
            int idxB = (y - y_start + 2) % kSlidingWindowSize;

            const uint32_t* rowT = rowBuffers[idxT].data();
            const uint32_t* rowC = rowBuffers[idxC].data();
            const uint32_t* rowB = rowBuffers[idxB].data();

            // Border x = 0
            {
              uint32_t c = rowC[0];
              uint32_t t = rowT[0];
              uint32_t b = rowB[0];
              uint32_t l = c;
              uint32_t r = rowC[1];
              uint32_t res = 0;
              for (int ch = 0; ch < 4; ++ch) {
                int valC = (c >> (ch * 8)) & 0xFF;
                int valT = (t >> (ch * 8)) & 0xFF;
                int valB = (b >> (ch * 8)) & 0xFF;
                int valL = (l >> (ch * 8)) & 0xFF;
                int valR = (r >> (ch * 8)) & 0xFF;
                int sh = valC + ((4 * valC - valT - valB - valL - valR) >> 4);
                res |= (uint32_t)std::clamp(sh, 0, 255) << (ch * 8);
              }
              dstRow[0] = res;
            }

            int x = 1;
            int avx_end = W_dst - 8;
            for (; x < avx_end; x += 8) {
              __m256i reg_C = _mm256_loadu_si256((const __m256i*)&rowC[x]);
              __m256i reg_T = _mm256_loadu_si256((const __m256i*)&rowT[x]);
              __m256i reg_B = _mm256_loadu_si256((const __m256i*)&rowB[x]);
              __m256i reg_L = _mm256_loadu_si256((const __m256i*)&rowC[x - 1]);
              __m256i reg_R = _mm256_loadu_si256((const __m256i*)&rowC[x + 1]);

              __m256i zero = _mm256_setzero_si256();

              __m256i C_lo = _mm256_unpacklo_epi8(reg_C, zero);
              __m256i C_hi = _mm256_unpackhi_epi8(reg_C, zero);

              __m256i T_lo = _mm256_unpacklo_epi8(reg_T, zero);
              __m256i T_hi = _mm256_unpackhi_epi8(reg_T, zero);

              __m256i B_lo = _mm256_unpacklo_epi8(reg_B, zero);
              __m256i B_hi = _mm256_unpackhi_epi8(reg_B, zero);

              __m256i L_lo = _mm256_unpacklo_epi8(reg_L, zero);
              __m256i L_hi = _mm256_unpackhi_epi8(reg_L, zero);

              __m256i R_lo = _mm256_unpacklo_epi8(reg_R, zero);
              __m256i R_hi = _mm256_unpackhi_epi8(reg_R, zero);

              // Parallelizing additions to reduce latency
              __m256i TB_lo = _mm256_add_epi16(T_lo, B_lo);
              __m256i LR_lo = _mm256_add_epi16(L_lo, R_lo);
              __m256i env_lo = _mm256_add_epi16(TB_lo, LR_lo);
              __m256i C4_lo = _mm256_slli_epi16(C_lo, 2);
              __m256i sum_lo = _mm256_sub_epi16(C4_lo, env_lo);
              __m256i diff_lo = _mm256_srai_epi16(sum_lo, 4);
              __m256i res_lo = _mm256_add_epi16(C_lo, diff_lo);

              __m256i TB_hi = _mm256_add_epi16(T_hi, B_hi);
              __m256i LR_hi = _mm256_add_epi16(L_hi, R_hi);
              __m256i env_hi = _mm256_add_epi16(TB_hi, LR_hi);
              __m256i C4_hi = _mm256_slli_epi16(C_hi, 2);
              __m256i sum_hi = _mm256_sub_epi16(C4_hi, env_hi);
              __m256i diff_hi = _mm256_srai_epi16(sum_hi, 4);
              __m256i res_hi = _mm256_add_epi16(C_hi, diff_hi);

              __m256i res_pack = _mm256_packus_epi16(res_lo, res_hi);
              _mm256_storeu_si256((__m256i*)&dstRow[x], res_pack);
            }

            // Scalar fallback
            for (; x < W_dst; ++x) {
              uint32_t c = rowC[x];
              uint32_t t = rowT[x];
              uint32_t b = rowB[x];
              uint32_t l = rowC[x - 1];
              uint32_t r = (x == W_dst - 1) ? c : rowC[x + 1];
              uint32_t res = 0;
              for (int ch = 0; ch < 4; ++ch) {
                int valC = (c >> (ch * 8)) & 0xFF;
                int valT = (t >> (ch * 8)) & 0xFF;
                int valB = (b >> (ch * 8)) & 0xFF;
                int valL = (l >> (ch * 8)) & 0xFF;
                int valR = (r >> (ch * 8)) & 0xFF;
                int sh = valC + ((4 * valC - valT - valB - valL - valR) >> 4);
                res |= (uint32_t)std::clamp(sh, 0, 255) << (ch * 8);
              }
              dstRow[x] = res;
            }

            // Advance window
            if (y < H_dst - 1) {
              fillRowBuffer(y + 2, idxT);
            }
          }
          semaphore.release(1);
        }));
      }
      semaphore.acquire(numTasks);
    }

    return destImg;

  } else {
    // --- 2. Downscaling ---
    // Base resize using Qt bilinear scaling
    QImage scaledImg = srcImg.scaled(destSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    QImage destImg(W_dst, H_dst, srcImg.format());

    // Determine thread count
    int numThreads = std::thread::hardware_concurrency();
    if (numThreads <= 0) numThreads = 4;

    QThreadPool* pool = getScalingThreadPool();

    {
      QSemaphore semaphore;
      int rowsPerThread = H_dst / numThreads;
      if (rowsPerThread == 0) rowsPerThread = 1;

      int numTasks = 0;
      for (int i = 0; i < numThreads; ++i) {
        int y_start = i * rowsPerThread;
        int y_end = (i == numThreads - 1) ? H_dst : (i + 1) * rowsPerThread;
        if (y_start >= H_dst) break;

        numTasks++;
        pool->start(new ScalerTask([&scaledImg, &destImg, W_dst, H_dst, y_start, y_end, &semaphore]() {
          // Thread-local sliding window buffer: stores 9 rows of horizontally blurred floats
          std::vector<std::vector<float>> rowBuffers(kGaussianKernelSize, std::vector<float>(W_dst * 4));

          __m256 vGaussWeights256[kGaussianKernelSize];
          __m128 vGaussWeights128[kGaussianKernelSize];
          for (int k = 0; k < kGaussianKernelSize; ++k) {
            vGaussWeights256[k] = _mm256_set1_ps(kGaussianWeights[k]);
            vGaussWeights128[k] = _mm_set1_ps(kGaussianWeights[k]);
          }
          const __m256 vSharp256    = _mm256_set1_ps(kUnsharpSharpStrength);
          const __m256 vBlur256     = _mm256_set1_ps(kUnsharpBlurStrength);
          const __m256 vRound256    = _mm256_set1_ps(kRoundOffset);
          const __m256 vColorMax256 = _mm256_set1_ps(kColorMax);
          const __m128 vSharp128    = _mm_set1_ps(kUnsharpSharpStrength);
          const __m128 vBlur128     = _mm_set1_ps(kUnsharpBlurStrength);
          const __m128 vRound128    = _mm_set1_ps(kRoundOffset);
          const __m128 vColorMax128 = _mm_set1_ps(kColorMax);

          auto blurRowHorizontal = [&](int yd, float* outRow) {
            int yd_clamped = std::clamp(yd, 0, H_dst - 1);
            auto srcRow = constScanlineSpan(scaledImg, yd_clamped);

            int x = 0;
            for (; x + 1 < W_dst; x += 2) {
              __m256 sum = _mm256_setzero_ps();
              for (int k = -kGaussianHalfWidth; k <= kGaussianHalfWidth; ++k) {
                int sxA = std::clamp(x + k, 0, W_dst - 1);
                int sxB = std::clamp(x + 1 + k, 0, W_dst - 1);
                __m256 v_p = _mm256_cvtepi32_ps(_mm256_cvtepu8_epi32(_mm_set_epi32(0, 0, srcRow[sxB], srcRow[sxA])));
                sum = _mm256_fmadd_ps(vGaussWeights256[k + kGaussianHalfWidth], v_p, sum);
              }
              _mm256_storeu_ps(&outRow[x * 4], sum);
            }
            // Scalar/SSE tail for an odd trailing pixel
            if (x < W_dst) {
              __m128 sum = _mm_setzero_ps();
              for (int k = -kGaussianHalfWidth; k <= kGaussianHalfWidth; ++k) {
                int sx = std::clamp(x + k, 0, W_dst - 1);
                __m128 v_p = _mm_cvtepi32_ps(_mm_cvtepu8_epi32(_mm_cvtsi32_si128(srcRow[sx])));
                sum = _mm_fmadd_ps(vGaussWeights128[k + kGaussianHalfWidth], v_p, sum);
              }
              _mm_storeu_ps(&outRow[x * 4], sum);
            }
          };

          // Prime window
          for (int r = 0; r < kGaussianKernelSize; ++r) {
            blurRowHorizontal(y_start - kGaussianHalfWidth + r, rowBuffers[r].data());
          }

          for (int y = y_start; y < y_end; ++y) {
            auto dstRow  = scanlineSpan(destImg, y);
            auto origRow = constScanlineSpan(scaledImg, y);

            // Compute vertical Gaussian blur + Unsharp Mask blending on the fly
            int x = 0;
            for (; x + 1 < W_dst; x += 2) {
              __m256 sum = _mm256_setzero_ps();
              for (int k = 0; k < kGaussianKernelSize; ++k) {
                int bufIdx = (y - y_start + k) % kGaussianKernelSize;
                __m256 r_val = _mm256_loadu_ps(&rowBuffers[bufIdx][x * 4]);
                sum = _mm256_fmadd_ps(vGaussWeights256[k], r_val, sum);
              }

              __m128i orig_loaded = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(&origRow[x]));
              __m256 orig_f = _mm256_cvtepi32_ps(_mm256_cvtepu8_epi32(orig_loaded));

              // strength: 0.15 unsharp mask -> 1.15 * original - 0.15 * blurred
              __m256 final_f = _mm256_fmsub_ps(vSharp256, orig_f, _mm256_mul_ps(vBlur256, sum));

              __m256 rounded = _mm256_add_ps(final_f, vRound256);
              rounded = _mm256_min_ps(_mm256_max_ps(rounded, _mm256_setzero_ps()), vColorMax256);

              __m256i res_i = _mm256_cvtps_epi32(rounded);
              __m256i packed_16 = _mm256_packus_epi32(res_i, res_i);
              __m256i packed_8 = _mm256_packus_epi16(packed_16, packed_16);

              dstRow[x]     = _mm_cvtsi128_si32(_mm256_castsi256_si128(packed_8));
              dstRow[x + 1] = _mm_cvtsi128_si32(_mm256_extractf128_si256(packed_8, 1));
            }
            // Scalar/SSE tail for an odd trailing pixel
            if (x < W_dst) {
              __m128 sum = _mm_setzero_ps();
              for (int k = 0; k < kGaussianKernelSize; ++k) {
                int bufIdx = (y - y_start + k) % kGaussianKernelSize;
                __m128 r_val = _mm_loadu_ps(&rowBuffers[bufIdx][x * 4]);
                sum = _mm_fmadd_ps(vGaussWeights128[k], r_val, sum);
              }

              __m128 orig_f = _mm_cvtepi32_ps(_mm_cvtepu8_epi32(_mm_cvtsi32_si128(origRow[x])));

              __m128 final_f = _mm_fmsub_ps(vSharp128, orig_f, _mm_mul_ps(vBlur128, sum));

              __m128 rounded = _mm_add_ps(final_f, vRound128);
              rounded = _mm_min_ps(_mm_max_ps(rounded, _mm_setzero_ps()), vColorMax128);

              __m128i res_i = _mm_cvtps_epi32(rounded);
              __m128i packed_16 = _mm_packus_epi32(res_i, res_i);
              __m128i packed_8 = _mm_packus_epi16(packed_16, packed_16);

              dstRow[x] = _mm_cvtsi128_si32(packed_8);
            }

            // Advance window
            if (y < H_dst - 1) {
              int idx_to_replace = (y - y_start) % kGaussianKernelSize;
              blurRowHorizontal(y + 5, rowBuffers[idx_to_replace].data());
            }
          }
          semaphore.release(1);
        }));
      }
      semaphore.acquire(numTasks);
    }

    return destImg;
  }
}

ColorMatrix ImageLib::getColorAdjustmentMatrix(float exposure, float contrast, float brightness, float temperature, float tint, float saturation, float hue) {
  // Helper to multiply A and B (3x3 matrices), storing result in C
  auto multiply = [](const float A[3][3], const float B[3][3], float C[3][3]) {
    for (int i = 0; i < 3; ++i) {
      for (int j = 0; j < 3; ++j) {
        C[i][j] = A[i][0] * B[0][j] + A[i][1] * B[1][j] + A[i][2] * B[2][j];
      }
    }
  };

  // 1 & 2. White balance (Temperature & Tint) & Exposure
  float factor = std::pow(2.0f, exposure);
  float w_r = (1.0f + temperature + tint * 0.5f) * factor;
  float w_g = (1.0f - tint) * factor;
  float w_b = (1.0f - temperature + tint * 0.5f) * factor;

  float M_current[3][3] = {
    {w_r,  0.0f, 0.0f},
    {0.0f, w_g,  0.0f},
    {0.0f, 0.0f, w_b }
  };

  // 3. Hue rotate
  if (std::abs(hue) > kAdjustEpsilon) {
    float hueRad = hue * static_cast<float>(ImageLib::kPi) / 180.0f;
    float cosAngle = std::cos(hueRad);
    float sinAngle = std::sin(hueRad);
    float k = 0.57735f;
    float cosInv = 1.0f - cosAngle;

    float M_hue[3][3] = {
      { cosAngle + k * k * cosInv,      -k * sinAngle + k * k * cosInv,  k * sinAngle + k * k * cosInv },
      { k * sinAngle + k * k * cosInv,  cosAngle + k * k * cosInv,       -k * sinAngle + k * k * cosInv },
      { -k * sinAngle + k * k * cosInv, k * sinAngle + k * k * cosInv,   cosAngle + k * k * cosInv }
    };

    float M_temp[3][3];
    multiply(M_hue, M_current, M_temp);
    for (int i = 0; i < 3; ++i) {
      for (int j = 0; j < 3; ++j) M_current[i][j] = M_temp[i][j];
    }
  }

  // 4. Saturation
  if (std::abs(saturation - 1.0f) > kAdjustEpsilon) {
    float rWeight = 0.2126f * (1.0f - saturation);
    float gWeight = 0.7152f * (1.0f - saturation);
    float bWeight = 0.0722f * (1.0f - saturation);

    float M_sat[3][3] = {
      { saturation + rWeight, gWeight,                bWeight },
      { rWeight,              saturation + gWeight,   bWeight },
      { rWeight,              gWeight,                saturation + bWeight }
    };

    float M_temp[3][3];
    multiply(M_sat, M_current, M_temp);
    for (int i = 0; i < 3; ++i) {
      for (int j = 0; j < 3; ++j) M_current[i][j] = M_temp[i][j];
    }
  }

  // 5 & 6. Contrast & Brightness
  ColorMatrix result;
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      result.m[i][j] = M_current[i][j] * contrast;
    }
  }
  result.offset = brightness * contrast + 0.5f * (1.0f - contrast);

  return result;
}

QImage ImageLib::applyColorAdjustments(std::shared_ptr<const QImage> source, float exposure, float contrast, float brightness, float temperature, float tint, float saturation, float hue) {
  if (!source)
    return QImage();

  QImage dst = source->convertToFormat(QImage::Format_ARGB32);
  ColorMatrix cm = getColorAdjustmentMatrix(exposure, contrast, brightness, temperature, tint, saturation, hue);

  int height = dst.height();
  int width = dst.width();

  // Set up AVX2 constants
  __m256i mask_b_i = _mm256_set1_epi32(0x000000FF);
  __m256i mask_g_i = _mm256_set1_epi32(0x0000FF00);
  __m256i mask_r_i = _mm256_set1_epi32(0x00FF0000);
  __m256i mask_a_i = _mm256_set1_epi32(0xFF000000);

  __m256 v_zero = _mm256_setzero_ps();
  __m256 v_255 = _mm256_set1_ps(255.0f);

  __m256 v_m00 = _mm256_set1_ps(cm.m[0][0]);
  __m256 v_m01 = _mm256_set1_ps(cm.m[0][1]);
  __m256 v_m02 = _mm256_set1_ps(cm.m[0][2]);

  __m256 v_m10 = _mm256_set1_ps(cm.m[1][0]);
  __m256 v_m11 = _mm256_set1_ps(cm.m[1][1]);
  __m256 v_m12 = _mm256_set1_ps(cm.m[1][2]);

  __m256 v_m20 = _mm256_set1_ps(cm.m[2][0]);
  __m256 v_m21 = _mm256_set1_ps(cm.m[2][1]);
  __m256 v_m22 = _mm256_set1_ps(cm.m[2][2]);

  // Pre-scale offset to avoid doing it per pixel
  __m256 v_offset_scaled = _mm256_set1_ps(cm.offset * 255.0f + 0.5f);

  dst.detach();
  uchar *bits = dst.bits();
  int bytesPerLine = dst.bytesPerLine();

  int numThreads = std::thread::hardware_concurrency();
  if (numThreads <= 0) numThreads = 4;

  QThreadPool* pool = getScalingThreadPool();

  {
    QSemaphore semaphore;
    int rowsPerThread = height / numThreads;
    if (rowsPerThread == 0) rowsPerThread = 1;

    int numTasks = 0;
    for (int i = 0; i < numThreads; ++i) {
      int y_start = i * rowsPerThread;
      int y_end = (i == numThreads - 1) ? height : (i + 1) * rowsPerThread;
      if (y_start >= height) break;

      numTasks++;
      pool->start(new ScalerTask([bits, bytesPerLine, cm, width, y_start, y_end, &semaphore,
                                  mask_b_i, mask_g_i, mask_r_i, mask_a_i,
                                  v_zero, v_255,
                                  v_m00, v_m01, v_m02,
                                  v_m10, v_m11, v_m12,
                                  v_m20, v_m21, v_m22,
                                  v_offset_scaled]() {
        for (int y = y_start; y < y_end; ++y) {
          QRgb *line = reinterpret_cast<QRgb*>(bits + y * bytesPerLine);
          int x = 0;
          int avx_end = width - 8;

          for (; x <= avx_end; x += 8) {
            __m256i pix = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&line[x]));

            // Extract channels as 32-bit integers
            __m256i b_i = _mm256_and_si256(pix, mask_b_i);
            __m256i g_i = _mm256_srli_epi32(_mm256_and_si256(pix, mask_g_i), 8);
            __m256i r_i = _mm256_srli_epi32(_mm256_and_si256(pix, mask_r_i), 16);
            __m256i a_i = _mm256_and_si256(pix, mask_a_i);

            // Convert to float in [0.0, 255.0] range directly without inverse division
            __m256 b_f = _mm256_cvtepi32_ps(b_i);
            __m256 g_f = _mm256_cvtepi32_ps(g_i);
            __m256 r_f = _mm256_cvtepi32_ps(r_i);

            // Apply matrix: out = M * in + pre_scaled_offset
            __m256 out_r = _mm256_fmadd_ps(v_m00, r_f, _mm256_fmadd_ps(v_m01, g_f, _mm256_fmadd_ps(v_m02, b_f, v_offset_scaled)));
            __m256 out_g = _mm256_fmadd_ps(v_m10, r_f, _mm256_fmadd_ps(v_m11, g_f, _mm256_fmadd_ps(v_m12, b_f, v_offset_scaled)));
            __m256 out_b = _mm256_fmadd_ps(v_m20, r_f, _mm256_fmadd_ps(v_m21, g_f, _mm256_fmadd_ps(v_m22, b_f, v_offset_scaled)));

            // Clamp to [0, 255]
            out_r = _mm256_min_ps(_mm256_max_ps(out_r, v_zero), v_255);
            out_g = _mm256_min_ps(_mm256_max_ps(out_g, v_zero), v_255);
            out_b = _mm256_min_ps(_mm256_max_ps(out_b, v_zero), v_255);

            // Convert back to integers
            __m256i out_r_i = _mm256_cvtps_epi32(out_r);
            __m256i out_g_i = _mm256_cvtps_epi32(out_g);
            __m256i out_b_i = _mm256_cvtps_epi32(out_b);

            // Repack channels into ARGB format
            __m256i out_r_shifted = _mm256_slli_epi32(out_r_i, 16);
            __m256i out_g_shifted = _mm256_slli_epi32(out_g_i, 8);

            __m256i out_pix = _mm256_or_si256(a_i, _mm256_or_si256(out_r_shifted, _mm256_or_si256(out_g_shifted, out_b_i)));

            _mm256_storeu_si256(reinterpret_cast<__m256i*>(&line[x]), out_pix);
          }

          // Scalar fallback loop for remaining pixels
          for (; x < width; ++x) {
            QRgb pixel = line[x];
            unsigned int a = pixel & 0xFF000000;
            float r = ((pixel >> 16) & 0xFF);
            float g = ((pixel >> 8) & 0xFF);
            float b = (pixel & 0xFF);

            float out_r = cm.m[0][0] * r + cm.m[0][1] * g + cm.m[0][2] * b + cm.offset * 255.0f;
            float out_g = cm.m[1][0] * r + cm.m[1][1] * g + cm.m[1][2] * b + cm.offset * 255.0f;
            float out_b = cm.m[2][0] * r + cm.m[2][1] * g + cm.m[2][2] * b + cm.offset * 255.0f;

            int nr = std::clamp(static_cast<int>(out_r + 0.5f), 0, 255);
            int ng = std::clamp(static_cast<int>(out_g + 0.5f), 0, 255);
            int nb = std::clamp(static_cast<int>(out_b + 0.5f), 0, 255);

            line[x] = a | (nr << 16) | (ng << 8) | nb;
          }
        }
        semaphore.release(1);
      }));
    }
    semaphore.acquire(numTasks);
  }

  return dst;
}

QImage ImageLib::loadICO(const QString &path) {
  QIcon icon(path);
  QList<QSize> sizes = icon.availableSizes();
  if (sizes.isEmpty()) {
    return QImage();
  }
  QSize maxSize = *std::ranges::max_element(sizes, {}, &QSize::width);
  QPixmap iconPix = icon.pixmap(maxSize);
  return iconPix.toImage();
}
