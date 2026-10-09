#pragma once

#include <QSGGeometryNode>
#include <QSGMaterial>
#include <QSGTexture>
#include <QSizeF>
#include <memory>

// Scene graph material of ThumbnailItem: draws a texture over a quad with
// rounded, antialiased corners and an additive highlight
// (res/shaders/rhi/thumbnail.vert / .frag). The texture is not owned; the
// node that uses the material owns it.
//
// Created and used on the render thread (QQuickItem::updatePaintNode()).
class ThumbnailMaterial final : public QSGMaterial {
public:
  ThumbnailMaterial();

  [[nodiscard]] QSGMaterialType *type() const override;
  [[nodiscard]] QSGMaterialShader *
  createShader(QSGRendererInterface::RenderMode renderMode) const override;
  [[nodiscard]] int compare(const QSGMaterial *other) const override;

  QSGTexture *texture = nullptr;
  // Size of the drawn quad in item units, for the corner distance field.
  QSizeF rectSize;
  float cornerRadius = 0.0f;
  // Fraction of the thumbnail added to itself (0: none).
  float highlight = 0.0f;
  // Width of the antialiased edge in item units (one device pixel).
  float edgeWidth = 1.0f;
};

// Geometry node of one thumbnail with its material and the texture made from
// the thumbnail image, which it owns. cacheKey identifies that image
// (QImage::cacheKey()), so the texture is only created again for new pixels.
class ThumbnailNode final : public QSGGeometryNode {
public:
  ThumbnailNode();

  [[nodiscard]] ThumbnailMaterial &thumbnailMaterial();
  // Replaces the texture (and the cache key it was made from).
  void setTexture(std::unique_ptr<QSGTexture> texture, qint64 cacheKey);
  [[nodiscard]] qint64 cacheKey() const;
  // Sets the quad to rect with texture coordinates 0 - 1.
  void setRect(const QRectF &rect);

private:
  QSGGeometry mGeometry;
  ThumbnailMaterial mMaterial;
  std::unique_ptr<QSGTexture> mTexture;
  qint64 mCacheKey = 0;
};
