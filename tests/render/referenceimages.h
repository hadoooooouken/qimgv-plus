#pragma once

#include <QColor>
#include <QImage>
#include <QPoint>
#include <QSize>
#include <QString>
#include <array>
#include <vector>

// CPU references for the GPU renderer's pixel tests. All colours are
// premultiplied RGBA in [0, 1].

using Rgba = std::array<double, 4>;

class FloatImage {
public:
  FloatImage() = default;
  explicit FloatImage(QSize size);

  [[nodiscard]] QSize size() const;
  [[nodiscard]] const Rgba &at(int x, int y) const;
  [[nodiscard]] Rgba &at(int x, int y);

private:
  QSize mSize;
  std::vector<Rgba> mPixels;
};

// Premultiplied copy of image, from the exact channel values of its format.
[[nodiscard]] FloatImage premultipliedSource(const QImage &image);

// Average of factor x factor blocks; the image size must be divisible by
// factor. A mip level of a power-of-two factor equals this average.
[[nodiscard]] FloatImage boxReduce(const FloatImage &image, int factor);
// Same for a Format_Grayscale8 image, without a full-size float copy.
[[nodiscard]] FloatImage boxReduceGrayscale8(const QImage &image, int factor);

enum class ReferenceFilter { Nearest, Bilinear };

// Integer scale of the image on screen: magnification / reduction.
struct ReferenceScale {
  int magnification = 1;
  int reduction = 1;
};

struct ReferenceScene {
  QSize frameSize;
  // Image top-left corner in device pixels.
  QPoint origin;
  ReferenceScale scale;
  ReferenceFilter filter = ReferenceFilter::Bilinear;
  // Opaque.
  QColor background;
};

// Expected frame: the background with source (premultiplied) drawn over it.
// Reductions use box averages (exact mip levels), magnifications nearest or
// clamp-to-edge bilinear sampling at pixel centres.
[[nodiscard]] FloatImage expectedFrame(const FloatImage &source,
                                       const ReferenceScene &scene);

// Largest difference in 8-bit levels between any channel of actual
// (Format_RGBA8888_Premultiplied) and expected; describes the first worst
// pixel in where.
[[nodiscard]] int maxLevelDifference(const QImage &actual,
                                     const FloatImage &expected,
                                     QString *where);
