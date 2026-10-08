#include "tilegrid.h"

#include <QtGlobal>

namespace {
// Core and texture range of a tile along one axis, [start, end).
struct AxisSpan {
  int coreStart = 0;
  int coreEnd = 0;
  int textureStart = 0;
  int textureEnd = 0;
};

QList<AxisSpan> splitAxis(int length, int maxTextureSize) {
  if (length <= maxTextureSize)
    return {AxisSpan{0, length, 0, length}};

  // The core step is a multiple of kOverlap, so every core and every texture
  // start (core start - kOverlap) is aligned to kOverlap.
  const int step = (maxTextureSize - 2 * TileGrid::kOverlap) /
                   TileGrid::kOverlap * TileGrid::kOverlap;
  QList<AxisSpan> spans;
  spans.reserve((length + step - 1) / step);
  for (int start = 0; start < length; start += step) {
    const int end = qMin(start + step, length);
    spans.append(AxisSpan{start, end, qMax(0, start - TileGrid::kOverlap),
                          qMin(length, end + TileGrid::kOverlap)});
  }
  return spans;
}
} // namespace

namespace TileGrid {

QList<ImageTile> layout(QSize imageSize, int maxTextureSize) {
  if (imageSize.isEmpty())
    return {};
  const bool fits = imageSize.width() <= maxTextureSize &&
                    imageSize.height() <= maxTextureSize;
  if (!fits && maxTextureSize < kMinimumTiledTextureSize)
    return {};

  const QList<AxisSpan> columns = splitAxis(imageSize.width(), maxTextureSize);
  const QList<AxisSpan> rows = splitAxis(imageSize.height(), maxTextureSize);
  QList<ImageTile> tiles;
  tiles.reserve(columns.size() * rows.size());
  for (const AxisSpan &row : rows) {
    for (const AxisSpan &column : columns) {
      tiles.append(ImageTile{
          QRect(QPoint(column.coreStart, row.coreStart),
                QPoint(column.coreEnd - 1, row.coreEnd - 1)),
          QRect(QPoint(column.textureStart, row.textureStart),
                QPoint(column.textureEnd - 1, row.textureEnd - 1))});
    }
  }
  return tiles;
}

} // namespace TileGrid
