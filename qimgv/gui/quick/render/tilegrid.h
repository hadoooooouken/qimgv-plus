#pragma once

#include <QList>
#include <QRect>
#include <QSize>

// One texture of an image that is split into several textures because it is
// larger than the GPU texture size limit. Rects are in source pixels.
struct ImageTile {
  // Region drawn from this tile. The cores of all tiles partition the image
  // without gaps or overlaps.
  QRect core;
  // Region uploaded into the tile's texture: the core plus up to
  // TileGrid::kOverlap pixels of each neighbouring tile, so that filtering
  // at the core edges reads the same texels as an untiled texture would.
  QRect texture;

  friend bool operator==(const ImageTile &, const ImageTile &) = default;
};

// Tile layout of large images for the GPU renderer.
//
// A one-texel overlap only keeps mip level 0 seamless. Every tiled texture
// therefore starts on a multiple of kOverlap = 2^kExactMipLevels source
// pixels and overlaps its neighbours by kOverlap pixels, so that mip levels
// 0..kExactMipLevels of each tile line up with the mip pyramid of the whole
// image and trilinear filtering is seamless down to a scale of
// 1 / 2^kExactMipLevels. Below that scale the coarser mip levels of
// neighbouring tiles may differ slightly at the seams.
//
// Each axis is split independently; an axis that fits into one texture is
// not split.
namespace TileGrid {
inline constexpr int kExactMipLevels = 6;
inline constexpr int kOverlap = 1 << kExactMipLevels;
// Smallest texture size limit that still leaves a core of kOverlap pixels
// between the two overlaps of a tile.
inline constexpr int kMinimumTiledTextureSize = 3 * kOverlap;

// Tiles of an image of imageSize source pixels for textures of at most
// maxTextureSize pixels per side, in row-major order. Returns a single tile
// covering the image when it fits into one texture. Returns an empty list for
// an empty image, and when the image has to be tiled but maxTextureSize is
// below kMinimumTiledTextureSize.
[[nodiscard]] QList<ImageTile> layout(QSize imageSize, int maxTextureSize);
} // namespace TileGrid
