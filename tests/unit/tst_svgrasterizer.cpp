#include <QFile>
#include <QImage>
#include <QPainter>
#include <QSignalSpy>
#include <QSvgRenderer>
#include <QTemporaryDir>
#include <QTest>

#include "components/svgrasterizer/svgrasterizer.h"
#include "testsuites.h"

namespace {
using namespace Qt::StringLiterals;

// A 100 x 50 document of opaque rectangles on whole units, so that rasters
// at whole multiples of its size have no partly covered pixels.
const QByteArray kDocument = "<svg xmlns='http://www.w3.org/2000/svg' width='100' height='50' viewBox='0 0 100 50'>\n"
    "<rect x='0' y='0' width='100' height='50' fill='#204060'/>\n"
    "<rect x='10' y='5' width='30' height='20' fill='#e04020'/>\n"
    "<rect x='45' y='25' width='5' height='25' fill='#20e040'/>\n"
    "<rect x='70' y='10' width='1' height='30' fill='#ffffff'/>\n"
    "</svg>\n"_ba;
constexpr QSize kDocumentSize(100, 50);
// Reference raster: the whole document at kReferenceScale times its size.
constexpr int kReferenceScale = 4;
constexpr QRect kSourceRect(10, 5, 40, 20);
// Antialiasing of the two paths may differ in the last bit.
constexpr int kRasterTolerance = 1;
constexpr int kChannels = 4;
constexpr int kTimeoutMs = 5000;

QImage referenceRaster(const QByteArray &document, QSize size) {
  QSvgRenderer renderer(document);
  QImage image(size, QImage::Format_ARGB32_Premultiplied);
  image.fill(Qt::transparent);
  QPainter painter(&image);
  renderer.render(&painter, QRectF(QPointF(0, 0), QSizeF(size)));
  return image;
}

int maxDifference(const QImage &a, const QImage &b) {
  const QImage first = a.convertToFormat(QImage::Format_ARGB32_Premultiplied);
  const QImage second = b.convertToFormat(QImage::Format_ARGB32_Premultiplied);
  int worst = 0;
  for (int y = 0; y < first.height(); ++y) {
    const uchar *lineA = first.constScanLine(y);
    const uchar *lineB = second.constScanLine(y);
    for (int i = 0; i < first.width() * kChannels; ++i)
      worst = qMax(worst, std::abs(int(lineA[i]) - int(lineB[i])));
  }
  return worst;
}
} // namespace

class SvgRasterizerTests : public QObject {
  Q_OBJECT

private slots:
  void initTestCase() {
    QVERIFY(mDir.isValid());
    mDocument = writeFile(u"document.svg"_s, kDocument);
    mBroken = writeFile(u"broken.svg"_s, "<svg><rect"_ba);
  }

  void classifiesFilesSizesAndRequests() {
    QVERIFY(SvgRasterizer::isSvgFile(u"C:/a/b.svg"_s));
    QVERIFY(SvgRasterizer::isSvgFile(u"C:/a/B.SVG"_s));
    QVERIFY(!SvgRasterizer::isSvgFile(u"C:/a/b.svgz"_s));
    QVERIFY(!SvgRasterizer::isSvgFile(u"C:/a/b.png"_s));

    QVERIFY(SvgRasterizer::matchesImage(kDocumentSize, kDocumentSize));
    // Decoded with a limited side, the aspect ratio kept.
    QVERIFY(SvgRasterizer::matchesImage(kDocumentSize, QSize(40, 20)));
    // Rotated by an edit.
    QVERIFY(!SvgRasterizer::matchesImage(kDocumentSize, kDocumentSize.transposed()));
    QVERIFY(!SvgRasterizer::matchesImage(QSize(), kDocumentSize));

    const QSize target = kSourceRect.size() * kReferenceScale;
    QVERIFY(SvgRasterizer::isValidRequest({kSourceRect, target}, kDocumentSize));
    QVERIFY(!SvgRasterizer::isValidRequest({QRect(), target}, kDocumentSize));
    QVERIFY(!SvgRasterizer::isValidRequest({kSourceRect, QSize()}, kDocumentSize));
    QVERIFY(!SvgRasterizer::isValidRequest({QRect(90, 0, 20, 10), target}, kDocumentSize));
    QVERIFY(!SvgRasterizer::isValidRequest(
        {kSourceRect, QSize(SvgRasterizer::kMaxTargetDimension + 1, 1)}, kDocumentSize));
  }

  void rasterOfAPartMatchesTheWholeDocument() {
    SvgRasterizer rasterizer;
    QSignalSpy ready(&rasterizer, &SvgRasterizer::documentReady);
    QSignalSpy rasterized(&rasterizer, &SvgRasterizer::rasterized);
    rasterizer.open(mDocument, kDocumentSize);
    QVERIFY(rasterizer.isOpen());
    QVERIFY(!rasterizer.isReady());
    QVERIFY(ready.wait(kTimeoutMs));
    QCOMPARE(ready.at(0).at(0).toSize(), kDocumentSize);
    QCOMPARE(rasterizer.documentSize(), kDocumentSize);

    const SvgRasterRequest request{kSourceRect, kSourceRect.size() * kReferenceScale};
    const quint64 id = rasterizer.request(request);
    QVERIFY(id != SvgRasterizer::kNoRequest);
    QVERIFY(rasterized.wait(kTimeoutMs));
    const auto raster = rasterized.at(0).at(0).value<SvgRaster>();
    QCOMPARE(raster.requestId, id);
    QCOMPARE(raster.request, request);
    QCOMPARE(raster.image.size(), request.targetSize);

    const QImage reference = referenceRaster(kDocument, kDocumentSize * kReferenceScale);
    const QRect area(kSourceRect.topLeft() * kReferenceScale, request.targetSize);
    QVERIFY(maxDifference(raster.image, reference.copy(area)) <= kRasterTolerance);
    // Not a stretched copy of a raster at the document size: the red
    // rectangle's edge is sharp at the raster's scale.
    QVERIFY(maxDifference(raster.image,
                          referenceRaster(kDocument, kDocumentSize)
                              .copy(kSourceRect)
                              .scaled(request.targetSize, Qt::IgnoreAspectRatio,
                                      Qt::SmoothTransformation)) > kRasterTolerance);
  }

  // Only the latest request is reported; earlier ones are replaced while
  // queued or discarded when they already run.
  void onlyTheLatestRequestIsReported() {
    SvgRasterizer rasterizer;
    QSignalSpy ready(&rasterizer, &SvgRasterizer::documentReady);
    QSignalSpy rasterized(&rasterizer, &SvgRasterizer::rasterized);
    rasterizer.open(mDocument, kDocumentSize);
    QVERIFY(ready.wait(kTimeoutMs));

    const QRect whole(QPoint(0, 0), kDocumentSize);
    rasterizer.request({whole, kDocumentSize * 2});
    rasterizer.request({whole, kDocumentSize * 3});
    const quint64 last = rasterizer.request({kSourceRect, kSourceRect.size()});
    QTRY_VERIFY_WITH_TIMEOUT(!rasterizer.isBusy(), kTimeoutMs);
    QCoreApplication::processEvents();
    QCOMPARE(rasterized.count(), 1);
    QCOMPARE(rasterized.at(0).at(0).value<SvgRaster>().requestId, last);
  }

  void cancelledAndClosedRequestsAreNotReported() {
    SvgRasterizer rasterizer;
    QSignalSpy ready(&rasterizer, &SvgRasterizer::documentReady);
    QSignalSpy rasterized(&rasterizer, &SvgRasterizer::rasterized);
    rasterizer.open(mDocument, kDocumentSize);
    QVERIFY(ready.wait(kTimeoutMs));

    rasterizer.request({kSourceRect, kSourceRect.size()});
    rasterizer.cancelRequests();
    QTRY_VERIFY_WITH_TIMEOUT(!rasterizer.isBusy(), kTimeoutMs);
    QCoreApplication::processEvents();
    QCOMPARE(rasterized.count(), 0);

    rasterizer.request({kSourceRect, kSourceRect.size()});
    rasterizer.close();
    QVERIFY(!rasterizer.isOpen());
    QTRY_VERIFY_WITH_TIMEOUT(!rasterizer.isBusy(), kTimeoutMs);
    QCoreApplication::processEvents();
    QCOMPARE(rasterized.count(), 0);

    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(u"no document is ready"_s));
    QCOMPARE(rasterizer.request({kSourceRect, kSourceRect.size()}),
             SvgRasterizer::kNoRequest);
  }

  // A document replaced while it is still being opened is never reported.
  void reopeningDropsThePreviousDocument() {
    SvgRasterizer rasterizer;
    QSignalSpy ready(&rasterizer, &SvgRasterizer::documentReady);
    QSignalSpy failed(&rasterizer, &SvgRasterizer::documentFailed);
    rasterizer.open(mBroken, kDocumentSize);
    rasterizer.open(mDocument, kDocumentSize);
    QVERIFY(ready.wait(kTimeoutMs));
    QTRY_VERIFY_WITH_TIMEOUT(!rasterizer.isBusy(), kTimeoutMs);
    QCoreApplication::processEvents();
    QCOMPARE(ready.count(), 1);
    QCOMPARE(failed.count(), 0);
  }

  void brokenAndMissingDocumentsFail() {
    SvgRasterizer rasterizer;
    QSignalSpy failed(&rasterizer, &SvgRasterizer::documentFailed);
    rasterizer.open(mBroken, kDocumentSize);
    QVERIFY(failed.wait(kTimeoutMs));
    QVERIFY(!rasterizer.isOpen());

    rasterizer.open(mDir.filePath(u"missing.svg"_s), kDocumentSize);
    QVERIFY(failed.wait(kTimeoutMs));
    QVERIFY(failed.at(1).at(0).toString().contains(u"Cannot open"_s));
    QVERIFY(!rasterizer.isOpen());
  }

private:
  QString writeFile(const QString &name, const QByteArray &content) {
    const QString path = mDir.filePath(name);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(content) != content.size())
      qFatal("Cannot write the test file %s", qPrintable(path));
    return path;
  }

  QTemporaryDir mDir;
  QString mDocument;
  QString mBroken;
};

int runSvgRasterizerTests(int argc, char **argv) {
  SvgRasterizerTests tests;
  return QTest::qExec(&tests, argc, argv);
}

#include "tst_svgrasterizer.moc"
