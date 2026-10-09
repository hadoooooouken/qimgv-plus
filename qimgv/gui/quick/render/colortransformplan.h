#pragma once

#include <QColorSpace>
#include <QString>
#include <QtGlobal>
#include <array>
#include <expected>
#include <optional>
#include <vector>

// Colour management data for ImageRenderer's conversion pass
// (res/shaders/rhi/convert.frag): how pixels in a source colour space are
// brought into the target colour space of the display.
//
// Colour spaces made of three primaries and one of Qt's named transfer
// functions (every preset of ColorManager and most monitor ICC profiles) are
// converted analytically: transfer curve, 3x3 matrix, inverse transfer curve.
// Everything else (table-based curves, ICC element-list profiles) goes
// through a 3D lookup table built with QColorTransform (buildColorLut()), on
// a worker thread (ColorLutBuilder).
//
// Pure functions of their arguments; safe on any thread.

// A transfer curve in the parametric form of QColorTransferFunction (ICC
// parametric curve type 4): encoded value x maps to linear light
//   x < d ? c * x + f : pow(a * x + b, g) + e.
struct TransferCurve {
  float a = 1.0f;
  float b = 0.0f;
  float c = 0.0f;
  float d = 0.0f;
  float e = 0.0f;
  float f = 0.0f;
  float g = 1.0f;

  // The named curves of QColorSpace::TransferFunction, with Qt's parameters
  // (qcolortransferfunction_p.h).
  [[nodiscard]] static TransferCurve linear();
  [[nodiscard]] static TransferCurve gamma(float exponent);
  [[nodiscard]] static TransferCurve sRgb();
  [[nodiscard]] static TransferCurve proPhotoRgb();
  [[nodiscard]] static TransferCurve bt2020();

  // Encoded -> linear and its inverse, in double precision (CPU reference of
  // the shader).
  [[nodiscard]] double toLinear(double encoded) const;
  [[nodiscard]] double fromLinear(double linear) const;

  friend bool operator==(const TransferCurve &, const TransferCurve &) = default;
};

// Row-major 3x3 matrix acting on column vectors.
using ColorMatrix3 = std::array<double, 9>;

// RGB -> CIE XYZ relative to the D50 white of the ICC connection space, the
// matrix QColorTransform uses: the colorant tags (rXYZ, gXYZ, bXYZ) of
// colorSpace.iccProfile(). Empty when the profile has none (element-list
// profiles) or they are degenerate.
[[nodiscard]] std::optional<ColorMatrix3> rgbToXyzD50(const QColorSpace &colorSpace);

// The parametric curve of colorSpace's transfer function; empty for custom
// (table) curves and for the HDR curves PQ and HLG.
[[nodiscard]] std::optional<TransferCurve>
transferCurveOf(const QColorSpace &colorSpace);

// How pixels in a source colour space reach the target colour space.
enum class ColorTransformKind {
  // Source and target are the same; nothing to do.
  Identity,
  // sourceCurve, matrix, inverse targetCurve.
  Parametric,
  // A 3D lookup table from buildColorLut().
  Lut,
  // QColorTransform cannot convert between the two (an invalid or non-RGB
  // space); the pixels are shown unconverted.
  Unsupported,
};

struct ParametricColorTransform {
  TransferCurve sourceCurve;
  // Linear source RGB -> linear target RGB.
  ColorMatrix3 matrix{1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0};
  TransferCurve targetCurve;

  friend bool operator==(const ParametricColorTransform &,
                         const ParametricColorTransform &) = default;
};

struct ColorTransformPlan {
  ColorTransformKind kind = ColorTransformKind::Identity;
  // Kind Parametric only.
  ParametricColorTransform parametric;
  // Kind Unsupported only.
  QString reason;

  friend bool operator==(const ColorTransformPlan &,
                         const ColorTransformPlan &) = default;
};

// Plans the conversion from source to target. An invalid source is treated
// as sRGB (like ColorManager::applyColorManagement()); a target that is not a
// valid target colour space makes the plan Unsupported.
[[nodiscard]] ColorTransformPlan planColorTransform(const QColorSpace &source,
                                                    const QColorSpace &target);

// 3D lookup table from encoded source RGB to encoded target RGB, clamped to
// [0, 1]: size^3 RGBA entries as IEEE half-float bits, red varying fastest,
// then green, then blue (one size x size slice per blue step); alpha is 1.
// Sampled trilinearly at texel centres: coordinate = value * (size - 1) /
// size + 0.5 / size.
struct ColorLut {
  // Lattice points per axis. 33 matches the usual ICC CMM grid: on smooth
  // transforms the trilinear error stays below one 8-bit level.
  static constexpr int kDefaultSize = 33;
  static constexpr int kChannels = 4;

  int size = 0;
  std::vector<quint16> halfRgba;
};

// Builds the table by sending the lattice through
// source.transformationToColorSpace(target). Slow for ICC element-list
// profiles (tens of milliseconds); never call it on the GUI or render
// thread. Fails for invalid spaces and sizes below 2.
[[nodiscard]] std::expected<ColorLut, QString>
buildColorLut(const QColorSpace &source, const QColorSpace &target,
              int size = ColorLut::kDefaultSize);
