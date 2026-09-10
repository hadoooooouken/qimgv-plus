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

// Forward declarations for scanline helpers
inline std::span<uint32_t> scanlineSpan(QImage &img, int y);
inline std::span<const uint32_t> constScanlineSpan(const QImage &img, int y);

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

// --- Magic Kernel Sharp 2021 (a = 3, v = 3) ---
// Reference: johncostella.com/magic. MKS2021 is defined as
//   k(x) = sum_{s=-3}^{3} c_s * m3(x + s)
// where m3 is the "Magic Kernel" for a = 3 (a piecewise-quadratic kernel with
// support (-1.5, +1.5)) and c_s are the fixed "Magic Sharp" coefficients for
// a = 3, v = 3. k(x) itself has support (-4.5, +4.5).
constexpr double kMks3C0 = 17.0 / 12.0;
constexpr double kMks3C1 = -35.0 / 144.0;
constexpr double kMks3C2 = 1.0 / 24.0;
constexpr double kMks3C3 = -1.0 / 144.0;
constexpr double kMks2021Support = 4.5;

// m3(x): the Magic Kernel for a = 3.
inline double magicKernelA3(double x) {
  if (x <= -1.5 || x >= 1.5)
    return 0.0;
  double x2 = x * x;
  if (x <= -0.5)
    return x2 / 2.0 + 1.5 * x + 9.0 / 8.0;
  if (x <= 0.5)
    return -x2 + 0.75;
  return x2 / 2.0 - 1.5 * x + 9.0 / 8.0;
}

// k(x): the full Magic Kernel Sharp 2021 kernel.
inline double mks2021Kernel(double x) {
  if (x <= -kMks2021Support || x >= kMks2021Support)
    return 0.0;
  double sum = kMks3C0 * magicKernelA3(x);
  sum += kMks3C1 * (magicKernelA3(x - 1.0) + magicKernelA3(x + 1.0));
  sum += kMks3C2 * (magicKernelA3(x - 2.0) + magicKernelA3(x + 2.0));
  sum += kMks3C3 * (magicKernelA3(x - 3.0) + magicKernelA3(x + 3.0));
  return sum;
}

// One output sample's resampling taps: 'count' consecutive source indices
// starting at 'left' (not yet clamped to the source range), with weights
// stored in a shared pool at 'weightOffset'. Storing indices as a contiguous
// run (rather than a full index list) keeps this cheap to build and cheap to
// store, since MKS2021's support is contiguous in source space.
struct MksAxisTap {
  int left;
  int count;
  int weightOffset;
};

// Builds the per-output-sample resampling taps for one axis (horizontal or
// vertical). When downscaling (nSrc > nDst) the kernel is widened by
// 1 / scale and its output re-normalized by the same factor, which is the
// standard way to turn an interpolation kernel into a (single-pass)
// anti-aliasing minification filter.
void buildMksAxisTaps(int nSrc, int nDst, std::vector<MksAxisTap> &taps,
                       std::vector<float> &weightPool) {
  taps.resize(nDst);
  double scale = (double)nSrc / (double)nDst;
  double filterScale = std::max(scale, 1.0);
  double support = kMks2021Support * filterScale;

  weightPool.clear();
  weightPool.reserve((size_t)nDst * (size_t)(support * 2.0 + 2.0));

  for (int x = 0; x < nDst; ++x) {
    double u = (x + 0.5) * scale - 0.5;
    int left = (int)std::floor(u - support);
    int right = (int)std::ceil(u + support);
    if (right < left)
      right = left;

    int weightOffset = (int)weightPool.size();
    double sum = 0.0;
    for (int i = left; i <= right; ++i) {
      double w = mks2021Kernel((u - i) / filterScale) / filterScale;
      weightPool.push_back((float)w);
      sum += w;
    }

    // Re-normalize: guards against the tiny discretization error introduced
    // by cutting the (theoretically infinite-precision) kernel off at
    // integer 'left'/'right' bounds, so output brightness never drifts.
    if (sum != 0.0) {
      float invSum = (float)(1.0 / sum);
      for (int i = weightOffset; i < (int)weightPool.size(); ++i)
        weightPool[i] *= invSum;
    }

    taps[x] = { left, right - left + 1, weightOffset };
  }
}

// --- Fixed-width (K = 11) AVX2 fast path ---
// buildMksAxisTaps() proves that scale <= 1 (upscaling, or no resize at all)
// always needs at most 11 taps (filterScale is clamped to 1, so the support
// never widens past its base 9px width, +/-1 for integer floor/ceil
// alignment). Any actual downscale exceeds 11 taps almost immediately (12+
// at just 1% reduction), so this path is only ever selected for upscale /
// 1:1, which is exactly the interactive zoom / Fit-to-window / Resize-up
// case. Genuine downscaling always falls back to the variable-tap path
// below, which stays correct (just not AVX2-widened to 2 pixels/iteration)
// for arbitrary scale factors.
constexpr int kMksFixedTaps = 11;

struct MksFixedTaps {
  std::vector<int> left;
  std::vector<std::array<float, kMksFixedTaps>> weights;
};

// Only valid to call when every tap.count in 'taps' is <= kMksFixedTaps.
MksFixedTaps convertToFixedTaps(const std::vector<MksAxisTap> &taps,
                                 const std::vector<float> &weightPool) {
  MksFixedTaps fixed;
  fixed.left.resize(taps.size());
  fixed.weights.resize(taps.size());
  for (size_t x = 0; x < taps.size(); ++x) {
    fixed.left[x] = taps[x].left;
    std::array<float, kMksFixedTaps> arr{};
    for (int t = 0; t < taps[x].count; ++t)
      arr[t] = weightPool[taps[x].weightOffset + t];
    // Any remaining slots (t >= taps[x].count) stay 0.0f, which is safe:
    // they multiply a clamped (repeated) edge pixel by zero.
    fixed.weights[x] = arr;
  }
  return fixed;
}

// Horizontal fixed-11-tap pass for one row range, AVX2, 2 output pixels/iter.
void mksHorizontalFixed(const QImage &src, QImage &inter,
                         const MksFixedTaps &taps, int W_src, int W_dst,
                         int y_start, int y_end) {
  for (int y = y_start; y < y_end; ++y) {
    auto srcRow = constScanlineSpan(src, y);
    auto interRow = scanlineSpan(inter, y);

    int x = 0;
    for (; x + 1 < W_dst; x += 2) {
      const auto &w0 = taps.weights[x];
      const auto &w1 = taps.weights[x + 1];
      int left0 = taps.left[x];
      int left1 = taps.left[x + 1];

      __m256 acc = _mm256_setzero_ps();
      for (int t = 0; t < kMksFixedTaps; ++t) {
        uint32_t px0 = srcRow[std::clamp(left0 + t, 0, W_src - 1)];
        uint32_t px1 = srcRow[std::clamp(left1 + t, 0, W_src - 1)];

        __m128i two = _mm_set_epi32(0, 0, px1, px0);
        __m256 v_p = _mm256_cvtepi32_ps(_mm256_cvtepu8_epi32(two));
        __m256 v_w = _mm256_setr_ps(w0[t], w0[t], w0[t], w0[t],
                                     w1[t], w1[t], w1[t], w1[t]);
        acc = _mm256_fmadd_ps(v_p, v_w, acc);
      }

      acc = _mm256_add_ps(acc, _mm256_set1_ps(kRoundOffset));
      acc = _mm256_min_ps(_mm256_max_ps(acc, _mm256_setzero_ps()), _mm256_set1_ps(kColorMax));
      __m256i res_i = _mm256_cvtps_epi32(acc);

      __m128i lo = _mm256_castsi256_si128(res_i);
      __m128i hi = _mm256_extracti128_si256(res_i, 1);
      interRow[x]     = _mm_cvtsi128_si32(_mm_packus_epi16(_mm_packus_epi32(lo, lo), _mm_packus_epi32(lo, lo)));
      interRow[x + 1] = _mm_cvtsi128_si32(_mm_packus_epi16(_mm_packus_epi32(hi, hi), _mm_packus_epi32(hi, hi)));
    }
    // Scalar/SSE tail for an odd trailing pixel
    if (x < W_dst) {
      const auto &w = taps.weights[x];
      int left = taps.left[x];
      __m128 acc = _mm_setzero_ps();
      for (int t = 0; t < kMksFixedTaps; ++t) {
        uint32_t px = srcRow[std::clamp(left + t, 0, W_src - 1)];
        __m128 v_p = _mm_cvtepi32_ps(_mm_cvtepu8_epi32(_mm_cvtsi32_si128(px)));
        acc = _mm_fmadd_ps(v_p, _mm_set1_ps(w[t]), acc);
      }
      acc = _mm_add_ps(acc, _mm_set1_ps(kRoundOffset));
      acc = _mm_min_ps(_mm_max_ps(acc, _mm_setzero_ps()), _mm_set1_ps(kColorMax));
      __m128i res = _mm_cvtps_epi32(acc);
      interRow[x] = _mm_cvtsi128_si32(_mm_packus_epi16(_mm_packus_epi32(res, res), _mm_packus_epi32(res, res)));
    }
  }
}

// Vertical fixed-11-tap pass for one row range, AVX2, 2 output pixels/iter.
void mksVerticalFixed(const QImage &inter, QImage &dst,
                       const MksFixedTaps &taps, int H_src, int W_dst,
                       int y_start, int y_end) {
  for (int y = y_start; y < y_end; ++y) {
    const auto &w = taps.weights[y];
    int left = taps.left[y];
    auto dstRow = scanlineSpan(dst, y);

    std::array<std::span<const uint32_t>, kMksFixedTaps> rows;
    for (int t = 0; t < kMksFixedTaps; ++t)
      rows[t] = constScanlineSpan(inter, std::clamp(left + t, 0, H_src - 1));

    int x = 0;
    for (; x + 1 < W_dst; x += 2) {
      __m256 acc = _mm256_setzero_ps();
      for (int t = 0; t < kMksFixedTaps; ++t) {
        uint32_t px0 = rows[t][x];
        uint32_t px1 = rows[t][x + 1];
        __m128i two = _mm_set_epi32(0, 0, px1, px0);
        __m256 v_p = _mm256_cvtepi32_ps(_mm256_cvtepu8_epi32(two));
        acc = _mm256_fmadd_ps(v_p, _mm256_set1_ps(w[t]), acc);
      }
      acc = _mm256_add_ps(acc, _mm256_set1_ps(kRoundOffset));
      acc = _mm256_min_ps(_mm256_max_ps(acc, _mm256_setzero_ps()), _mm256_set1_ps(kColorMax));
      __m256i res_i = _mm256_cvtps_epi32(acc);

      __m128i lo = _mm256_castsi256_si128(res_i);
      __m128i hi = _mm256_extracti128_si256(res_i, 1);
      dstRow[x]     = _mm_cvtsi128_si32(_mm_packus_epi16(_mm_packus_epi32(lo, lo), _mm_packus_epi32(lo, lo)));
      dstRow[x + 1] = _mm_cvtsi128_si32(_mm_packus_epi16(_mm_packus_epi32(hi, hi), _mm_packus_epi32(hi, hi)));
    }
    if (x < W_dst) {
      __m128 acc = _mm_setzero_ps();
      for (int t = 0; t < kMksFixedTaps; ++t) {
        uint32_t px = rows[t][x];
        __m128 v_p = _mm_cvtepi32_ps(_mm_cvtepu8_epi32(_mm_cvtsi32_si128(px)));
        acc = _mm_fmadd_ps(v_p, _mm_set1_ps(w[t]), acc);
      }
      acc = _mm_add_ps(acc, _mm_set1_ps(kRoundOffset));
      acc = _mm_min_ps(_mm_max_ps(acc, _mm_setzero_ps()), _mm_set1_ps(kColorMax));
      __m128i res = _mm_cvtps_epi32(acc);
      dstRow[x] = _mm_cvtsi128_si32(_mm_packus_epi16(_mm_packus_epi32(res, res), _mm_packus_epi32(res, res)));
    }
  }
}

// Horizontal variable-tap pass (fallback for genuine downscaling, where the
// widened kernel needs more than kMksFixedTaps taps).
//
// Processes 2 output pixels per AVX2 iteration, same as mksHorizontalFixed.
// Unlike the fixed path, adjacent output columns can have different tap
// counts/left offsets here, so the shorter of the pair is padded with
// weight 0 up to the pair's max count for the duration of the iteration;
// the (safely clamped, just irrelevant) source pixel it reads contributes
// nothing to the sum. A scalar/SSE tail handles a leftover odd column.
void mksHorizontalGeneral(const QImage &src, QImage &inter,
                           const std::vector<MksAxisTap> &taps,
                           const std::vector<float> &weightPool, int W_src,
                           int W_dst, int y_start, int y_end) {
  for (int y = y_start; y < y_end; ++y) {
    auto srcRow = constScanlineSpan(src, y);
    auto interRow = scanlineSpan(inter, y);

    int x = 0;
    for (; x + 1 < W_dst; x += 2) {
      const MksAxisTap &tap0 = taps[x];
      const MksAxisTap &tap1 = taps[x + 1];
      int maxCount = std::max(tap0.count, tap1.count);

      __m256 acc = _mm256_setzero_ps();
      for (int t = 0; t < maxCount; ++t) {
        int srcX0 = std::clamp(tap0.left + t, 0, W_src - 1);
        int srcX1 = std::clamp(tap1.left + t, 0, W_src - 1);
        uint32_t px0 = srcRow[srcX0];
        uint32_t px1 = srcRow[srcX1];

        __m128i two = _mm_set_epi32(0, 0, px1, px0);
        __m256 v_p = _mm256_cvtepi32_ps(_mm256_cvtepu8_epi32(two));

        float w0 = (t < tap0.count) ? weightPool[tap0.weightOffset + t] : 0.0f;
        float w1 = (t < tap1.count) ? weightPool[tap1.weightOffset + t] : 0.0f;
        __m256 v_w = _mm256_setr_ps(w0, w0, w0, w0, w1, w1, w1, w1);
        acc = _mm256_fmadd_ps(v_p, v_w, acc);
      }

      acc = _mm256_add_ps(acc, _mm256_set1_ps(kRoundOffset));
      acc = _mm256_min_ps(_mm256_max_ps(acc, _mm256_setzero_ps()), _mm256_set1_ps(kColorMax));
      __m256i res_i = _mm256_cvtps_epi32(acc);

      __m128i lo = _mm256_castsi256_si128(res_i);
      __m128i hi = _mm256_extracti128_si256(res_i, 1);
      interRow[x]     = _mm_cvtsi128_si32(_mm_packus_epi16(_mm_packus_epi32(lo, lo), _mm_packus_epi32(lo, lo)));
      interRow[x + 1] = _mm_cvtsi128_si32(_mm_packus_epi16(_mm_packus_epi32(hi, hi), _mm_packus_epi32(hi, hi)));
    }
    // Scalar/SSE tail for an odd trailing pixel
    if (x < W_dst) {
      const MksAxisTap &tap = taps[x];
      __m128 acc = _mm_setzero_ps();
      for (int t = 0; t < tap.count; ++t) {
        int srcX = std::clamp(tap.left + t, 0, W_src - 1);
        __m128 v_p = _mm_cvtepi32_ps(_mm_cvtepu8_epi32(_mm_cvtsi32_si128(srcRow[srcX])));
        acc = _mm_fmadd_ps(v_p, _mm_set1_ps(weightPool[tap.weightOffset + t]), acc);
      }
      acc = _mm_add_ps(acc, _mm_set1_ps(kRoundOffset));
      acc = _mm_min_ps(_mm_max_ps(acc, _mm_setzero_ps()), _mm_set1_ps(kColorMax));
      __m128i res = _mm_cvtps_epi32(acc);
      interRow[x] = _mm_cvtsi128_si32(_mm_packus_epi16(_mm_packus_epi32(res, res), _mm_packus_epi32(res, res)));
    }
  }
}

// Vertical variable-tap pass (fallback for genuine downscaling).
void mksVerticalGeneral(const QImage &inter, QImage &dst,
                         const std::vector<MksAxisTap> &taps,
                         const std::vector<float> &weightPool, int H_src,
                         int W_dst, int y_start, int y_end) {
  // Reused across every output row in this thread's chunk instead of
  // allocating a fresh vector per row: resize() below only grows capacity
  // (never releases it), so once it reaches the widest tap count in the
  // range, subsequent rows cost zero allocations.
  std::vector<std::span<const uint32_t>> rows;
  for (int y = y_start; y < y_end; ++y) {
    const MksAxisTap &tap = taps[y];
    auto dstRow = scanlineSpan(dst, y);

    rows.resize(tap.count);
    for (int t = 0; t < tap.count; ++t)
      rows[t] = constScanlineSpan(inter, std::clamp(tap.left + t, 0, H_src - 1));

    int x = 0;
    for (; x + 1 < W_dst; x += 2) {
      __m256 acc = _mm256_setzero_ps();
      for (int t = 0; t < tap.count; ++t) {
        uint32_t px0 = rows[t][x];
        uint32_t px1 = rows[t][x + 1];
        __m128i two = _mm_set_epi32(0, 0, px1, px0);
        __m256 v_p = _mm256_cvtepi32_ps(_mm256_cvtepu8_epi32(two));
        acc = _mm256_fmadd_ps(v_p, _mm256_set1_ps(weightPool[tap.weightOffset + t]), acc);
      }
      acc = _mm256_add_ps(acc, _mm256_set1_ps(kRoundOffset));
      acc = _mm256_min_ps(_mm256_max_ps(acc, _mm256_setzero_ps()), _mm256_set1_ps(kColorMax));
      __m256i res_i = _mm256_cvtps_epi32(acc);

      __m128i lo = _mm256_castsi256_si128(res_i);
      __m128i hi = _mm256_extracti128_si256(res_i, 1);
      dstRow[x]     = _mm_cvtsi128_si32(_mm_packus_epi16(_mm_packus_epi32(lo, lo), _mm_packus_epi32(lo, lo)));
      dstRow[x + 1] = _mm_cvtsi128_si32(_mm_packus_epi16(_mm_packus_epi32(hi, hi), _mm_packus_epi32(hi, hi)));
    }
    // Scalar/SSE tail for an odd trailing column
    if (x < W_dst) {
      __m128 acc = _mm_setzero_ps();
      for (int t = 0; t < tap.count; ++t) {
        __m128 v_p = _mm_cvtepi32_ps(_mm_cvtepu8_epi32(_mm_cvtsi32_si128(rows[t][x])));
        acc = _mm_fmadd_ps(v_p, _mm_set1_ps(weightPool[tap.weightOffset + t]), acc);
      }
      acc = _mm_add_ps(acc, _mm_set1_ps(kRoundOffset));
      acc = _mm_min_ps(_mm_max_ps(acc, _mm_setzero_ps()), _mm_set1_ps(kColorMax));
      __m128i res = _mm_cvtps_epi32(acc);
      dstRow[x] = _mm_cvtsi128_si32(_mm_packus_epi16(_mm_packus_epi32(res, res), _mm_packus_epi32(res, res)));
    }
  }
}

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

QImage ImageLib::scaled(std::shared_ptr<const QImage> source, QSize destSize,
                        ScalingFilter filter) {
  int maxDim = 12288;
  qint64 maxPixels = 100000000;
  if (settings->useUpscayl() || settings->resizeUseUpscayl()) {
    maxDim = 16384;         // Cap to GPU max texture size / GDI memory safety limit (16384)
    maxPixels = 268435456;  // Cap to 256 Megapixels (~1.07 GB RAM) to prevent drawing allocations crashes
  }

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
  case QI_FILTER_MKS2021:
    return scaled_MKS2021(scaleTarget, destSize);
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

  if (smooth && source->hasAlphaChannel()) {
    // Qt::SmoothTransformation interpolates QImage::Format_ARGB32 as straight
    // (non-premultiplied) alpha, so RGB baked into fully-transparent source
    // pixels (e.g. black, as exported by most design tools) bleeds a dark/light
    // fringe into neighboring opaque pixels. Converting to premultiplied alpha
    // first makes the RGB of transparent pixels 0, so they contribute nothing
    // to the blend regardless of what color was stored there.
    QImage premult = source->convertToFormat(QImage::Format_ARGB32_Premultiplied);
    dest = premult.scaled(destSize.width(), destSize.height(),
                          Qt::IgnoreAspectRatio, mode);
    dest = dest.convertToFormat(QImage::Format_ARGB32);
  } else {
    dest = source->scaled(destSize.width(), destSize.height(),
                          Qt::IgnoreAspectRatio, mode);
  }
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

  // Degenerate case: requested size matches the source, so there is nothing
  // to resample. Skip straight to the format conversion this function would
  // have produced anyway.
  if (W_dst == W_src && H_dst == H_src) {
    return source->convertToFormat(source->hasAlphaChannel() ? QImage::Format_ARGB32
                                                               : QImage::Format_RGB32);
  }

  // Convert source to a 32-bit format we can interpolate directly.
  // All the weighted-sum math below (bicubic, Gaussian blur, unsharp mask)
  // operates per-channel on raw R/G/B/A bytes, including alpha itself, with no
  // knowledge of alpha weighting. If the image has an alpha channel we must
  // work in *premultiplied* alpha (Format_ARGB32_Premultiplied) rather than
  // straight alpha (Format_ARGB32): with straight alpha, a fully-transparent
  // texel can still carry an arbitrary baked-in RGB color (most exporters
  // render transparent regions on a black backdrop), and that color gets
  // averaged into neighboring opaque pixels near an edge, producing a dark or
  // light fringe. With premultiplied alpha, fully-transparent texels are
  // exactly (0,0,0,0), so they contribute nothing to the weighted sum
  // regardless of what was stored there, and the fringe disappears.
  QImage srcImg = *source.get();
  bool workingPremultiplied = srcImg.hasAlphaChannel();
  QImage::Format workFmt = workingPremultiplied ? QImage::Format_ARGB32_Premultiplied
                                                 : QImage::Format_RGB32;
  if (srcImg.format() != workFmt) {
    srcImg = srcImg.convertToFormat(workFmt);
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

    if (workingPremultiplied)
      return destImg.convertToFormat(QImage::Format_ARGB32);
    return destImg;

  } else {
    // --- 2. Downscaling ---
    // Base resize using Qt bilinear scaling (srcImg is already premultiplied
    // when it has alpha, so this inherits the same fringe-free interpolation)
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

    if (workingPremultiplied)
      return destImg.convertToFormat(QImage::Format_ARGB32);
    return destImg;
  }
}

QImage ImageLib::scaled_MKS2021(std::shared_ptr<const QImage> source,
                                 QSize destSize) {
  if (!source || source->isNull())
    return QImage();

  int W_src = source->width();
  int H_src = source->height();
  int W_dst = destSize.width();
  int H_dst = destSize.height();

  if (W_dst <= 0 || H_dst <= 0)
    return QImage();

  // Degenerate case: no resampling to do at all. Skip straight to a format
  // conversion matching this function's normal output contract (ARGB32 if
  // the source has alpha, RGB32 otherwise) instead of running the taps
  // through an identity resample.
  if (W_dst == W_src && H_dst == H_src) {
    return source->convertToFormat(source->hasAlphaChannel() ? QImage::Format_ARGB32
                                                               : QImage::Format_RGB32);
  }

  // Same premultiplied-alpha handling as scaled_Smart: avoids a dark/light
  // fringe from RGB baked into fully-transparent source texels.
  QImage srcImg = *source.get();
  bool workingPremultiplied = srcImg.hasAlphaChannel();
  QImage::Format workFmt = workingPremultiplied ? QImage::Format_ARGB32_Premultiplied
                                                 : QImage::Format_RGB32;
  if (srcImg.format() != workFmt) {
    srcImg = srcImg.convertToFormat(workFmt);
  }

  // Build resampling taps once per axis; identical logic handles upscaling
  // and downscaling (the kernel is simply widened for the latter).
  std::vector<MksAxisTap> hTaps, vTaps;
  std::vector<float> hWeightPool, vWeightPool;
  buildMksAxisTaps(W_src, W_dst, hTaps, hWeightPool);
  buildMksAxisTaps(H_src, H_dst, vTaps, vWeightPool);

  // Per axis, use the AVX2 fixed-11-tap fast path whenever every tap in that
  // axis actually fits (true for upscale / 1:1, per the invariant documented
  // on kMksFixedTaps); fall back to the variable-tap path only where the
  // widened downscale kernel genuinely needs more taps than that.
  bool hUseFixed = std::ranges::all_of(
      hTaps, [](const MksAxisTap &t) { return t.count <= kMksFixedTaps; });
  bool vUseFixed = std::ranges::all_of(
      vTaps, [](const MksAxisTap &t) { return t.count <= kMksFixedTaps; });

  MksFixedTaps hFixedTaps, vFixedTaps;
  if (hUseFixed)
    hFixedTaps = convertToFixedTaps(hTaps, hWeightPool);
  if (vUseFixed)
    vFixedTaps = convertToFixedTaps(vTaps, vWeightPool);

  int numThreads = std::thread::hardware_concurrency();
  if (numThreads <= 0) numThreads = 4;
  QThreadPool *pool = getScalingThreadPool();

  // --- Horizontal pass: W_src x H_src -> W_dst x H_src ---
  QImage interImg(W_dst, H_src, srcImg.format());
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
      pool->start(new ScalerTask([&srcImg, &interImg, &hTaps, &hWeightPool,
                                   &hFixedTaps, hUseFixed, W_src, W_dst,
                                   y_start, y_end, &semaphore]() {
        if (hUseFixed)
          mksHorizontalFixed(srcImg, interImg, hFixedTaps, W_src, W_dst, y_start, y_end);
        else
          mksHorizontalGeneral(srcImg, interImg, hTaps, hWeightPool, W_src, W_dst, y_start, y_end);
        semaphore.release(1);
      }));
    }
    semaphore.acquire(numTasks);
  }

  // --- Vertical pass: W_dst x H_src -> W_dst x H_dst ---
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
      pool->start(new ScalerTask([&interImg, &destImg, &vTaps, &vWeightPool,
                                   &vFixedTaps, vUseFixed, W_dst, H_src,
                                   y_start, y_end, &semaphore]() {
        if (vUseFixed)
          mksVerticalFixed(interImg, destImg, vFixedTaps, H_src, W_dst, y_start, y_end);
        else
          mksVerticalGeneral(interImg, destImg, vTaps, vWeightPool, H_src, W_dst, y_start, y_end);
        semaphore.release(1);
      }));
    }
    semaphore.acquire(numTasks);
  }

  if (workingPremultiplied)
    return destImg.convertToFormat(QImage::Format_ARGB32);
  return destImg;
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
