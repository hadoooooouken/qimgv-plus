#include "referenceimages.h"

#include <QtGlobal>
#include <cmath>

namespace {
using namespace Qt::StringLiterals;

constexpr double kMaxLevel = 255.0;
constexpr int kChannels = 4;
constexpr int kAlpha = 3;
constexpr double kPixelCentre = 0.5;

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
