#pragma once

#include <QColor>
#include <QImage>
#include <QObject>
#include <QPointF>
#include <QtQml/qqmlregistration.h>
#include <memory>

// Value types handed from ImageRenderItem (GUI thread) to ImageRenderer
// (render thread) in QQuickRhiItemRenderer::synchronize().

// QML access is scoped: RenderEnums.TextureSampling.Trilinear.
namespace RenderEnums {
Q_NAMESPACE
QML_ELEMENT
Q_CLASSINFO("RegisterEnumClassesUnscoped", "false")

enum class TextureSampling {
  // Nearest texel, no mipmaps (the viewer's "nearest" filter).
  Nearest,
  // Bilinear from the full-resolution level only.
  Bilinear,
  // Bilinear magnification, trilinear (mipmapped) minification; the GPU path
  // of the viewer's smooth filters.
  Trilinear,
};
Q_ENUM_NS(TextureSampling)
} // namespace RenderEnums

// Where the image is drawn, in the units of ViewTransform (S0.4).
struct ImagePlacement {
  // Top-left corner of the image in item coordinates (logical pixels).
  QPointF position;
  // Device pixels per source pixel; 1.0 shows the image 1:1 on screen at any
  // device pixel ratio.
  qreal scale = 1.0;

  friend bool operator==(const ImagePlacement &,
                         const ImagePlacement &) = default;
};

struct RenderSettings {
  RenderEnums::TextureSampling sampling =
      RenderEnums::TextureSampling::Trilinear;
  // Straight-alpha colour of the item outside the image.
  QColor backgroundColor = Qt::black;
  // Draws a checkerboard under images that have an alpha channel.
  bool transparencyGrid = false;
  // Upper bound for the side of one texture in pixels, applied below the
  // GPU's QRhi::TextureSizeMax; kNoTileSizeLimit uses the GPU limit only.
  int maxTileSize = kNoTileSizeLimit;

  static constexpr int kNoTileSizeLimit = 0;

  friend bool operator==(const RenderSettings &,
                         const RenderSettings &) = default;
};

// Everything one frame of ImageRenderer depends on.
struct RenderFrame {
  // Immutable and shared with the GUI thread; null when no image is shown.
  std::shared_ptr<const QImage> image;
  // Incremented by every ImageRenderItem::setImage(); the renderer re-uploads
  // when it differs from the generation of its textures.
  quint64 imageGeneration = 0;
  ImagePlacement placement;
  RenderSettings settings;
  // QQuickWindow::effectiveDevicePixelRatio() of the item's window.
  qreal devicePixelRatio = 1.0;
};
