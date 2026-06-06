#include "imagelib.h"
#include <thread>
#include <vector>
#include <cmath>
#include <algorithm>
#include <immintrin.h>

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



QImage *ImageLib::scaled_Smart(std::shared_ptr<const QImage> source,
                               QSize destSize) {
  if (!source || source->isNull())
    return new QImage();

  int W_src = source->width();
  int H_src = source->height();
  int W_dst = destSize.width();
  int H_dst = destSize.height();

  if (W_dst <= 0 || H_dst <= 0)
    return new QImage();

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
      hWeights[x].w0 = -0.5f * dx * dx * dx + dx * dx - 0.5f * dx;
      hWeights[x].w1 = 1.5f * dx * dx * dx - 2.5f * dx * dx + 1.0f;
      hWeights[x].w2 = -1.5f * dx * dx * dx + 2.0f * dx * dx + 0.5f * dx;
      hWeights[x].w3 = 0.5f * dx * dx * dx - 0.5f * dx * dx;
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
      vWeights[y].w0 = -0.5f * dy * dy * dy + dy * dy - 0.5f * dy;
      vWeights[y].w1 = 1.5f * dy * dy * dy - 2.5f * dy * dy + 1.0f;
      vWeights[y].w2 = -1.5f * dy * dy * dy + 2.0f * dy * dy + 0.5f * dy;
      vWeights[y].w3 = 0.5f * dy * dy * dy - 0.5f * dy * dy;
    }

    // Horizontal pass: W_src x H_src -> W_dst x H_src
    QImage interImg(W_dst, H_src, srcImg.format());
    
    // Determine thread count
    unsigned int numThreads = std::thread::hardware_concurrency();
    if (numThreads == 0) numThreads = 4;

    {
      std::vector<std::thread> threads;
      int rowsPerThread = H_src / numThreads;
      if (rowsPerThread == 0) rowsPerThread = 1;

      for (unsigned int i = 0; i < numThreads; ++i) {
        int y_start = i * rowsPerThread;
        int y_end = (i == numThreads - 1) ? H_src : (i + 1) * rowsPerThread;
        if (y_start >= H_src) break;

        threads.emplace_back([&srcImg, &interImg, &hWeights, W_dst, y_start, y_end]() {
          for (int y = y_start; y < y_end; ++y) {
            const uint32_t* srcRow = (const uint32_t*)srcImg.constScanLine(y);
            uint32_t* interRow = (uint32_t*)interImg.scanLine(y);
            for (int x = 0; x < W_dst; ++x) {
              const auto& hw = hWeights[x];
              uint32_t p0 = srcRow[hw.x0];
              uint32_t p1 = srcRow[hw.x1];
              uint32_t p2 = srcRow[hw.x2];
              uint32_t p3 = srcRow[hw.x3];

              float b = hw.w0 * (p0 & 0xFF) + hw.w1 * (p1 & 0xFF) + hw.w2 * (p2 & 0xFF) + hw.w3 * (p3 & 0xFF);
              float g = hw.w0 * ((p0 >> 8) & 0xFF) + hw.w1 * ((p1 >> 8) & 0xFF) + hw.w2 * ((p2 >> 8) & 0xFF) + hw.w3 * ((p3 >> 8) & 0xFF);
              float r = hw.w0 * ((p0 >> 16) & 0xFF) + hw.w1 * ((p1 >> 16) & 0xFF) + hw.w2 * ((p2 >> 16) & 0xFF) + hw.w3 * ((p3 >> 16) & 0xFF);
              float a = hw.w0 * ((p0 >> 24) & 0xFF) + hw.w1 * ((p1 >> 24) & 0xFF) + hw.w2 * ((p2 >> 24) & 0xFF) + hw.w3 * ((p3 >> 24) & 0xFF);

              uint8_t ub = std::clamp(b + 0.5f, 0.0f, 255.0f);
              uint8_t ug = std::clamp(g + 0.5f, 0.0f, 255.0f);
              uint8_t ur = std::clamp(r + 0.5f, 0.0f, 255.0f);
              uint8_t ua = std::clamp(a + 0.5f, 0.0f, 255.0f);

              interRow[x] = (ua << 24) | (ur << 16) | (ug << 8) | ub;
            }
          }
        });
      }
      for (auto& t : threads) {
        t.join();
      }
    }

    // Vertical pass + Cross-kernel Sharpening combined in one step
    QImage *destImg = new QImage(W_dst, H_dst, srcImg.format());

    {
      std::vector<std::thread> threads;
      int rowsPerThread = H_dst / numThreads;
      if (rowsPerThread == 0) rowsPerThread = 1;

      for (unsigned int i = 0; i < numThreads; ++i) {
        int y_start = i * rowsPerThread;
        int y_end = (i == numThreads - 1) ? H_dst : (i + 1) * rowsPerThread;
        if (y_start >= H_dst) break;

        threads.emplace_back([&interImg, destImg, &vWeights, W_dst, H_dst, y_start, y_end]() {
          // Thread-local sliding window buffer
          std::vector<std::vector<uint32_t>> rowBuffers(3, std::vector<uint32_t>(W_dst));

          auto fillRowBuffer = [&](int yd, int bufIdx) {
            int yd_clamped = std::clamp(yd, 0, H_dst - 1);
            const auto& vw = vWeights[yd_clamped];
            const uint32_t* r0 = (const uint32_t*)interImg.constScanLine(vw.y0);
            const uint32_t* r1 = (const uint32_t*)interImg.constScanLine(vw.y1);
            const uint32_t* r2 = (const uint32_t*)interImg.constScanLine(vw.y2);
            const uint32_t* r3 = (const uint32_t*)interImg.constScanLine(vw.y3);
            uint32_t* out = rowBuffers[bufIdx].data();

            for (int x = 0; x < W_dst; ++x) {
              uint32_t p0 = r0[x];
              uint32_t p1 = r1[x];
              uint32_t p2 = r2[x];
              uint32_t p3 = r3[x];

              float b = vw.w0 * (p0 & 0xFF) + vw.w1 * (p1 & 0xFF) + vw.w2 * (p2 & 0xFF) + vw.w3 * (p3 & 0xFF);
              float g = vw.w0 * ((p0 >> 8) & 0xFF) + vw.w1 * ((p1 >> 8) & 0xFF) + vw.w2 * ((p2 >> 8) & 0xFF) + vw.w3 * ((p3 >> 8) & 0xFF);
              float r = vw.w0 * ((p0 >> 16) & 0xFF) + vw.w1 * ((p1 >> 16) & 0xFF) + vw.w2 * ((p2 >> 16) & 0xFF) + vw.w3 * ((p3 >> 16) & 0xFF);
              float a = vw.w0 * ((p0 >> 24) & 0xFF) + vw.w1 * ((p1 >> 24) & 0xFF) + vw.w2 * ((p2 >> 24) & 0xFF) + vw.w3 * ((p3 >> 24) & 0xFF);

              uint8_t ub = std::clamp(b + 0.5f, 0.0f, 255.0f);
              uint8_t ug = std::clamp(g + 0.5f, 0.0f, 255.0f);
              uint8_t ur = std::clamp(r + 0.5f, 0.0f, 255.0f);
              uint8_t ua = std::clamp(a + 0.5f, 0.0f, 255.0f);

              out[x] = (ua << 24) | (ur << 16) | (ug << 8) | ub;
            }
          };

          // Prime window
          fillRowBuffer(y_start - 1, 0);
          fillRowBuffer(y_start, 1);
          fillRowBuffer(y_start + 1, 2);

          for (int y = y_start; y < y_end; ++y) {
            uint32_t* dstRow = (uint32_t*)destImg->scanLine(y);

            int idxT = (y - y_start) % 3;
            int idxC = (y - y_start + 1) % 3;
            int idxB = (y - y_start + 2) % 3;

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

              // sum = 4*C - T - B - L - R
              __m256i sum_lo = _mm256_slli_epi16(C_lo, 2);
              sum_lo = _mm256_sub_epi16(sum_lo, T_lo);
              sum_lo = _mm256_sub_epi16(sum_lo, B_lo);
              sum_lo = _mm256_sub_epi16(sum_lo, L_lo);
              sum_lo = _mm256_sub_epi16(sum_lo, R_lo);
              __m256i diff_lo = _mm256_srai_epi16(sum_lo, 4);
              __m256i res_lo = _mm256_add_epi16(C_lo, diff_lo);

              __m256i sum_hi = _mm256_slli_epi16(C_hi, 2);
              sum_hi = _mm256_sub_epi16(sum_hi, T_hi);
              sum_hi = _mm256_sub_epi16(sum_hi, B_hi);
              sum_hi = _mm256_sub_epi16(sum_hi, L_hi);
              sum_hi = _mm256_sub_epi16(sum_hi, R_hi);
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
        });
      }
      for (auto& t : threads) {
        t.join();
      }
    }

    return destImg;

  } else {
    // --- 2. Downscaling ---
    // Base resize using Qt bilinear scaling
    QImage scaledImg = srcImg.scaled(destSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    // Precompute 1D Gaussian kernel coefficients for sigma = 2.0 (kernel size 9)
    static const float gWeights[9] = {
      0.02763f, 0.06628f, 0.12384f, 0.18017f, 0.20416f, 0.18017f, 0.12384f, 0.06628f, 0.02763f
    };

    QImage *destImg = new QImage(W_dst, H_dst, srcImg.format());

    // Determine thread count
    unsigned int numThreads = std::thread::hardware_concurrency();
    if (numThreads == 0) numThreads = 4;

    {
      std::vector<std::thread> threads;
      int rowsPerThread = H_dst / numThreads;
      if (rowsPerThread == 0) rowsPerThread = 1;

      for (unsigned int i = 0; i < numThreads; ++i) {
        int y_start = i * rowsPerThread;
        int y_end = (i == numThreads - 1) ? H_dst : (i + 1) * rowsPerThread;
        if (y_start >= H_dst) break;

        threads.emplace_back([&scaledImg, destImg, W_dst, H_dst, y_start, y_end]() {
          // Thread-local sliding window buffer: stores 9 rows of horizontally blurred floats
          std::vector<std::vector<float>> rowBuffers(9, std::vector<float>(W_dst * 4));

          auto blurRowHorizontal = [&](int yd, float* outRow) {
            int yd_clamped = std::clamp(yd, 0, H_dst - 1);
            const uint32_t* srcRow = (const uint32_t*)scaledImg.constScanLine(yd_clamped);
            for (int x = 0; x < W_dst; ++x) {
              float sumB = 0.0f, sumG = 0.0f, sumR = 0.0f, sumA = 0.0f;
              for (int k = -4; k <= 4; ++k) {
                int sx = std::clamp(x + k, 0, W_dst - 1);
                uint32_t p = srcRow[sx];
                float kw = gWeights[k + 4];
                sumB += kw * (p & 0xFF);
                sumG += kw * ((p >> 8) & 0xFF);
                sumR += kw * ((p >> 16) & 0xFF);
                sumA += kw * ((p >> 24) & 0xFF);
              }
              outRow[x * 4 + 0] = sumB;
              outRow[x * 4 + 1] = sumG;
              outRow[x * 4 + 2] = sumR;
              outRow[x * 4 + 3] = sumA;
            }
          };

          // Prime window
          for (int r = 0; r < 9; ++r) {
            blurRowHorizontal(y_start - 4 + r, rowBuffers[r].data());
          }

          for (int y = y_start; y < y_end; ++y) {
            uint32_t* dstRow = (uint32_t*)destImg->scanLine(y);
            const uint32_t* origRow = (const uint32_t*)scaledImg.constScanLine(y);

            // Compute vertical Gaussian blur + Unsharp Mask blending on the fly
            for (int x = 0; x < W_dst; ++x) {
              float sumB = 0.0f, sumG = 0.0f, sumR = 0.0f, sumA = 0.0f;
              for (int k = 0; k < 9; ++k) {
                int bufIdx = (y - y_start + k) % 9;
                float kw = gWeights[k];
                const float* r = rowBuffers[bufIdx].data();
                sumB += kw * r[x * 4 + 0];
                sumG += kw * r[x * 4 + 1];
                sumR += kw * r[x * 4 + 2];
                sumA += kw * r[x * 4 + 3];
              }

              uint32_t origP = origRow[x];
              float origB = origP & 0xFF;
              float origG = (origP >> 8) & 0xFF;
              float origR = (origP >> 16) & 0xFF;
              float origA = (origP >> 24) & 0xFF;

              // strength: 0.15 unsharp mask -> 1.15 * original - 0.15 * blurred
              float finalB = 1.15f * origB - 0.15f * sumB;
              float finalG = 1.15f * origG - 0.15f * sumG;
              float finalR = 1.15f * origR - 0.15f * sumR;
              float finalA = 1.15f * origA - 0.15f * sumA;

              uint8_t ub = std::clamp(finalB + 0.5f, 0.0f, 255.0f);
              uint8_t ug = std::clamp(finalG + 0.5f, 0.0f, 255.0f);
              uint8_t ur = std::clamp(finalR + 0.5f, 0.0f, 255.0f);
              uint8_t ua = std::clamp(finalA + 0.5f, 0.0f, 255.0f);

              dstRow[x] = (ua << 24) | (ur << 16) | (ug << 8) | ub;
            }

            // Advance window
            if (y < H_dst - 1) {
              int idx_to_replace = (y - y_start) % 9;
              blurRowHorizontal(y + 5, rowBuffers[idx_to_replace].data());
            }
          }
        });
      }
      for (auto& t : threads) {
        t.join();
      }
    }

    return destImg;
  }
}

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

