#include <QColorSpace>
#include <QColorTransform>
#include <QFloat16>
#include <QPointF>
#include <QRgba64>
#include <QSignalSpy>
#include <QTest>
#include <QThreadPool>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>

#include "gui/quick/render/colorlutbuilder.h"
#include "gui/quick/render/colortransformplan.h"
#include "testsuites.h"

namespace {
using namespace Qt::StringLiterals;

// Qt converts through interpolated transfer tables; the parametric conversion
// is exact in double precision. The tables are least accurate where an
// inverse pure-gamma curve (Adobe RGB, the Rec2020 preset) is steepest, just
// above black: up to about 2.3 8-bit levels there.
constexpr double kParametricTolerance = 3.0 / 255.0;
// ICC colorants are s15Fixed16 numbers: 2^-16 per matrix entry.
constexpr double kFixedPointTolerance = 1e-4;
// Half-float storage of the lookup table entries.
constexpr double kLutEntryTolerance = 1.0 / 1024.0;
constexpr double kRoundTripTolerance = 1e-6;
// Encoded test values per axis (0, 1/(n-1), ..., 1).
constexpr int kGridSteps = 9;
constexpr int kSmallLutSize = 9;
constexpr int kInvalidLutSize = 1;
constexpr double kQRgba64Max = 65535.0;
constexpr int kTransferTableSize = 1024;
constexpr double kTableGamma = 2.0;
constexpr int kBuildTimeoutMs = 10000;

// ColorManager's Rec2020 preset.
QColorSpace rec2020Gamma22() {
  return QColorSpace(QPointF(0.3127, 0.3290), QPointF(0.708, 0.292),
                     QPointF(0.170, 0.797), QPointF(0.131, 0.046),
                     QColorSpace::TransferFunction::Gamma, 2.2f);
}

// sRGB primaries with a table transfer function (a pure 2.0 gamma), which
// has no parametric form in the public API.
QColorSpace tableCurveSpace() {
  QList<uint16_t> table;
  for (int i = 0; i < kTransferTableSize; ++i) {
    const double x = double(i) / (kTransferTableSize - 1);
    table.append(uint16_t(std::lround(std::pow(x, kTableGamma) * kQRgba64Max)));
  }
  return QColorSpace(QColorSpace::Primaries::SRgb, table);
}

std::array<double, 3> applyParametric(const ParametricColorTransform &t,
                                      const std::array<double, 3> &encoded) {
  std::array<double, 3> linear{};
  for (int i = 0; i < 3; ++i)
    linear[i] = t.sourceCurve.toLinear(encoded[i]);
  std::array<double, 3> result{};
  for (int row = 0; row < 3; ++row) {
    const double value = t.matrix[row * 3] * linear[0] +
                         t.matrix[row * 3 + 1] * linear[1] +
                         t.matrix[row * 3 + 2] * linear[2];
    result[row] = t.targetCurve.fromLinear(std::clamp(value, 0.0, 1.0));
  }
  return result;
}

double gridValue(int step) { return double(step) / (kGridSteps - 1); }

quint16 toChannel(double value) {
  return quint16(std::lround(value * kQRgba64Max));
}

// Largest channel difference between the parametric plan and Qt's
// transform over the grid.
double maxParametricError(const QColorSpace &source, const QColorSpace &target) {
  const ColorTransformPlan plan = planColorTransform(source, target);
  if (plan.kind != ColorTransformKind::Parametric)
    return std::numeric_limits<double>::infinity();
  const QColorTransform transform = source.transformationToColorSpace(target);
  double worst = 0.0;
  for (int r = 0; r < kGridSteps; ++r) {
    for (int g = 0; g < kGridSteps; ++g) {
      for (int b = 0; b < kGridSteps; ++b) {
        const std::array<double, 3> encoded{gridValue(r), gridValue(g),
                                            gridValue(b)};
        const std::array<double, 3> ours = applyParametric(plan.parametric, encoded);
        const QRgba64 qt = transform.map(QRgba64::fromRgba64(
            toChannel(encoded[0]), toChannel(encoded[1]), toChannel(encoded[2]),
            quint16(kQRgba64Max)));
        worst = std::max({worst, std::abs(ours[0] - qt.red() / kQRgba64Max),
                          std::abs(ours[1] - qt.green() / kQRgba64Max),
                          std::abs(ours[2] - qt.blue() / kQRgba64Max)});
      }
    }
  }
  return worst;
}

double halfValue(quint16 bits) {
  qfloat16 half;
  std::memcpy(&half, &bits, sizeof(bits));
  return double(float(half));
}
} // namespace

Q_DECLARE_METATYPE(QColorSpace)
Q_DECLARE_METATYPE(TransferCurve)

class ColorTransformTests : public QObject {
  Q_OBJECT

private slots:
  void transferCurvesRoundTrip_data() {
    QTest::addColumn<TransferCurve>("curve");
    QTest::newRow("linear") << TransferCurve::linear();
    QTest::newRow("gamma 2.2") << TransferCurve::gamma(2.2f);
    QTest::newRow("sRGB") << TransferCurve::sRgb();
    QTest::newRow("ProPhoto") << TransferCurve::proPhotoRgb();
    QTest::newRow("BT.2020") << TransferCurve::bt2020();
  }

  void transferCurvesRoundTrip() {
    QFETCH(TransferCurve, curve);
    for (int step = 0; step < kGridSteps; ++step) {
      const double x = gridValue(step);
      QVERIFY2(std::abs(curve.fromLinear(curve.toLinear(x)) - x) <
                   kRoundTripTolerance,
               qPrintable(QString::number(x)));
    }
    QVERIFY(std::abs(curve.toLinear(1.0) - 1.0) < kRoundTripTolerance);
  }

  void sameSpaceIsIdentity() {
    const QColorSpace srgb(QColorSpace::SRgb);
    QCOMPARE(planColorTransform(srgb, srgb).kind, ColorTransformKind::Identity);
    // Untagged images are sRGB.
    QCOMPARE(planColorTransform(QColorSpace(), srgb).kind,
             ColorTransformKind::Identity);
  }

  void namedSpacesAreParametric_data() {
    QTest::addColumn<QColorSpace>("source");
    QTest::addColumn<QColorSpace>("target");
    const QColorSpace srgb(QColorSpace::SRgb);
    QTest::newRow("sRGB -> Display P3")
        << srgb << QColorSpace(QColorSpace::DisplayP3);
    QTest::newRow("sRGB -> Adobe RGB")
        << srgb << QColorSpace(QColorSpace::AdobeRgb);
    QTest::newRow("sRGB -> ProPhoto")
        << srgb << QColorSpace(QColorSpace::ProPhotoRgb);
    QTest::newRow("sRGB -> Rec2020 preset") << srgb << rec2020Gamma22();
    QTest::newRow("sRGB -> linear sRGB")
        << srgb << QColorSpace(QColorSpace::SRgbLinear);
    QTest::newRow("sRGB -> BT.2020")
        << srgb << QColorSpace(QColorSpace::Bt2020);
    QTest::newRow("Adobe RGB -> sRGB")
        << QColorSpace(QColorSpace::AdobeRgb) << srgb;
    QTest::newRow("ProPhoto -> Display P3")
        << QColorSpace(QColorSpace::ProPhotoRgb)
        << QColorSpace(QColorSpace::DisplayP3);
    QTest::newRow("ICC round trip -> sRGB")
        << QColorSpace::fromIccProfile(
               QColorSpace(QColorSpace::DisplayP3).iccProfile())
        << srgb;
  }

  void namedSpacesAreParametric() {
    QFETCH(QColorSpace, source);
    QFETCH(QColorSpace, target);
    QCOMPARE(planColorTransform(source, target).kind,
             ColorTransformKind::Parametric);
    const double error = maxParametricError(source, target);
    QVERIFY2(error <= kParametricTolerance,
             qPrintable(u"max error %1"_s.arg(error)));
  }

  void whiteMapsToWhite() {
    const ColorTransformPlan plan = planColorTransform(
        QColorSpace(QColorSpace::SRgb), QColorSpace(QColorSpace::DisplayP3));
    for (int row = 0; row < 3; ++row) {
      const double sum = plan.parametric.matrix[row * 3] +
                         plan.parametric.matrix[row * 3 + 1] +
                         plan.parametric.matrix[row * 3 + 2];
      QVERIFY(std::abs(sum - 1.0) < kFixedPointTolerance);
    }
    // RGB white has the luminance Y = 1 of the D50 connection white.
    const std::optional<ColorMatrix3> toXyz =
        rgbToXyzD50(QColorSpace(QColorSpace::SRgb));
    QVERIFY(toXyz);
    QVERIFY(std::abs((*toXyz)[3] + (*toXyz)[4] + (*toXyz)[5] - 1.0) <
            kFixedPointTolerance);
  }

  void tableCurvesNeedLut() {
    QCOMPARE(planColorTransform(QColorSpace(QColorSpace::SRgb),
                                tableCurveSpace())
                 .kind,
             ColorTransformKind::Lut);
  }

  void invalidOrGreyTargetsAreUnsupported() {
    const QColorSpace srgb(QColorSpace::SRgb);
    const ColorTransformPlan invalid = planColorTransform(srgb, QColorSpace());
    QCOMPARE(invalid.kind, ColorTransformKind::Unsupported);
    QVERIFY(!invalid.reason.isEmpty());
    const QColorSpace grey(QColorSpace::PrimaryPoints::fromPrimaries(
                               QColorSpace::Primaries::SRgb)
                               .whitePoint,
                           QColorSpace::TransferFunction::SRgb);
    QCOMPARE(planColorTransform(srgb, grey).kind,
             ColorTransformKind::Unsupported);
  }

  void lutMatchesTransformAtLatticePoints() {
    const QColorSpace source(QColorSpace::SRgb);
    const QColorSpace target = tableCurveSpace();
    const std::expected<ColorLut, QString> lut =
        buildColorLut(source, target, kSmallLutSize);
    QVERIFY2(lut, qPrintable(lut ? QString() : lut.error()));
    QCOMPARE(lut->size, kSmallLutSize);
    QCOMPARE(lut->halfRgba.size(),
             std::size_t(kSmallLutSize) * kSmallLutSize * kSmallLutSize *
                 ColorLut::kChannels);
    const QColorTransform transform = source.transformationToColorSpace(target);
    const double step = 1.0 / (kSmallLutSize - 1);
    for (int b = 0; b < kSmallLutSize; ++b) {
      for (int g = 0; g < kSmallLutSize; ++g) {
        for (int r = 0; r < kSmallLutSize; ++r) {
          const QRgba64 expected = transform.map(QRgba64::fromRgba64(
              toChannel(r * step), toChannel(g * step), toChannel(b * step),
              quint16(kQRgba64Max)));
          const std::size_t index =
              ((std::size_t(b) * kSmallLutSize + g) * kSmallLutSize + r) *
              ColorLut::kChannels;
          QVERIFY(std::abs(halfValue(lut->halfRgba[index]) -
                           expected.red() / kQRgba64Max) <= kLutEntryTolerance);
          QVERIFY(std::abs(halfValue(lut->halfRgba[index + 1]) -
                           expected.green() / kQRgba64Max) <= kLutEntryTolerance);
          QVERIFY(std::abs(halfValue(lut->halfRgba[index + 2]) -
                           expected.blue() / kQRgba64Max) <= kLutEntryTolerance);
        }
      }
    }
  }

  void lutRejectsInvalidInput() {
    const QColorSpace srgb(QColorSpace::SRgb);
    QVERIFY(!buildColorLut(srgb, tableCurveSpace(), kInvalidLutSize));
    QVERIFY(!buildColorLut(srgb, QColorSpace()));
  }

  void builderCachesTables() {
    QThreadPool pool;
    ColorLutBuilder builder(nullptr, &pool);
    QSignalSpy ready(&builder, &ColorLutBuilder::lutReady);
    QSignalSpy failed(&builder, &ColorLutBuilder::lutFailed);
    const QColorSpace source(QColorSpace::SRgb);
    const QColorSpace target = tableCurveSpace();
    QVERIFY(!builder.find(source, target));
    builder.request(source, target);
    // A second request for the same pair joins the running build.
    builder.request(source, target);
    QVERIFY(builder.isBuilding());
    QVERIFY(ready.wait(kBuildTimeoutMs));
    QCOMPARE(ready.count(), 1);
    QCOMPARE(failed.count(), 0);
    QVERIFY(!builder.isBuilding());
    const std::shared_ptr<const ColorLut> lut = builder.find(source, target);
    QVERIFY(lut);
    QCOMPARE(lut->size, ColorLut::kDefaultSize);
    // Cached: no new build.
    builder.request(source, target);
    QVERIFY(!builder.isBuilding());
  }

  void builderReportsFailureOnce() {
    QThreadPool pool;
    ColorLutBuilder builder(nullptr, &pool);
    QSignalSpy ready(&builder, &ColorLutBuilder::lutReady);
    QSignalSpy failed(&builder, &ColorLutBuilder::lutFailed);
    const QColorSpace source(QColorSpace::SRgb);
    builder.request(source, QColorSpace());
    QVERIFY(failed.wait(kBuildTimeoutMs));
    builder.request(source, QColorSpace());
    QVERIFY(!builder.isBuilding());
    QCoreApplication::processEvents();
    QCOMPARE(failed.count(), 1);
    QCOMPARE(ready.count(), 0);
  }

  void destroyedBuilderDropsCompletion() {
    QThreadPool pool;
    auto builder = std::make_unique<ColorLutBuilder>(nullptr, &pool);
    builder->request(QColorSpace(QColorSpace::SRgb), tableCurveSpace());
    builder.reset();
    // The worker finishes and posts its completion to a destroyed builder.
    QVERIFY(pool.waitForDone(kBuildTimeoutMs));
    QCoreApplication::processEvents();
  }
};

int runColorTransformTests(int argc, char **argv) {
  ColorTransformTests tests;
  return QTest::qExec(&tests, argc, argv);
}

#include "tst_colortransform.moc"
