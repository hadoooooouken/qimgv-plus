#pragma once

#include <QRect>
#include <QSize>
#include <vector>

#include "gui/quick/render/tilegrid.h"

// Geometry and weights of ImageRenderer's Magic Kernel Sharp 2021
// resampling.
//
// The image is resampled onto an output grid of whole device pixels, the
// size the CPU scaler (ImageLib::scaled_MKS2021()) would produce for the same
// scale; output pixel (0, 0) is drawn at the image's snapped top-left corner.
// Only the visible outputs are computed, tile by tile: every output belongs to
// the tile whose core contains the source position of its centre.

// One axis of the resampling of sourceSize source pixels to outputSize output
// pixels; the same kernel and taps as buildMksAxisTaps() in
// utils/imagelib.cpp.
struct Mks2021Axis {
  // Half width of the kernel in source pixels at scales >= 1. Below 1:1 the
  // kernel is widened by the reduction factor (filterScale()).
  static constexpr double kSupport = 4.5;

  // k(x): the Magic Kernel Sharp 2021 kernel (a = 3, v = 3).
  [[nodiscard]] static double kernel(double x);

  int sourceSize = 1;
  int outputSize = 1;

  // Source pixels per output pixel.
  [[nodiscard]] double ratio() const;
  [[nodiscard]] double filterScale() const;
  [[nodiscard]] double support() const;
  // Position of the centre of output in source pixel indices (pixel i spans
  // [i - 0.5, i + 0.5)).
  [[nodiscard]] double centre(int output) const;
  // First and last source index weighted for output, before clamping to the
  // image.
  [[nodiscard]] int firstTap(int output) const;
  [[nodiscard]] int lastTap(int output) const;
  // Normalized weights of the source indices firstTap(output) ..
  // lastTap(output).
  [[nodiscard]] std::vector<double> weights(int output) const;
  // First output whose centre lies at or after the left edge of source pixel
  // sourceBegin, clamped to [0, outputSize]. The outputs
  // [firstOwnedOutput(a), firstOwnedOutput(b)) are those centred in source
  // pixels [a, b), so adjacent tile cores own adjacent outputs.
  [[nodiscard]] int firstOwnedOutput(int sourceBegin) const;
};

namespace ResampleGrid {
// Output size of an image of imageSize source pixels shown at scale device
// pixels per source pixel; at least one pixel per side.
[[nodiscard]] QSize outputSize(QSize imageSize, double scale);

// Outputs of the tile that lie inside the colour buffer of targetSize device
// pixels, in output coordinates; empty when none do. origin is the device
// position of output (0, 0).
[[nodiscard]] QRect visibleTileOutputs(const ImageTile &tile, QSize imageSize,
                                       QSize outputSize, QPoint origin,
                                       QSize targetSize);
} // namespace ResampleGrid
