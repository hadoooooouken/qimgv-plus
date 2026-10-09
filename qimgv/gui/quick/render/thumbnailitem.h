#pragma once

#include <QImage>
#include <QQuickItem>
#include <QRectF>
#include <QSize>
#include <QSizeF>
#include <QtQml/qqmlregistration.h>

// Decoded pixels of one thumbnail as a QML value: the image the thumbnailer
// produced (already decoded and scaled on its worker threads) and the size of
// the image it was made from. Copying shares the pixels (QImage).
struct ThumbnailHandle {
  Q_GADGET
  QML_VALUE_TYPE(thumbnailHandle)
  QML_UNCREATABLE("Provided by the thumbnail models")
  Q_PROPERTY(bool valid READ isValid FINAL)
  Q_PROPERTY(QSize pixelSize READ pixelSize FINAL)

public:
  QImage image;
  QSize sourceSize;

  [[nodiscard]] bool isValid() const { return !image.isNull(); }
  [[nodiscard]] QSize pixelSize() const { return image.size(); }

  // Same pixels (QImage::cacheKey()) and source size.
  friend bool operator==(const ThumbnailHandle &left,
                         const ThumbnailHandle &right) {
    return left.image.cacheKey() == right.image.cacheKey() &&
           left.sourceSize == right.sourceSize;
  }
};

// Size, in item units, at which a thumbnail of pixelSize made from an image
// of sourceSize is drawn in an area of maximumSize (ThumbnailWidget::
// updateThumbnailDrawPosition()): scaled to fit, keeping its aspect ratio,
// but not larger than the source image shown at devicePixelRatio. The source
// size is compared in the orientation of the thumbnail. An empty or unknown
// source size does not limit the size.
[[nodiscard]] QSize thumbnailDrawSize(QSize pixelSize, QSize sourceSize,
                                      QSize maximumSize,
                                      qreal devicePixelRatio);

// One thumbnail of the thumbnail strip and the folder view. The texture is
// created from the decoded image with QQuickWindow::createTextureFromImage()
// in updatePaintNode() - no image provider, no URL, no decoding - and only
// again when the image changes. The thumbnail is drawn at
// thumbnailDrawSize() of maximumSize, centred in the item, with rounded
// corners (cornerRadius) and a hover highlight (highlight: the fraction of
// the thumbnail added to itself).
//
// On the software scene graph backend the thumbnail is drawn without rounded
// corners and highlight.
//
// GUI thread only, except updatePaintNode(), which runs on the render thread
// while the GUI thread is blocked.
class ThumbnailItem : public QQuickItem {
  Q_OBJECT
  QML_ELEMENT
  Q_PROPERTY(ThumbnailHandle thumbnail READ thumbnail WRITE setThumbnail NOTIFY thumbnailChanged FINAL)
  Q_PROPERTY(QSize maximumSize READ maximumSize WRITE setMaximumSize NOTIFY maximumSizeChanged FINAL)
  Q_PROPERTY(qreal cornerRadius READ cornerRadius WRITE setCornerRadius NOTIFY cornerRadiusChanged FINAL)
  Q_PROPERTY(qreal highlight READ highlight WRITE setHighlight NOTIFY highlightChanged FINAL)
  Q_PROPERTY(QRectF paintedRect READ paintedRect NOTIFY paintedRectChanged FINAL)

public:
  explicit ThumbnailItem(QQuickItem *parent = nullptr);

  [[nodiscard]] ThumbnailHandle thumbnail() const;
  void setThumbnail(const ThumbnailHandle &thumbnail);
  [[nodiscard]] QSize maximumSize() const;
  void setMaximumSize(QSize size);
  [[nodiscard]] qreal cornerRadius() const;
  void setCornerRadius(qreal radius);
  [[nodiscard]] qreal highlight() const;
  void setHighlight(qreal highlight);
  // Where the thumbnail is drawn, in item coordinates; empty without an
  // image.
  [[nodiscard]] QRectF paintedRect() const;

signals:
  void thumbnailChanged();
  void maximumSizeChanged();
  void cornerRadiusChanged();
  void highlightChanged();
  void paintedRectChanged();

protected:
  QSGNode *updatePaintNode(QSGNode *oldNode,
                           UpdatePaintNodeData *updatePaintNodeData) override;
  void geometryChange(const QRectF &newGeometry,
                      const QRectF &oldGeometry) override;
  void itemChange(ItemChange change, const ItemChangeData &value) override;

private:
  [[nodiscard]] qreal devicePixelRatio() const;
  void updatePaintedRect();
  [[nodiscard]] QSGNode *updateSoftwareNode(QSGNode *oldNode);
  [[nodiscard]] QSGNode *updateMaterialNode(QSGNode *oldNode);

  ThumbnailHandle mThumbnail;
  QSize mMaximumSize;
  qreal mCornerRadius = 0.0;
  qreal mHighlight = 0.0;
  QRectF mPaintedRect;
  // The image changed since its texture was made; written in
  // updatePaintNode() while the GUI thread is blocked.
  bool mImageDirty = true;
};
