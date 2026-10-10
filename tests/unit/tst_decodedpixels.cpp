#include <QTest>
#include <QThread>

#include <atomic>
#include <memory>
#include <vector>

#include "sourcecontainers/decodedpixels.h"
#include "testsuites.h"

namespace {
constexpr QSize kImageSize(64, 32);
// Threads asking for the SDR copy at the same time.
constexpr int kConcurrentReaders = 8;

std::shared_ptr<const QImage> hdrImage() {
  QImage image(kImageSize, QImage::Format_RGBA16FPx4);
  image.fill(Qt::white);
  return std::make_shared<const QImage>(std::move(image));
}

std::shared_ptr<const QImage> sdrImage() {
  QImage image(kImageSize, QImage::Format_ARGB32);
  image.fill(Qt::darkCyan);
  return std::make_shared<const QImage>(std::move(image));
}

// Counts its calls; converts like the CPU fallback (8-bit sRGB).
struct CountingConversion {
  std::shared_ptr<std::atomic<int>> calls = std::make_shared<std::atomic<int>>(0);

  DecodedPixels::SdrConversion function() const {
    return [calls = calls](const QImage &hdr) {
      ++*calls;
      return hdr.convertToFormat(QImage::Format_ARGB32);
    };
  }
};
} // namespace

class DecodedPixelsTests : public QObject {
  Q_OBJECT

private slots:
  void keepsTheHdrSourceAndConvertsOnFirstUse() {
    CountingConversion conversion;
    const std::shared_ptr<const QImage> hdr = hdrImage();
    DecodedPixels pixels;
    pixels.assign(hdr, true, conversion.function());
    QCOMPARE(conversion.calls->load(), 0);
    QVERIFY(pixels.hasHdrSource());
    QCOMPARE(pixels.decoded(), hdr);
    QCOMPARE(pixels.size(), kImageSize);
    QCOMPARE(conversion.calls->load(), 0);

    const std::shared_ptr<const QImage> sdr = pixels.sdr();
    QVERIFY(sdr);
    QCOMPARE(sdr->format(), QImage::Format_ARGB32);
    QCOMPARE(pixels.sdr(), sdr);
    QCOMPARE(conversion.calls->load(), 1);
    // The viewer keeps showing the HDR source.
    QCOMPARE(pixels.decoded(), hdr);
  }

  void anSdrImageIsItsOwnCopy() {
    CountingConversion conversion;
    const std::shared_ptr<const QImage> sdr = sdrImage();
    DecodedPixels pixels;
    pixels.assign(sdr, false, conversion.function());
    QCOMPARE(pixels.sdr(), sdr);
    QCOMPARE(pixels.decoded(), sdr);
    QVERIFY(!pixels.hasHdrSource());
    QCOMPARE(conversion.calls->load(), 0);
  }

  void concurrentReadersConvertOnce() {
    CountingConversion conversion;
    DecodedPixels pixels;
    pixels.assign(hdrImage(), true, conversion.function());

    std::vector<std::shared_ptr<const QImage>> results(kConcurrentReaders);
    std::vector<std::unique_ptr<QThread>> readers;
    for (int i = 0; i < kConcurrentReaders; ++i) {
      readers.emplace_back(QThread::create([&pixels, &results, i]() { results[i] = pixels.sdr(); }));
      readers.back()->start();
    }
    for (const auto &reader : readers)
      reader->wait();

    QCOMPARE(conversion.calls->load(), 1);
    for (const auto &result : results)
      QCOMPARE(result, results.front());
    QVERIFY(results.front());
  }

  void aFailedConversionIsReportedOnce() {
    std::atomic<int> calls = 0;
    DecodedPixels pixels;
    pixels.assign(hdrImage(), true, [&calls](const QImage &) {
      ++calls;
      return QImage();
    });
    QTest::ignoreMessage(QtWarningMsg, "DecodedPixels: the HDR image could not be converted to SDR");
    QVERIFY(!pixels.sdr());
    QVERIFY(!pixels.sdr());
    QCOMPARE(calls.load(), 1);
    // The HDR source can still be shown.
    QVERIFY(pixels.decoded());
  }

  void replacingDropsTheHdrSource() {
    CountingConversion conversion;
    DecodedPixels pixels;
    pixels.assign(hdrImage(), true, conversion.function());
    const std::shared_ptr<const QImage> edited = sdrImage();
    pixels.replace(edited);
    QVERIFY(!pixels.hasHdrSource());
    QCOMPARE(pixels.decoded(), edited);
    QCOMPARE(pixels.sdr(), edited);
    QCOMPARE(conversion.calls->load(), 0);
  }

  void assigningNothingClears() {
    CountingConversion conversion;
    DecodedPixels pixels;
    pixels.assign(sdrImage(), false, conversion.function());
    pixels.assign(nullptr, false, conversion.function());
    QVERIFY(!pixels.sdr());
    QVERIFY(!pixels.decoded());
    QCOMPARE(pixels.size(), QSize());
  }
};

int runDecodedPixelsTests(int argc, char **argv) {
  DecodedPixelsTests tests;
  return QTest::qExec(&tests, argc, argv);
}

#include "tst_decodedpixels.moc"
