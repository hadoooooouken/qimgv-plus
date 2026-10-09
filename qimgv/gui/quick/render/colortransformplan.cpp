#include "colortransformplan.h"

#include <QByteArray>
#include <QColorTransform>
#include <QFloat16>
#include <QImage>
#include <QtEndian>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace {
using namespace Qt::StringLiterals;

// ICC profile layout (ICC.1:2010 sections 7.2 - 7.3, 10.31): the tag count
// follows the 128-byte header, then one 12-byte entry (signature, offset,
// size) per tag. An XYZType tag holds its type signature, 4 reserved bytes
// and three s15Fixed16Number values.
constexpr int kIccHeaderSize = 128;
constexpr int kIccTagEntrySize = 12;
constexpr int kIccTagEntryOffsetField = 4;
constexpr int kIccTagEntrySizeField = 8;
constexpr int kIccXyzValuesOffset = 8;
constexpr int kIccXyzTagSize = kIccXyzValuesOffset + 3 * 4;
constexpr quint32 kIccXyzType = 0x58595A20;     // 'XYZ '
constexpr quint32 kIccRedColorant = 0x7258595A;   // 'rXYZ'
constexpr quint32 kIccGreenColorant = 0x6758595A; // 'gXYZ'
constexpr quint32 kIccBlueColorant = 0x6258595A;  // 'bXYZ'
constexpr double kS15Fixed16Scale = 65536.0;

// Parameters of Qt's named transfer functions
// (QColorTransferFunction::fromSRgb() / fromProPhotoRgb() / fromBt2020()).
constexpr float kSRgbScale = 1.0f / 1.055f;
constexpr float kSRgbOffset = 0.055f / 1.055f;
constexpr float kSRgbLinearSlope = 1.0f / 12.92f;
constexpr float kSRgbBreak = 0.04045f;
constexpr float kSRgbGamma = 2.4f;
constexpr float kProPhotoLinearSlope = 1.0f / 16.0f;
constexpr float kProPhotoBreak = 16.0f / 512.0f;
constexpr float kProPhotoGamma = 1.8f;
constexpr float kBt2020Scale = 1.0f / 1.0993f;
constexpr float kBt2020Offset = 0.0993f / 1.0993f;
constexpr float kBt2020LinearSlope = 1.0f / 4.5f;
constexpr float kBt2020Break = 0.08145f;
constexpr float kBt2020Gamma = 2.2f;
constexpr float kLinearGamma = 1.0f;

// Determinants below this make a primaries matrix degenerate.
constexpr double kMinDeterminant = 1e-12;
constexpr int kMinimumLutSize = 2;

struct Xyz {
  double x = 0.0;
  double y = 0.0;
  double z = 0.0;
};

quint32 readUInt32(const QByteArray &data, qsizetype offset) {
  return qFromBigEndian<quint32>(data.constData() + offset);
}

// The XYZType tag tagSignature of profile.
std::optional<Xyz> iccXyzTag(const QByteArray &profile, quint32 tagSignature) {
  if (profile.size() < kIccHeaderSize + 4)
    return std::nullopt;
  const quint32 tagCount = readUInt32(profile, kIccHeaderSize);
  for (quint32 index = 0; index < tagCount; ++index) {
    const qsizetype entry =
        kIccHeaderSize + 4 + qsizetype(index) * kIccTagEntrySize;
    if (entry + kIccTagEntrySize > profile.size())
      return std::nullopt;
    if (readUInt32(profile, entry) != tagSignature)
      continue;
    const qsizetype offset =
        readUInt32(profile, entry + kIccTagEntryOffsetField);
    const qsizetype size = readUInt32(profile, entry + kIccTagEntrySizeField);
    if (size < kIccXyzTagSize || offset + kIccXyzTagSize > profile.size() ||
        readUInt32(profile, offset) != kIccXyzType)
      return std::nullopt;
    const auto value = [&](int component) {
      return static_cast<qint32>(readUInt32(
                 profile, offset + kIccXyzValuesOffset + component * 4)) /
             kS15Fixed16Scale;
    };
    return Xyz{value(0), value(1), value(2)};
  }
  return std::nullopt;
}

ColorMatrix3 multiply(const ColorMatrix3 &l, const ColorMatrix3 &r) {
  ColorMatrix3 result{};
  for (int row = 0; row < 3; ++row) {
    for (int column = 0; column < 3; ++column) {
      double sum = 0.0;
      for (int k = 0; k < 3; ++k)
        sum += l[row * 3 + k] * r[k * 3 + column];
      result[row * 3 + column] = sum;
    }
  }
  return result;
}

std::optional<ColorMatrix3> inverted(const ColorMatrix3 &m) {
  const double det = m[0] * (m[4] * m[8] - m[5] * m[7]) -
                     m[1] * (m[3] * m[8] - m[5] * m[6]) +
                     m[2] * (m[3] * m[7] - m[4] * m[6]);
  if (std::abs(det) < kMinDeterminant)
    return std::nullopt;
  const double inv = 1.0 / det;
  return ColorMatrix3{(m[4] * m[8] - m[5] * m[7]) * inv,
                      (m[2] * m[7] - m[1] * m[8]) * inv,
                      (m[1] * m[5] - m[2] * m[4]) * inv,
                      (m[5] * m[6] - m[3] * m[8]) * inv,
                      (m[0] * m[8] - m[2] * m[6]) * inv,
                      (m[2] * m[3] - m[0] * m[5]) * inv,
                      (m[3] * m[7] - m[4] * m[6]) * inv,
                      (m[1] * m[6] - m[0] * m[7]) * inv,
                      (m[0] * m[4] - m[1] * m[3]) * inv};
}

} // namespace

//------------------------------------------------------------------------------
TransferCurve TransferCurve::linear() { return gamma(kLinearGamma); }

TransferCurve TransferCurve::gamma(float exponent) {
  TransferCurve curve;
  curve.g = exponent;
  return curve;
}

TransferCurve TransferCurve::sRgb() {
  return TransferCurve{kSRgbScale, kSRgbOffset, kSRgbLinearSlope, kSRgbBreak,
                       0.0f,       0.0f,        kSRgbGamma};
}

TransferCurve TransferCurve::proPhotoRgb() {
  return TransferCurve{1.0f, 0.0f, kProPhotoLinearSlope, kProPhotoBreak,
                       0.0f, 0.0f, kProPhotoGamma};
}

TransferCurve TransferCurve::bt2020() {
  return TransferCurve{kBt2020Scale, kBt2020Offset, kBt2020LinearSlope,
                       kBt2020Break, 0.0f,          0.0f,
                       kBt2020Gamma};
}

double TransferCurve::toLinear(double encoded) const {
  if (encoded < d)
    return c * encoded + f;
  return std::pow(std::max(a * encoded + b, 0.0), double(g)) + e;
}

double TransferCurve::fromLinear(double linearValue) const {
  if (c > 0.0f && linearValue < double(c) * d + f)
    return (linearValue - f) / c;
  return (std::pow(std::max(linearValue - e, 0.0), 1.0 / g) - b) / a;
}

//------------------------------------------------------------------------------
std::optional<ColorMatrix3> rgbToXyzD50(const QColorSpace &colorSpace) {
  // The colorant tags hold the D50-adapted primaries that Qt's own
  // transforms use; QColorSpace::primaryPoints() is not used because Qt
  // 6.12 returns its points in the wrong fields.
  const QByteArray profile = colorSpace.iccProfile();
  const std::optional<Xyz> r = iccXyzTag(profile, kIccRedColorant);
  const std::optional<Xyz> g = iccXyzTag(profile, kIccGreenColorant);
  const std::optional<Xyz> b = iccXyzTag(profile, kIccBlueColorant);
  if (!r || !g || !b)
    return std::nullopt;
  const ColorMatrix3 toXyz{r->x, g->x, b->x, r->y, g->y, b->y,
                           r->z, g->z, b->z};
  if (!inverted(toXyz))
    return std::nullopt;
  return toXyz;
}

std::optional<TransferCurve> transferCurveOf(const QColorSpace &colorSpace) {
  switch (colorSpace.transferFunction()) {
  case QColorSpace::TransferFunction::Linear:
    return TransferCurve::linear();
  case QColorSpace::TransferFunction::Gamma:
    return TransferCurve::gamma(colorSpace.gamma());
  case QColorSpace::TransferFunction::SRgb:
    return TransferCurve::sRgb();
  case QColorSpace::TransferFunction::ProPhotoRgb:
    return TransferCurve::proPhotoRgb();
  case QColorSpace::TransferFunction::Bt2020:
    return TransferCurve::bt2020();
  case QColorSpace::TransferFunction::Custom:
  case QColorSpace::TransferFunction::St2084:
  case QColorSpace::TransferFunction::Hlg:
    break;
  }
  return std::nullopt;
}

//------------------------------------------------------------------------------
ColorTransformPlan planColorTransform(const QColorSpace &source,
                                      const QColorSpace &target) {
  ColorTransformPlan plan;
  if (!target.isValid() || !target.isValidTarget()) {
    plan.kind = ColorTransformKind::Unsupported;
    plan.reason = u"the display colour space is not a valid conversion "
                  u"target"_s;
    return plan;
  }
  const QColorSpace effectiveSource =
      source.isValid() ? source : QColorSpace(QColorSpace::SRgb);
  if (effectiveSource == target)
    return plan;
  if (effectiveSource.colorModel() != QColorSpace::ColorModel::Rgb ||
      target.colorModel() != QColorSpace::ColorModel::Rgb) {
    plan.kind = ColorTransformKind::Unsupported;
    plan.reason = u"only RGB colour spaces are converted on the GPU"_s;
    return plan;
  }

  plan.kind = ColorTransformKind::Lut;
  using Model = QColorSpace::TransformModel;
  if (effectiveSource.transformModel() != Model::ThreeComponentMatrix ||
      target.transformModel() != Model::ThreeComponentMatrix)
    return plan;
  const std::optional<TransferCurve> sourceCurve =
      transferCurveOf(effectiveSource);
  const std::optional<TransferCurve> targetCurve = transferCurveOf(target);
  const std::optional<ColorMatrix3> sourceToXyz = rgbToXyzD50(effectiveSource);
  const std::optional<ColorMatrix3> targetToXyz = rgbToXyzD50(target);
  if (!sourceCurve || !targetCurve || !sourceToXyz || !targetToXyz)
    return plan;
  const std::optional<ColorMatrix3> xyzToTarget = inverted(*targetToXyz);
  if (!xyzToTarget)
    return plan;

  plan.kind = ColorTransformKind::Parametric;
  plan.parametric.sourceCurve = *sourceCurve;
  plan.parametric.matrix = multiply(*xyzToTarget, *sourceToXyz);
  plan.parametric.targetCurve = *targetCurve;
  return plan;
}

//------------------------------------------------------------------------------
std::expected<ColorLut, QString> buildColorLut(const QColorSpace &source,
                                               const QColorSpace &target,
                                               int size) {
  if (size < kMinimumLutSize)
    return std::unexpected(u"invalid colour lookup table size %1"_s.arg(size));
  const QColorSpace effectiveSource =
      source.isValid() ? source : QColorSpace(QColorSpace::SRgb);
  if (!target.isValidTarget())
    return std::unexpected(
        u"the display colour space is not a valid conversion target"_s);

  // One lattice point per pixel: x = red, y = green + blue * size.
  QImage lattice(size, size * size, QImage::Format_RGBA32FPx4);
  if (lattice.isNull())
    return std::unexpected(u"cannot allocate a %1^3 colour lattice"_s.arg(size));
  const float step = 1.0f / static_cast<float>(size - 1);
  for (int blue = 0; blue < size; ++blue) {
    for (int green = 0; green < size; ++green) {
      auto *line =
          reinterpret_cast<float *>(lattice.scanLine(blue * size + green));
      for (int red = 0; red < size; ++red) {
        float *pixel = line + red * ColorLut::kChannels;
        pixel[0] = red * step;
        pixel[1] = green * step;
        pixel[2] = blue * step;
        pixel[3] = 1.0f;
      }
    }
  }
  const QColorTransform transform =
      effectiveSource.transformationToColorSpace(target);
  if (!transform.isIdentity())
    lattice.applyColorTransform(transform);

  ColorLut lut;
  lut.size = size;
  lut.halfRgba.resize(static_cast<std::size_t>(size) * size * size *
                      ColorLut::kChannels);
  std::size_t index = 0;
  for (int row = 0; row < lattice.height(); ++row) {
    const auto *line = reinterpret_cast<const float *>(lattice.constScanLine(row));
    for (int value = 0; value < size * ColorLut::kChannels; ++value) {
      const float channel = std::isfinite(line[value])
                                ? std::clamp(line[value], 0.0f, 1.0f)
                                : 0.0f;
      const qfloat16 half(channel);
      std::memcpy(&lut.halfRgba[index++], &half, sizeof(quint16));
    }
  }
  return lut;
}
