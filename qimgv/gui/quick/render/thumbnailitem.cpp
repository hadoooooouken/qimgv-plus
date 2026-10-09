#include "thumbnailitem.h"

#include <QDebug>
#include <QGuiApplication>
#include <QQuickWindow>
#include <QSGImageNode>
#include <QSGRendererInterface>
#include <memory>

#include "gui/quick/render/thumbnailmaterial.h"

namespace {
constexpr qreal kMinimumDevicePixelRatio = 1.0;

// One device pixel in item units.
float deviceEdgeWidth(qreal devicePixelRatio) {
  return static_cast<float>(1.0 / qMax(devicePixelRatio, kMinimumDevicePixelRatio));
}
} // namespace

QSize thumbnailDrawSize(QSize pixelSize, QSize sourceSize, QSize maximumSize,
                        qreal devicePixelRatio) {
  if (pixelSize.isEmpty() || maximumSize.isEmpty())
    return {};
  QSize drawSize = pixelSize.scaled(maximumSize, Qt::KeepAspectRatio);

  // A cached thumbnail may be smaller than its cell even though the source
  // image is large enough to fill it: enlargement is limited by the source
  // resolution, not by the resolution of the cached copy.
  const bool sourceIsLandscape = sourceSize.width() > sourceSize.height();
  const bool thumbnailIsLandscape = pixelSize.width() > pixelSize.height();
  if (sourceIsLandscape != thumbnailIsLandscape)
    sourceSize.transpose();

  const qreal effectiveRatio = qMax(devicePixelRatio, kMinimumDevicePixelRatio);
  const QSize maximumLogicalSize(qRound(sourceSize.width() / effectiveRatio),
                                 qRound(sourceSize.height() / effectiveRatio));
  if (maximumLogicalSize.isValid() && !maximumLogicalSize.isEmpty() &&
      (drawSize.width() > maximumLogicalSize.width() ||
       drawSize.height() > maximumLogicalSize.height())) {
    drawSize = drawSize.scaled(maximumLogicalSize, Qt::KeepAspectRatio);
  }
  return drawSize;
}

//------------------------------------------------------------------------------
ThumbnailItem::ThumbnailItem(QQuickItem *parent) : QQuickItem(parent) {
  setFlag(ItemHasContents, true);
}

ThumbnailHandle ThumbnailItem::thumbnail() const { return mThumbnail; }

void ThumbnailItem::setThumbnail(const ThumbnailHandle &thumbnail) {
  if (mThumbnail == thumbnail)
    return;
  mThumbnail = thumbnail;
  mImageDirty = true;
  updatePaintedRect();
  update();
  emit thumbnailChanged();
}

QSize ThumbnailItem::maximumSize() const { return mMaximumSize; }

void ThumbnailItem::setMaximumSize(QSize size) {
  if (mMaximumSize == size)
    return;
  mMaximumSize = size;
  updatePaintedRect();
  emit maximumSizeChanged();
}

qreal ThumbnailItem::cornerRadius() const { return mCornerRadius; }

void ThumbnailItem::setCornerRadius(qreal radius) {
  if (qFuzzyCompare(mCornerRadius, radius))
    return;
  mCornerRadius = radius;
  update();
  emit cornerRadiusChanged();
}

qreal ThumbnailItem::highlight() const { return mHighlight; }

void ThumbnailItem::setHighlight(qreal highlight) {
  if (qFuzzyCompare(mHighlight, highlight))
    return;
  mHighlight = highlight;
  update();
  emit highlightChanged();
}

QRectF ThumbnailItem::paintedRect() const { return mPaintedRect; }

void ThumbnailItem::geometryChange(const QRectF &newGeometry,
                                   const QRectF &oldGeometry) {
  QQuickItem::geometryChange(newGeometry, oldGeometry);
  if (newGeometry.size() != oldGeometry.size())
    updatePaintedRect();
}

void ThumbnailItem::itemChange(ItemChange change, const ItemChangeData &value) {
  QQuickItem::itemChange(change, value);
  if (change == ItemSceneChange || change == ItemDevicePixelRatioHasChanged) {
    // A new window has a new scene graph: its node and texture are made anew.
    mImageDirty = true;
    updatePaintedRect();
  }
}

qreal ThumbnailItem::devicePixelRatio() const {
  if (const QQuickWindow *itemWindow = window())
    return itemWindow->effectiveDevicePixelRatio();
  return qGuiApp->devicePixelRatio();
}

// Centred in the item; the origin is truncated to whole units like the
// widget's QPoint.
void ThumbnailItem::updatePaintedRect() {
  QRectF rect;
  if (mThumbnail.isValid()) {
    const QSize drawSize =
        thumbnailDrawSize(mThumbnail.pixelSize(), mThumbnail.sourceSize,
                          mMaximumSize, devicePixelRatio());
    if (!drawSize.isEmpty()) {
      const int left = static_cast<int>((width() - drawSize.width()) / 2.0);
      const int top = static_cast<int>((height() - drawSize.height()) / 2.0);
      rect = QRectF(left, top, drawSize.width(), drawSize.height());
    }
  }
  if (rect == mPaintedRect)
    return;
  mPaintedRect = rect;
  update();
  emit paintedRectChanged();
}

QSGNode *ThumbnailItem::updatePaintNode(QSGNode *oldNode,
                                        UpdatePaintNodeData *updatePaintNodeData) {
  Q_UNUSED(updatePaintNodeData)
  // A node that is not returned belongs to the item, which discards it.
  if (mPaintedRect.isEmpty() || !mThumbnail.isValid()) {
    const std::unique_ptr<QSGNode> discarded(oldNode);
    return nullptr;
  }
  const QQuickWindow *itemWindow = window();
  if (!itemWindow) {
    qWarning() << "ThumbnailItem: updatePaintNode() without a window";
    const std::unique_ptr<QSGNode> discarded(oldNode);
    return nullptr;
  }
  const bool software = itemWindow->rendererInterface()->graphicsApi() ==
                        QSGRendererInterface::Software;
  return software ? updateSoftwareNode(oldNode) : updateMaterialNode(oldNode);
}

QSGNode *ThumbnailItem::updateMaterialNode(QSGNode *oldNode) {
  // Only this item creates the nodes it gets back.
  std::unique_ptr<ThumbnailNode> node(static_cast<ThumbnailNode *>(oldNode));
  if (!node)
    node = std::make_unique<ThumbnailNode>();
  const qint64 cacheKey = mThumbnail.image.cacheKey();
  if (mImageDirty || node->cacheKey() != cacheKey) {
    std::unique_ptr<QSGTexture> texture(
        window()->createTextureFromImage(mThumbnail.image));
    if (!texture) {
      qWarning() << "ThumbnailItem: could not create a texture of"
                 << mThumbnail.image.size() << "pixels";
      return nullptr;
    }
    texture->setFiltering(QSGTexture::Linear);
    node->setTexture(std::move(texture), cacheKey);
    mImageDirty = false;
  }
  ThumbnailMaterial &material = node->thumbnailMaterial();
  material.rectSize = mPaintedRect.size();
  material.cornerRadius = static_cast<float>(mCornerRadius);
  material.highlight = static_cast<float>(mHighlight);
  material.edgeWidth = deviceEdgeWidth(devicePixelRatio());
  node->setRect(mPaintedRect);
  node->markDirty(QSGNode::DirtyMaterial);
  // The scene graph owns the returned node.
  return node.release();
}

QSGNode *ThumbnailItem::updateSoftwareNode(QSGNode *oldNode) {
  std::unique_ptr<QSGImageNode> node(static_cast<QSGImageNode *>(oldNode));
  if (!node) {
    node.reset(window()->createImageNode());
    node->setOwnsTexture(true);
    mImageDirty = true;
  }
  if (mImageDirty) {
    std::unique_ptr<QSGTexture> texture(
        window()->createTextureFromImage(mThumbnail.image));
    if (!texture) {
      qWarning() << "ThumbnailItem: could not create a texture of"
                 << mThumbnail.image.size() << "pixels";
      return nullptr;
    }
    // The node owns its texture (setOwnsTexture()).
    node->setTexture(texture.release());
    node->setFiltering(QSGTexture::Linear);
    mImageDirty = false;
  }
  node->setRect(mPaintedRect);
  return node.release();
}
