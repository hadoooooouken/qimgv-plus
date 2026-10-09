#include "thumbnailmaterial.h"

#include <QDebug>
#include <QMatrix4x4>
#include <QSGMaterialShader>
#include <QtGlobal>
#include <cstddef>
#include <cstring>

namespace {
using namespace Qt::StringLiterals;

constexpr int kSamplerBinding = 1;
constexpr int kQuadVertexCount = 4;

// std140 layout of ThumbnailParams (thumbnail.vert / .frag).
struct ThumbnailUniforms {
  float matrix[16];
  float opacity;
  float padding0;
  float rectSize[2];
  float cornerRadius;
  float highlight;
  float edgeWidth;
};
constexpr std::size_t kMatrixOffset = offsetof(ThumbnailUniforms, matrix);
constexpr std::size_t kOpacityOffset = offsetof(ThumbnailUniforms, opacity);
constexpr std::size_t kRectSizeOffset = offsetof(ThumbnailUniforms, rectSize);
constexpr std::size_t kCornerRadiusOffset =
    offsetof(ThumbnailUniforms, cornerRadius);
constexpr std::size_t kHighlightOffset = offsetof(ThumbnailUniforms, highlight);
constexpr std::size_t kEdgeWidthOffset = offsetof(ThumbnailUniforms, edgeWidth);
static_assert(kOpacityOffset == 64 && kRectSizeOffset == 72 &&
                  kEdgeWidthOffset == 88,
              "ThumbnailUniforms must match the std140 block of the shaders");

class ThumbnailMaterialShader final : public QSGMaterialShader {
public:
  ThumbnailMaterialShader() {
    setShaderFileName(VertexStage,
                      u":/qimgv/render/shaders/thumbnail.vert.qsb"_s);
    setShaderFileName(FragmentStage,
                      u":/qimgv/render/shaders/thumbnail.frag.qsb"_s);
  }

  bool updateUniformData(RenderState &state, QSGMaterial *newMaterial,
                         QSGMaterial *oldMaterial) override {
    Q_UNUSED(oldMaterial)
    QByteArray *buffer = state.uniformData();
    if (!buffer || static_cast<std::size_t>(buffer->size()) <
                       sizeof(ThumbnailUniforms)) {
      qWarning() << "ThumbnailMaterial: the uniform buffer of the thumbnail "
                    "shader is smaller than its parameter block";
      return false;
    }
    char *data = buffer->data();
    if (state.isMatrixDirty()) {
      const QMatrix4x4 matrix = state.combinedMatrix();
      std::memcpy(data + kMatrixOffset, matrix.constData(),
                  sizeof(ThumbnailUniforms::matrix));
    }
    if (state.isOpacityDirty()) {
      const float opacity = state.opacity();
      std::memcpy(data + kOpacityOffset, &opacity, sizeof(opacity));
    }
    const auto *material = static_cast<const ThumbnailMaterial *>(newMaterial);
    const float rectSize[2] = {static_cast<float>(material->rectSize.width()),
                               static_cast<float>(material->rectSize.height())};
    std::memcpy(data + kRectSizeOffset, rectSize, sizeof(rectSize));
    std::memcpy(data + kCornerRadiusOffset, &material->cornerRadius,
                sizeof(float));
    std::memcpy(data + kHighlightOffset, &material->highlight, sizeof(float));
    std::memcpy(data + kEdgeWidthOffset, &material->edgeWidth, sizeof(float));
    return true;
  }

  void updateSampledImage(RenderState &state, int binding,
                          QSGTexture **texture, QSGMaterial *newMaterial,
                          QSGMaterial *oldMaterial) override {
    Q_UNUSED(oldMaterial)
    if (binding != kSamplerBinding)
      return;
    auto *material = static_cast<ThumbnailMaterial *>(newMaterial);
    if (!material->texture) {
      qWarning() << "ThumbnailMaterial: drawn without a texture";
      return;
    }
    material->texture->commitTextureOperations(state.rhi(),
                                               state.resourceUpdateBatch());
    *texture = material->texture;
  }
};
} // namespace

//------------------------------------------------------------------------------
ThumbnailMaterial::ThumbnailMaterial() { setFlag(Blending, true); }

QSGMaterialType *ThumbnailMaterial::type() const {
  static QSGMaterialType materialType;
  return &materialType;
}

QSGMaterialShader *ThumbnailMaterial::createShader(
    QSGRendererInterface::RenderMode renderMode) const {
  Q_UNUSED(renderMode)
  return new ThumbnailMaterialShader;
}

int ThumbnailMaterial::compare(const QSGMaterial *other) const {
  const auto *material = static_cast<const ThumbnailMaterial *>(other);
  if (texture != material->texture) {
    const qint64 left = texture ? texture->comparisonKey() : 0;
    const qint64 right = material->texture ? material->texture->comparisonKey() : 0;
    return left < right ? -1 : 1;
  }
  if (rectSize.width() != material->rectSize.width())
    return rectSize.width() < material->rectSize.width() ? -1 : 1;
  if (rectSize.height() != material->rectSize.height())
    return rectSize.height() < material->rectSize.height() ? -1 : 1;
  if (cornerRadius != material->cornerRadius)
    return cornerRadius < material->cornerRadius ? -1 : 1;
  if (highlight != material->highlight)
    return highlight < material->highlight ? -1 : 1;
  if (edgeWidth != material->edgeWidth)
    return edgeWidth < material->edgeWidth ? -1 : 1;
  return 0;
}

//------------------------------------------------------------------------------
ThumbnailNode::ThumbnailNode()
    : mGeometry(QSGGeometry::defaultAttributes_TexturedPoint2D(),
                kQuadVertexCount) {
  mGeometry.setDrawingMode(QSGGeometry::DrawTriangleStrip);
  setGeometry(&mGeometry);
  setMaterial(&mMaterial);
}

ThumbnailMaterial &ThumbnailNode::thumbnailMaterial() { return mMaterial; }

void ThumbnailNode::setTexture(std::unique_ptr<QSGTexture> texture,
                               qint64 cacheKey) {
  mTexture = std::move(texture);
  mCacheKey = cacheKey;
  mMaterial.texture = mTexture.get();
  markDirty(DirtyMaterial);
}

qint64 ThumbnailNode::cacheKey() const { return mCacheKey; }

void ThumbnailNode::setRect(const QRectF &rect) {
  QSGGeometry::updateTexturedRectGeometry(&mGeometry, rect,
                                          QRectF(0.0, 0.0, 1.0, 1.0));
  markDirty(DirtyGeometry);
}
