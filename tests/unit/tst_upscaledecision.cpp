#include <QTest>

#include "components/upscaler/upscaledecision.h"
#include "testsuites.h"

namespace {
constexpr int kImageWidth = 1000;
constexpr int kZoomedInWidth = 1500;
constexpr int kZoomedOutWidth = 500;
constexpr int kLimitPercent = 200;
constexpr float kZoomBelowLimit = 150.0f;
constexpr float kZoomAboveLimit = 250.0f;

// Upscayl on, a static image shown zoomed in, no limit.
UpscaleInputs zoomedIn() {
  return {.panoramaMode = false,
          .useUpscayl = true,
          .staticImage = true,
          .limitEnabled = false,
          .limitPercent = kLimitPercent,
          .zoomPercent = kZoomAboveLimit,
          .displayedWidth = kZoomedInWidth,
          .imageWidth = kImageWidth};
}
} // namespace

Q_DECLARE_METATYPE(UpscaleAction)
Q_DECLARE_METATYPE(UpscaleInputs)

// The rules Core applied after every CPU scale (Core::onScalingFinished())
// before they were extracted; the Quick UI reaches them without a scale.
class UpscaleDecisionTests : public QObject {
  Q_OBJECT

private slots:
  void decide_data() {
    QTest::addColumn<UpscaleInputs>("inputs");
    QTest::addColumn<UpscaleAction>("expected");

    QTest::newRow("zoomed in") << zoomedIn() << UpscaleAction::Request;

    UpscaleInputs panorama = zoomedIn();
    panorama.panoramaMode = true;
    QTest::newRow("panorama") << panorama << UpscaleAction::HideCropAndReset;

    UpscaleInputs off = zoomedIn();
    off.useUpscayl = false;
    QTest::newRow("upscayl off") << off << UpscaleAction::HideCrop;

    UpscaleInputs offPanorama = off;
    offPanorama.panoramaMode = true;
    QTest::newRow("panorama wins over off") << offPanorama << UpscaleAction::HideCropAndReset;

    UpscaleInputs animation = zoomedIn();
    animation.staticImage = false;
    QTest::newRow("animation") << animation << UpscaleAction::None;

    UpscaleInputs zoomedOut = zoomedIn();
    zoomedOut.displayedWidth = kZoomedOutWidth;
    QTest::newRow("zoomed out") << zoomedOut << UpscaleAction::None;

    UpscaleInputs oneToOne = zoomedIn();
    oneToOne.displayedWidth = kImageWidth;
    QTest::newRow("1:1") << oneToOne << UpscaleAction::None;

    UpscaleInputs belowLimit = zoomedIn();
    belowLimit.limitEnabled = true;
    belowLimit.zoomPercent = kZoomBelowLimit;
    QTest::newRow("below limit") << belowLimit << UpscaleAction::InvalidatePreview;

    UpscaleInputs atLimit = belowLimit;
    atLimit.zoomPercent = kLimitPercent;
    QTest::newRow("at limit") << atLimit << UpscaleAction::InvalidatePreview;

    UpscaleInputs aboveLimit = belowLimit;
    aboveLimit.zoomPercent = kZoomAboveLimit;
    QTest::newRow("above limit") << aboveLimit << UpscaleAction::Request;

    UpscaleInputs aboveLimitZoomedOut = aboveLimit;
    aboveLimitZoomedOut.displayedWidth = kZoomedOutWidth;
    QTest::newRow("above limit, zoomed out") << aboveLimitZoomedOut << UpscaleAction::None;
  }

  void decide() {
    QFETCH(UpscaleInputs, inputs);
    QFETCH(UpscaleAction, expected);
    QCOMPARE(decideUpscale(inputs), expected);
  }
};

int runUpscaleDecisionTests(int argc, char **argv) {
  UpscaleDecisionTests tests;
  return QTest::qExec(&tests, argc, argv);
}

#include "tst_upscaledecision.moc"
