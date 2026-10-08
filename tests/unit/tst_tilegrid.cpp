#include <QTest>

#include "gui/quick/render/tilegrid.h"
#include "testsuites.h"

namespace {
// QRhi::TextureSizeMax of Direct3D 11 feature level 11.
constexpr int kD3d11TextureSizeMax = 16384;
constexpr QSize kHugeImage(32000, 8000);

// Every pixel of the image is in exactly one core.
bool coresPartition(const QList<ImageTile> &tiles, QSize imageSize) {
  qint64 coveredArea = 0;
  for (qsizetype a = 0; a < tiles.size(); ++a) {
    if (!QRect(QPoint(0, 0), imageSize).contains(tiles[a].core))
      return false;
    coveredArea += qint64(tiles[a].core.width()) * tiles[a].core.height();
    for (qsizetype b = a + 1; b < tiles.size(); ++b) {
      if (tiles[a].core.intersects(tiles[b].core))
        return false;
    }
  }
  return coveredArea == qint64(imageSize.width()) * imageSize.height();
}
} // namespace

class TileGridTests : public QObject {
  Q_OBJECT

private slots:
  void emptyImageHasNoTiles() {
    QVERIFY(TileGrid::layout(QSize(), kD3d11TextureSizeMax).isEmpty());
    QVERIFY(TileGrid::layout(QSize(0, 100), kD3d11TextureSizeMax).isEmpty());
  }

  void fittingImageIsOneTile() {
    const QSize size(kD3d11TextureSizeMax, 1200);
    const QList<ImageTile> tiles = TileGrid::layout(size, kD3d11TextureSizeMax);
    QCOMPARE(tiles.size(), 1);
    QCOMPARE(tiles.first().core, QRect(QPoint(0, 0), size));
    QCOMPARE(tiles.first().texture, tiles.first().core);
  }

  void fittingImageIgnoresTinyLimit() {
    // No tiling is needed, so the minimum tiled texture size does not apply.
    const QSize size(TileGrid::kOverlap, TileGrid::kOverlap);
    QCOMPARE(TileGrid::layout(size, TileGrid::kOverlap).size(), 1);
  }

  void tooSmallLimitForTilingFails() {
    const QSize size(4 * TileGrid::kMinimumTiledTextureSize, 10);
    QVERIFY(TileGrid::layout(size, TileGrid::kMinimumTiledTextureSize - 1)
                .isEmpty());
    QVERIFY(!TileGrid::layout(size, TileGrid::kMinimumTiledTextureSize)
                 .isEmpty());
  }

  void hugeImageSplitsOnlyTheLongAxis() {
    const QList<ImageTile> tiles =
        TileGrid::layout(kHugeImage, kD3d11TextureSizeMax);
    QCOMPARE(tiles.size(), 2);
    for (const ImageTile &tile : tiles) {
      QCOMPARE(tile.core.top(), 0);
      QCOMPARE(tile.core.height(), kHugeImage.height());
      QCOMPARE(tile.texture.height(), kHugeImage.height());
    }
    QVERIFY(coresPartition(tiles, kHugeImage));
  }

  void tilesRespectLimitAndAlignment_data() {
    QTest::addColumn<QSize>("imageSize");
    QTest::addColumn<int>("limit");
    QTest::newRow("huge on d3d11") << kHugeImage << kD3d11TextureSizeMax;
    QTest::newRow("minimum limit")
        << QSize(400, 300) << TileGrid::kMinimumTiledTextureSize;
    QTest::newRow("unaligned limit") << QSize(1001, 777) << 333;
    QTest::newRow("tall") << QSize(50, 5000) << 1024;
  }

  void tilesRespectLimitAndAlignment() {
    QFETCH(QSize, imageSize);
    QFETCH(int, limit);
    const QList<ImageTile> tiles = TileGrid::layout(imageSize, limit);
    QVERIFY(!tiles.isEmpty());
    QVERIFY(coresPartition(tiles, imageSize));
    const QRect image(QPoint(0, 0), imageSize);
    for (const ImageTile &tile : tiles) {
      QVERIFY(tile.texture.width() <= limit);
      QVERIFY(tile.texture.height() <= limit);
      QVERIFY(tile.texture.contains(tile.core));
      QVERIFY(image.contains(tile.texture));

      // A split axis has kOverlap pixels on every inner side (clamped at
      // the image edge when the last core is narrower) and an aligned
      // texture origin; an unsplit axis covers the whole image.
      const bool splitX = imageSize.width() > limit;
      const bool splitY = imageSize.height() > limit;
      if (splitX) {
        QCOMPARE(tile.texture.left() % TileGrid::kOverlap, 0);
        QCOMPARE(tile.core.left() % TileGrid::kOverlap, 0);
        if (tile.core.left() > 0)
          QCOMPARE(tile.core.left() - tile.texture.left(), TileGrid::kOverlap);
        if (tile.core.right() < image.right())
          QCOMPARE(tile.texture.right() - tile.core.right(),
                   qMin(TileGrid::kOverlap, image.right() - tile.core.right()));
      } else {
        QCOMPARE(tile.texture.left(), 0);
        QCOMPARE(tile.texture.width(), imageSize.width());
      }
      if (splitY) {
        QCOMPARE(tile.texture.top() % TileGrid::kOverlap, 0);
        QCOMPARE(tile.core.top() % TileGrid::kOverlap, 0);
        if (tile.core.top() > 0)
          QCOMPARE(tile.core.top() - tile.texture.top(), TileGrid::kOverlap);
        if (tile.core.bottom() < image.bottom())
          QCOMPARE(tile.texture.bottom() - tile.core.bottom(),
                   qMin(TileGrid::kOverlap, image.bottom() - tile.core.bottom()));
      } else {
        QCOMPARE(tile.texture.top(), 0);
        QCOMPARE(tile.texture.height(), imageSize.height());
      }
    }
  }

  void tilesAreRowMajor() {
    const QList<ImageTile> tiles =
        TileGrid::layout(QSize(400, 300), TileGrid::kMinimumTiledTextureSize);
    for (qsizetype index = 1; index < tiles.size(); ++index) {
      const QRect previous = tiles[index - 1].core;
      const QRect current = tiles[index].core;
      QVERIFY(current.top() > previous.top() ||
              (current.top() == previous.top() &&
               current.left() > previous.left()));
    }
  }
};

int runTileGridTests(int argc, char **argv) {
  TileGridTests tests;
  return QTest::qExec(&tests, argc, argv);
}

#include "tst_tilegrid.moc"
