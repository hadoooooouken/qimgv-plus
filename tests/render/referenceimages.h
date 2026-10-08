#pragma once

#include <QColor>
#include <QImage>
#include <QPoint>
#include <QSize>
#include <QString>
#include <array>
#include <optional>
#include <vector>

#include "utils/coloradjustments.h"

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

// The image drawn over an opaque background with its top-left corner at
// origin; drawn holds device pixels (premultiplied).
[[nodiscard]] FloatImage composeOver(const FloatImage &drawn, QSize frameSize,
                                     QPoint origin, const QColor &background);

// ImageRenderer's exact-ratio downsample: the same chain of exact-area box
// passes (each at most halving a side, rounding up), in double precision.
[[nodiscard]] FloatImage exactReduce(const FloatImage &image, QSize target);

enum class ReferenceSharpening { None, Cas, Smart };

// Per-pixel filter of the tile shader (image.frag) with its plain taps.
struct ReferenceFilterParams {
  ReferenceSharpening sharpening = ReferenceSharpening::None;
  double casSharpening = 0.0;
  double casContrast = 0.0;
  std::optional<ColorMatrix> color;
};

// The image magnified by an integer factor with clamp-to-edge bilinear
// sampling (factor 1: the texels themselves), sharpened with taps one device
// pixel apart and colour adjusted, as image.frag does above 1:1 and on the
// exact downsample.
[[nodiscard]] FloatImage filteredMagnified(const FloatImage &image,
                                           int magnification,
                                           const ReferenceFilterParams &params);

// Largest difference in 8-bit levels between any channel of actual
// (Format_RGBA8888_Premultiplied) and expected; describes the first worst
// pixel in where.
[[nodiscard]] int maxLevelDifference(const QImage &actual,
                                     const FloatImage &expected,
                                     QString *where);
