#include "referenceimages.h"

#include <QRect>
#include <QtGlobal>
#include <algorithm>
#include <cmath>

namespace {
using namespace Qt::StringLiterals;

constexpr double kMaxLevel = 255.0;
constexpr int kChannels = 4;
constexpr int kAlpha = 3;
constexpr double kPixelCentre = 0.5;

// Constants of res/shaders/rhi/image.frag and boxreduce.frag.
constexpr double kLuma[3] = {0.2126, 0.7152, 0.0722};
constexpr double kMinLuma = 1e-5;
constexpr double kOpaqueAlphaThreshold = 0.9999;
constexpr double kMinUnpremultiplyAlpha = 0.0001;
constexpr double kSharpenEpsilon = 0.001;
constexpr double kCasPeakContrastSlope = -3.0;
constexpr double kCasPeakBase = 8.0;
constexpr double kCasWindowTaps = 4.0;
constexpr double kCasRangeSum = 2.0;
constexpr double kSmartLaplacianCentre = 4.0;
constexpr double kSmartLaplacianAmount = 0.0625;
constexpr int kReduceDivisor = 2;
constexpr int kColorChannels = 3;
constexpr int kCasWindowSize = 9;

Rgba sampleBilinear(const FloatImage &image, double u, double v) {
  // u, v in texel units with texel centres at whole numbers.
  const int x0 = static_cast<int>(std::floor(u));
  const int y0 = static_cast<int>(std::floor(v));
  const double fx = u - x0;
  const double fy = v - y0;
  const int maxX = image.size().width() - 1;
  const int maxY = image.size().height() - 1;
  const int xa = qBound(0, x0, maxX);
  const int xb = qBound(0, x0 + 1, maxX);
  const int ya = qBound(0, y0, maxY);
  const int yb = qBound(0, y0 + 1, maxY);
  Rgba result{};
  for (int c = 0; c < kChannels; ++c) {
    const double top =
        image.at(xa, ya)[c] * (1.0 - fx) + image.at(xb, ya)[c] * fx;
    const double bottom =
        image.at(xa, yb)[c] * (1.0 - fx) + image.at(xb, yb)[c] * fx;
    result[c] = top * (1.0 - fy) + bottom * fy;
  }
  return result;
}

using Rgb = std::array<double, 3>;

Rgb rgbOf(const Rgba &pixel) { return {pixel[0], pixel[1], pixel[2]}; }

double luma(const Rgb &rgb) {
  return rgb[0] * kLuma[0] + rgb[1] * kLuma[1] + rgb[2] * kLuma[2];
}

// taps in row-major order: a b c / d e f / g h i.
Rgb casFromWindow(const std::array<Rgb, kCasWindowSize> &taps,
                  double sharpening, double contrast) {
  const Rgb &a = taps[0], &b = taps[1], &c = taps[2], &d = taps[3],
            &e = taps[4], &f = taps[5], &g = taps[6], &h = taps[7],
            &i = taps[8];
  Rgb mnSum{}, mxSum{};
  for (int ch = 0; ch < kColorChannels; ++ch) {
    const double mnCross = std::min({d[ch], e[ch], f[ch], b[ch], h[ch]});
    const double mxCross = std::max({d[ch], e[ch], f[ch], b[ch], h[ch]});
    const double mnAll = std::min({mnCross, a[ch], c[ch], g[ch], i[ch]});
    const double mxAll = std::max({mxCross, a[ch], c[ch], g[ch], i[ch]});
    mnSum[ch] = mnCross + mnAll;
    mxSum[ch] = mxCross + mxAll;
  }
  const double mnL = luma(mnSum);
  const double mxL = luma(mxSum);
  const double rcpM = 1.0 / std::max(mxL, kMinLuma);
  double amp = std::clamp(std::min(mnL, kCasRangeSum - mxL) * rcpM, 0.0, 1.0);
  amp = 1.0 / std::sqrt(std::max(amp, kMinLuma));
  const double peak = kCasPeakContrastSlope * contrast + kCasPeakBase;
  const double w = -1.0 / (amp * peak);
  const double rcpWeight = 1.0 / (kCasWindowTaps * w + 1.0);
  Rgb result{};
  for (int ch = 0; ch < kColorChannels; ++ch) {
    const double window = b[ch] + d[ch] + f[ch] + h[ch];
    const double sharpened =
        std::clamp((window * w + e[ch]) * rcpWeight, 0.0, 1.0);
    result[ch] = e[ch] + (sharpened - e[ch]) * sharpening;
  }
  return result;
}

Rgb smartPlain(const Rgb &centre, const Rgb &top, const Rgb &bottom,
               const Rgb &left, const Rgb &right) {
  const double lap = kSmartLaplacianCentre * luma(centre) - luma(top) -
                     luma(bottom) - luma(left) - luma(right);
  Rgb result{};
  for (int ch = 0; ch < kColorChannels; ++ch)
    result[ch] = std::clamp(centre[ch] + lap * kSmartLaplacianAmount, 0.0, 1.0);
  return result;
}

double axisOverlap(double srcIndex, double center, double halfFootprint) {
  const double lo = std::max(srcIndex - kPixelCentre, center - halfFootprint);
  const double hi = std::min(srcIndex + kPixelCentre, center + halfFootprint);
  return std::max(hi - lo, 0.0);
}

// One exact-area pass from image to size (each side at least half).
FloatImage boxReducePass(const FloatImage &image, QSize size) {
  const QSize source = image.size();
  const double ratioX = double(size.width()) / source.width();
  const double ratioY = double(size.height()) / source.height();
  const double halfX = kPixelCentre / ratioX;
  const double halfY = kPixelCentre / ratioY;
  FloatImage result(size);
  for (int y = 0; y < size.height(); ++y) {
    const double centerY = (y + kPixelCentre) / ratioY - kPixelCentre;
    const int firstY = int(std::floor(centerY - halfY));
    const int lastY = int(std::ceil(centerY + halfY));
    for (int x = 0; x < size.width(); ++x) {
      const double centerX = (x + kPixelCentre) / ratioX - kPixelCentre;
      const int firstX = int(std::floor(centerX - halfX));
      const int lastX = int(std::ceil(centerX + halfX));
      Rgba sum{};
      double weightSum = 0.0;
      for (int sy = firstY; sy <= lastY; ++sy) {
        const double wy = axisOverlap(sy, centerY, halfY);
        for (int sx = firstX; sx <= lastX; ++sx) {
          const double w = axisOverlap(sx, centerX, halfX) * wy;
          if (w <= 0.0)
            continue;
          const Rgba &pixel = image.at(qBound(0, sx, source.width() - 1),
                                       qBound(0, sy, source.height() - 1));
          for (int c = 0; c < kChannels; ++c)
            sum[c] += pixel[c] * w;
          weightSum += w;
        }
      }
      for (int c = 0; c < kChannels; ++c)
        sum[c] /= weightSum;
      result.at(x, y) = sum;
    }
  }
  return result;
}

int nextReduceStep(int current, int target) {
  return current == target
             ? target
             : qMax(target, (current + kReduceDivisor - 1) / kReduceDivisor);
}

Rgba sampleMagnified(const FloatImage &image, int x, int y,
                     const ReferenceScene &scene) {
  const double u = (x + kPixelCentre) / scene.scale.magnification;
  const double v = (y + kPixelCentre) / scene.scale.magnification;
  if (scene.filter == ReferenceFilter::Nearest) {
    return image.at(static_cast<int>(std::floor(u)),
                    static_cast<int>(std::floor(v)));
  }
  return sampleBilinear(image, u - kPixelCentre, v - kPixelCentre);
}
} // namespace

FloatImage::FloatImage(QSize size)
    : mSize(size), mPixels(std::size_t(size.width()) * size.height()) {}

QSize FloatImage::size() const { return mSize; }

const Rgba &FloatImage::at(int x, int y) const {
  return mPixels[std::size_t(y) * mSize.width() + x];
}

Rgba &FloatImage::at(int x, int y) {
  return mPixels[std::size_t(y) * mSize.width() + x];
}

FloatImage premultipliedSource(const QImage &image) {
  FloatImage result(image.size());
  for (int y = 0; y < image.height(); ++y) {
    for (int x = 0; x < image.width(); ++x) {
      const QColor color = image.pixelColor(x, y);
      const double alpha = color.alphaF();
      result.at(x, y) = {color.redF() * alpha, color.greenF() * alpha,
                         color.blueF() * alpha, alpha};
    }
  }
  return result;
}

FloatImage boxReduce(const FloatImage &image, int factor) {
  const QSize size = image.size() / factor;
  const double area = double(factor) * factor;
  FloatImage result(size);
  for (int y = 0; y < size.height(); ++y) {
    for (int x = 0; x < size.width(); ++x) {
      Rgba sum{};
      for (int dy = 0; dy < factor; ++dy) {
        for (int dx = 0; dx < factor; ++dx) {
          const Rgba &pixel = image.at(x * factor + dx, y * factor + dy);
          for (int c = 0; c < kChannels; ++c)
            sum[c] += pixel[c];
        }
      }
      for (int c = 0; c < kChannels; ++c)
        sum[c] /= area;
      result.at(x, y) = sum;
    }
  }
  return result;
}

FloatImage boxReduceGrayscale8(const QImage &image, int factor) {
  const QSize size = image.size() / factor;
  std::vector<quint64> sums(std::size_t(size.width()) * size.height());
  for (int y = 0; y < size.height() * factor; ++y) {
    const uchar *line = image.constScanLine(y);
    quint64 *row = sums.data() + std::size_t(y / factor) * size.width();
    for (int x = 0; x < size.width() * factor; ++x)
      row[x / factor] += line[x];
  }
  const double scale = kMaxLevel * factor * factor;
  FloatImage result(size);
  for (int y = 0; y < size.height(); ++y) {
    for (int x = 0; x < size.width(); ++x) {
      const double gray = sums[std::size_t(y) * size.width() + x] / scale;
      result.at(x, y) = {gray, gray, gray, 1.0};
    }
  }
  return result;
}

FloatImage composeOver(const FloatImage &drawn, QSize frameSize,
                       QPoint origin, const QColor &background) {
  const Rgba back{background.redF(), background.greenF(), background.blueF(),
                  1.0};
  FloatImage frame(frameSize);
  const QRect drawnRect(origin, drawn.size());
  for (int y = 0; y < frameSize.height(); ++y) {
    for (int x = 0; x < frameSize.width(); ++x) {
      if (!drawnRect.contains(x, y)) {
        frame.at(x, y) = back;
        continue;
      }
      const Rgba &pixel = drawn.at(x - origin.x(), y - origin.y());
      Rgba composed{};
      for (int c = 0; c < kChannels; ++c)
        composed[c] = pixel[c] + back[c] * (1.0 - pixel[kAlpha]);
      frame.at(x, y) = composed;
    }
  }
  return frame;
}

FloatImage exactReduce(const FloatImage &image, QSize target) {
  FloatImage current = image;
  while (current.size() != target) {
    const QSize next(nextReduceStep(current.size().width(), target.width()),
                     nextReduceStep(current.size().height(), target.height()));
    current = boxReducePass(current, next);
  }
  return current;
}

FloatImage filteredMagnified(const FloatImage &image, int magnification,
                             const ReferenceFilterParams &params) {
  const QSize size = image.size() * magnification;
  const double step = 1.0 / magnification;
  // Sample at a texel-space position (texel centres at k + 0.5).
  const auto sample = [&image](double u, double v) {
    return sampleBilinear(image, u - kPixelCentre, v - kPixelCentre);
  };
  FloatImage result(size);
  for (int y = 0; y < size.height(); ++y) {
    for (int x = 0; x < size.width(); ++x) {
      const double u = (x + kPixelCentre) * step;
      const double v = (y + kPixelCentre) * step;
      const Rgba centre = sample(u, v);
      Rgb rgb = rgbOf(centre);
      const double alpha = centre[kAlpha];

      Rgb sharpened = rgb;
      if (params.sharpening == ReferenceSharpening::Cas &&
          params.casSharpening > kSharpenEpsilon) {
        std::array<Rgb, kCasWindowSize> taps{};
        std::size_t index = 0;
        for (int dy = -1; dy <= 1; ++dy) {
          for (int dx = -1; dx <= 1; ++dx)
            taps[index++] = rgbOf(sample(u + dx * step, v + dy * step));
        }
        sharpened =
            casFromWindow(taps, params.casSharpening, params.casContrast);
      } else if (params.sharpening == ReferenceSharpening::Smart) {
        sharpened = smartPlain(rgb, rgbOf(sample(u, v - step)),
                               rgbOf(sample(u, v + step)),
                               rgbOf(sample(u - step, v)),
                               rgbOf(sample(u + step, v)));
      }
      if (alpha >= kOpaqueAlphaThreshold)
        rgb = sharpened;

      if (params.color) {
        const ColorMatrix &m = *params.color;
        Rgb straight = rgb;
        if (alpha > kMinUnpremultiplyAlpha) {
          for (double &channel : straight)
            channel /= alpha;
        }
        for (int row = 0; row < kColorChannels; ++row) {
          const double value = m.m[row][0] * straight[0] +
                               m.m[row][1] * straight[1] +
                               m.m[row][2] * straight[2] + m.offset;
          rgb[row] = std::clamp(value, 0.0, 1.0) * alpha;
        }
      }
      result.at(x, y) = {rgb[0], rgb[1], rgb[2], alpha};
    }
  }
  return result;
}

FloatImage expectedFrame(const FloatImage &source,
                         const ReferenceScene &scene) {
  const FloatImage image = scene.scale.reduction > 1
                               ? boxReduce(source, scene.scale.reduction)
                               : source;
  const QSize drawn = image.size() * scene.scale.magnification;
  const Rgba background{scene.background.redF(), scene.background.greenF(),
                        scene.background.blueF(), 1.0};
  FloatImage frame(scene.frameSize);
  for (int y = 0; y < scene.frameSize.height(); ++y) {
    for (int x = 0; x < scene.frameSize.width(); ++x) {
      const int ix = x - scene.origin.x();
      const int iy = y - scene.origin.y();
      if (ix < 0 || iy < 0 || ix >= drawn.width() || iy >= drawn.height()) {
        frame.at(x, y) = background;
        continue;
      }
      const Rgba pixel = scene.scale.magnification > 1
                             ? sampleMagnified(image, ix, iy, scene)
                             : image.at(ix, iy);
      Rgba composed{};
      for (int c = 0; c < kChannels; ++c)
        composed[c] = pixel[c] + background[c] * (1.0 - pixel[kAlpha]);
      frame.at(x, y) = composed;
    }
  }
  return frame;
}

int maxLevelDifference(const QImage &actual, const FloatImage &expected,
                       QString *where) {
  if (actual.size() != expected.size() ||
      actual.format() != QImage::Format_RGBA8888_Premultiplied) {
    *where = u"frame size or format mismatch"_s;
    return int(kMaxLevel);
  }
  int worst = 0;
  for (int y = 0; y < actual.height(); ++y) {
    const uchar *line = actual.constScanLine(y);
    for (int x = 0; x < actual.width(); ++x) {
      for (int c = 0; c < kChannels; ++c) {
        const int expectedLevel =
            int(std::lround(expected.at(x, y)[c] * kMaxLevel));
        const int difference =
            std::abs(int(line[x * kChannels + c]) - expectedLevel);
        if (difference > worst) {
          worst = difference;
          *where = u"pixel (%1, %2) channel %3: actual %4, expected %5"_s
                       .arg(x)
                       .arg(y)
                       .arg(c)
                       .arg(line[x * kChannels + c])
                       .arg(expectedLevel);
        }
      }
    }
  }
  return worst;
}
