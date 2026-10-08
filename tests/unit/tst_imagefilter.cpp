#include <QTest>
#include <cmath>

#include "gui/quick/render/imagefiltermode.h"
#include "testsuites.h"
#include "utils/coloradjustments.h"

namespace {
// Matrix entries are products of a few floats.
constexpr float kMatrixTolerance = 1e-4f;
constexpr float kDoubleExposure = 1.0f;
constexpr float kExposureFactor = 2.0f;
constexpr float kHighContrast = 2.0f;
constexpr float kBrightnessShift = 0.25f;
constexpr float kContrastPivot = 0.5f;
constexpr float kNoSaturation = 0.0f;
// A third of a turn around the grey axis maps blue -> red -> green -> blue.
constexpr float kThirdTurnDegrees = 120.0f;
constexpr float kLuma[3] = {0.2126f, 0.7152f, 0.0722f};

bool near(float actual, float expected) {
  return std::abs(actual - expected) <= kMatrixTolerance;
}

bool matrixEquals(const ColorMatrix &matrix, const float (&expected)[3][3],
                  float expectedOffset) {
  for (int row = 0; row < 3; ++row) {
    for (int column = 0; column < 3; ++column) {
      if (!near(matrix.m[row][column], expected[row][column]))
        return false;
    }
  }
  return near(matrix.offset, expectedOffset);
}
} // namespace

class ImageFilterTests : public QObject {
  Q_OBJECT

private slots:
  void neutralAdjustmentsAreIdentity() {
    const ColorAdjustments neutral;
    QVERIFY(!neutral.hasAdjustments());
    const float identity[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    QVERIFY(matrixEquals(colorAdjustmentMatrix(neutral), identity, 0.0f));
  }

  void exposureScalesAllChannels() {
    ColorAdjustments adjustments;
    adjustments.exposure = kDoubleExposure;
    const float f = kExposureFactor;
    const float expected[3][3] = {{f, 0, 0}, {0, f, 0}, {0, 0, f}};
    QVERIFY(matrixEquals(colorAdjustmentMatrix(adjustments), expected, 0.0f));
  }

  void contrastPivotsAroundMidGrey() {
    ColorAdjustments adjustments;
    adjustments.contrast = kHighContrast;
    adjustments.brightness = kBrightnessShift;
    const float c = kHighContrast;
    const float expected[3][3] = {{c, 0, 0}, {0, c, 0}, {0, 0, c}};
    const float offset = kBrightnessShift * c + kContrastPivot * (1.0f - c);
    QVERIFY(matrixEquals(colorAdjustmentMatrix(adjustments), expected, offset));
  }

  void zeroSaturationIsLuma() {
    ColorAdjustments adjustments;
    adjustments.saturation = kNoSaturation;
    const float expected[3][3] = {{kLuma[0], kLuma[1], kLuma[2]},
                                  {kLuma[0], kLuma[1], kLuma[2]},
                                  {kLuma[0], kLuma[1], kLuma[2]}};
    QVERIFY(matrixEquals(colorAdjustmentMatrix(adjustments), expected, 0.0f));
  }

  void thirdTurnHueRotatesChannels() {
    ColorAdjustments adjustments;
    adjustments.hue = kThirdTurnDegrees;
    // out.r = in.b, out.g = in.r, out.b = in.g
    const float expected[3][3] = {{0, 0, 1}, {1, 0, 0}, {0, 1, 0}};
    QVERIFY(matrixEquals(colorAdjustmentMatrix(adjustments), expected, 0.0f));
  }

  void scalingFiltersMapToRenderModes_data() {
    QTest::addColumn<int>("filter");
    QTest::addColumn<RenderEnums::TextureSampling>("sampling");
    QTest::addColumn<RenderEnums::Sharpening>("sharpening");
    using RenderEnums::Sharpening;
    using RenderEnums::TextureSampling;
    QTest::newRow("nearest") << int(QI_FILTER_NEAREST)
                             << TextureSampling::Nearest << Sharpening::None;
    QTest::newRow("bilinear") << int(QI_FILTER_BILINEAR)
                              << TextureSampling::Trilinear << Sharpening::None;
    QTest::newRow("smart") << int(QI_FILTER_SMART)
                           << TextureSampling::Trilinear << Sharpening::None;
    QTest::newRow("cas") << int(QI_FILTER_CAS) << TextureSampling::Trilinear
                         << Sharpening::Cas;
    QTest::newRow("smart gpu") << int(QI_FILTER_SMART_GPU)
                               << TextureSampling::Trilinear
                               << Sharpening::Smart;
    QTest::newRow("mks2021") << int(QI_FILTER_MKS2021)
                             << TextureSampling::Trilinear << Sharpening::None;
  }

  void scalingFiltersMapToRenderModes() {
    QFETCH(int, filter);
    QFETCH(RenderEnums::TextureSampling, sampling);
    QFETCH(RenderEnums::Sharpening, sharpening);
    const ImageFilterMode mode =
        imageFilterModeFor(static_cast<ScalingFilter>(filter));
    QCOMPARE(mode.sampling, sampling);
    QCOMPARE(mode.sharpening, sharpening);
  }
};

int runImageFilterTests(int argc, char **argv) {
  ImageFilterTests tests;
  return QTest::qExec(&tests, argc, argv);
}

#include "tst_imagefilter.moc"
