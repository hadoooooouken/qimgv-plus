#include "resamplegrid.h"

#include <QtGlobal>
#include <algorithm>
#include <cmath>

namespace {
constexpr double kPixelCentre = 0.5;
constexpr double kNoReduction = 1.0;
constexpr int kMinimumOutputSide = 1;

// Magic Kernel Sharp 2021, as in utils/imagelib.cpp. Reference:
// johncostella.com/magic. k(x) = sum_{s=-3}^{3} c_s * m3(x + s), where m3 is
// the Magic Kernel for a = 3 (piecewise quadratic, support (-1.5, 1.5)) and
// c_s are the Magic Sharp coefficients for a = 3, v = 3.
constexpr double kMks3C0 = 17.0 / 12.0;
constexpr double kMks3C1 = -35.0 / 144.0;
constexpr double kMks3C2 = 1.0 / 24.0;
constexpr double kMks3C3 = -1.0 / 144.0;
constexpr double kMagicSupport = 1.5;
constexpr double kMagicInner = 0.5;

double magicKernelA3(double x) {
  if (x <= -kMagicSupport || x >= kMagicSupport)
    return 0.0;
  const double x2 = x * x;
  if (x <= -kMagicInner)
    return x2 / 2.0 + 1.5 * x + 9.0 / 8.0;
  if (x <= kMagicInner)
    return -x2 + 0.75;
  return x2 / 2.0 - 1.5 * x + 9.0 / 8.0;
}
} // namespace

double Mks2021Axis::kernel(double x) {
  if (x <= -kSupport || x >= kSupport)
    return 0.0;
  double sum = kMks3C0 * magicKernelA3(x);
  sum += kMks3C1 * (magicKernelA3(x - 1.0) + magicKernelA3(x + 1.0));
  sum += kMks3C2 * (magicKernelA3(x - 2.0) + magicKernelA3(x + 2.0));
  sum += kMks3C3 * (magicKernelA3(x - 3.0) + magicKernelA3(x + 3.0));
  return sum;
}

double Mks2021Axis::ratio() const {
  return static_cast<double>(sourceSize) / static_cast<double>(outputSize);
}

double Mks2021Axis::filterScale() const {
  return std::max(ratio(), kNoReduction);
}

double Mks2021Axis::support() const { return kSupport * filterScale(); }

double Mks2021Axis::centre(int output) const {
  return (output + kPixelCentre) * ratio() - kPixelCentre;
}

int Mks2021Axis::firstTap(int output) const {
  return static_cast<int>(std::floor(centre(output) - support()));
}

int Mks2021Axis::lastTap(int output) const {
  return std::max(firstTap(output),
                  static_cast<int>(std::ceil(centre(output) + support())));
}

std::vector<double> Mks2021Axis::weights(int output) const {
  const double u = centre(output);
  const double scale = filterScale();
  const int first = firstTap(output);
  const int last = lastTap(output);
  std::vector<double> result;
  result.reserve(static_cast<std::size_t>(last - first + 1));
  double sum = 0.0;
  for (int i = first; i <= last; ++i) {
    const double w = kernel((u - i) / scale) / scale;
    result.push_back(w);
    sum += w;
  }
  // Same re-normalization as the CPU: the cut-off at integer bounds must not
  // change the brightness.
  if (sum != 0.0) {
    for (double &w : result)
      w /= sum;
  }
  return result;
}

int Mks2021Axis::firstOwnedOutput(int sourceBegin) const {
  // The centre of output x is at (x + 0.5) * ratio in pixel-edge coordinates;
  // the first x with (x + 0.5) * ratio >= sourceBegin.
  const double first = std::ceil(sourceBegin / ratio() - kPixelCentre);
  return static_cast<int>(std::clamp(first, 0.0, double(outputSize)));
}

QSize ResampleGrid::outputSize(QSize imageSize, double scale) {
  return QSize(qMax(kMinimumOutputSide, qRound(imageSize.width() * scale)),
               qMax(kMinimumOutputSide, qRound(imageSize.height() * scale)));
}

QRect ResampleGrid::visibleTileOutputs(const ImageTile &tile, QSize imageSize,
                                       QSize outputSize, QPoint origin,
                                       QSize targetSize) {
  const Mks2021Axis x{imageSize.width(), outputSize.width()};
  const Mks2021Axis y{imageSize.height(), outputSize.height()};
  const QRect owned(
      QPoint(x.firstOwnedOutput(tile.core.left()),
             y.firstOwnedOutput(tile.core.top())),
      QPoint(x.firstOwnedOutput(tile.core.left() + tile.core.width()) - 1,
             y.firstOwnedOutput(tile.core.top() + tile.core.height()) - 1));
  const QRect visible(-origin, targetSize);
  const QRect outputs = owned.intersected(visible);
  return outputs.isEmpty() ? QRect() : outputs;
}
