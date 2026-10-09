#include "referenceimages.h"

#include <QRect>
#include <QtGlobal>
#include <algorithm>
#include <cmath>
#include <vector>

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

// Magic Kernel Sharp 2021 (a = 3, v = 3), copied from utils/imagelib.cpp.
constexpr double kMks3C0 = 17.0 / 12.0;
constexpr double kMks3C1 = -35.0 / 144.0;
constexpr double kMks3C2 = 1.0 / 24.0;
constexpr double kMks3C3 = -1.0 / 144.0;
constexpr double kMks2021Support = 4.5;

double magicKernelA3(double x) {
  if (x <= -1.5 || x >= 1.5)
    return 0.0;
  const double x2 = x * x;
  if (x <= -0.5)
    return x2 / 2.0 + 1.5 * x + 9.0 / 8.0;
  if (x <= 0.5)
    return -x2 + 0.75;
  return x2 / 2.0 - 1.5 * x + 9.0 / 8.0;
}

double mks2021Kernel(double x) {
  if (x <= -kMks2021Support || x >= kMks2021Support)
    return 0.0;
  double sum = kMks3C0 * magicKernelA3(x);
  sum += kMks3C1 * (magicKernelA3(x - 1.0) + magicKernelA3(x + 1.0));
  sum += kMks3C2 * (magicKernelA3(x - 2.0) + magicKernelA3(x + 2.0));
  sum += kMks3C3 * (magicKernelA3(x - 3.0) + magicKernelA3(x + 3.0));
  return sum;
}

// Normalized taps of one output, as buildMksAxisTaps() builds them.
struct MksTaps {
  int first = 0;
  std::vector<double> weights;
};

MksTaps mksTaps(int sourceSize, int outputSize, int output) {
  const double scale = double(sourceSize) / double(outputSize);
  const double filterScale = std::max(scale, 1.0);
  const double support = kMks2021Support * filterScale;
  const double u = (output + kPixelCentre) * scale - kPixelCentre;
  MksTaps taps;
  taps.first = int(std::floor(u - support));
  const int last = std::max(taps.first, int(std::ceil(u + support)));
  double sum = 0.0;
  for (int i = taps.first; i <= last; ++i) {
    const double w = mks2021Kernel((u - i) / filterScale) / filterScale;
    taps.weights.push_back(w);
    sum += w;
  }
  if (sum != 0.0) {
    for (double &w : taps.weights)
      w /= sum;
  }
  return taps;
}

double quantized8Bit(double value) {
  return std::round(std::clamp(value, 0.0, 1.0) * kMaxLevel) / kMaxLevel;
}

// One MKS2021 pass along x (horizontal) or y, clamped to the image edges.
FloatImage mksPass(const FloatImage &image, int outputLength, bool horizontal) {
  const QSize source = image.size();
  const int sourceLength = horizontal ? source.width() : source.height();
  const QSize size = horizontal ? QSize(outputLength, source.height())
                                : QSize(source.width(), outputLength);
  FloatImage result(size);
  for (int o = 0; o < outputLength; ++o) {
    const MksTaps taps = mksTaps(sourceLength, outputLength, o);
    const int across = horizontal ? size.height() : size.width();
    for (int a = 0; a < across; ++a) {
      Rgba sum{};
      for (std::size_t t = 0; t < taps.weights.size(); ++t) {
        const int s = qBound(0, taps.first + int(t), sourceLength - 1);
        const Rgba &pixel = horizontal ? image.at(s, a) : image.at(a, s);
        for (int c = 0; c < kChannels; ++c)
          sum[c] += pixel[c] * taps.weights[t];
      }
      for (int c = 0; c < kChannels; ++c)
        sum[c] = std::clamp(sum[c], 0.0, 1.0);
      if (horizontal)
        result.at(o, a) = sum;
      else
        result.at(a, o) = sum;
    }
  }
  return result;
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

FloatImage mks2021Resample(const FloatImage &image, QSize target) {
  FloatImage rows = mksPass(image, target.width(), true);
  // The CPU and the GPU keep the intermediate image in 8 bits.
  for (int y = 0; y < rows.size().height(); ++y) {
    for (int x = 0; x < rows.size().width(); ++x) {
      for (int c = 0; c < kChannels; ++c)
        rows.at(x, y)[c] = quantized8Bit(rows.at(x, y)[c]);
    }
  }
  FloatImage result = mksPass(rows, target.height(), false);
  for (int y = 0; y < target.height(); ++y) {
    for (int x = 0; x < target.width(); ++x) {
      Rgba &pixel = result.at(x, y);
      for (int c = 0; c < kColorChannels; ++c)
        pixel[c] = std::min(pixel[c], pixel[kAlpha]);
    }
  }
  return result;
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

FloatImage panoramaFrame(const FloatImage &source, QSize frameSize,
                         const ReferencePanorama &camera,
                         const std::optional<ColorMatrix> &color) {
  constexpr double kPi = 3.14159265358979323846;
  constexpr double kDegrees = kPi / 180.0;
  const int width = source.size().width();
  const int height = source.size().height();
  const double tanHalfFov = std::tan(camera.fov * kDegrees * kPixelCentre);
  const double aspect = double(frameSize.width()) / frameSize.height();
  const double sinYaw = std::sin(camera.yaw * kDegrees);
  const double cosYaw = std::cos(camera.yaw * kDegrees);
  const double sinPitch = std::sin(camera.pitch * kDegrees);
  const double cosPitch = std::cos(camera.pitch * kDegrees);
  FloatImage frame(frameSize);
  for (int y = 0; y < frameSize.height(); ++y) {
    for (int x = 0; x < frameSize.width(); ++x) {
      const double sx = 2.0 * (x + kPixelCentre) / frameSize.width() - 1.0;
      const double sy = 2.0 * (y + kPixelCentre) / frameSize.height() - 1.0;
      double rx = sx * aspect * tanHalfFov;
      double ry = -sy * tanHalfFov;
      double rz = 1.0;
      const double length = std::sqrt(rx * rx + ry * ry + rz * rz);
      rx /= length;
      ry /= length;
      rz /= length;
      const double py = ry * cosPitch - rz * sinPitch;
      const double pz = ry * sinPitch + rz * cosPitch;
      const double yx = rx * cosYaw + pz * sinYaw;
      const double yz = -rx * sinYaw + pz * cosYaw;
      const double lon = std::atan2(yx, yz);
      const double lat = std::asin(std::clamp(py, -1.0, 1.0));
      const double u = (0.5 + lon / (2.0 * kPi)) * width - kPixelCentre;
      const double v = (0.5 - lat / kPi) * height - kPixelCentre;

      const int x0 = static_cast<int>(std::floor(u));
      const int y0 = static_cast<int>(std::floor(v));
      const double fx = u - x0;
      const double fy = v - y0;
      const auto texel = [&](int tx, int ty) -> const Rgba & {
        const int wrapped = ((tx % width) + width) % width;
        return source.at(wrapped, std::clamp(ty, 0, height - 1));
      };
      Rgba pixel{};
      for (int c = 0; c < kChannels; ++c) {
        const double top = texel(x0, y0)[c] * (1.0 - fx) +
                           texel(x0 + 1, y0)[c] * fx;
        const double bottom = texel(x0, y0 + 1)[c] * (1.0 - fx) +
                              texel(x0 + 1, y0 + 1)[c] * fx;
        pixel[c] = top * (1.0 - fy) + bottom * fy;
      }
      if (color) {
        const ColorMatrix &m = *color;
        const Rgba straight = pixel;
        for (int row = 0; row < kColorChannels; ++row) {
          const double value = m.m[row][0] * straight[0] +
                               m.m[row][1] * straight[1] +
                               m.m[row][2] * straight[2] + m.offset;
          pixel[row] = std::clamp(value, 0.0, 1.0);
        }
      }
      frame.at(x, y) = pixel;
    }
  }
  return frame;
}
