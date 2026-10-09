#pragma once

#include <QColor>
#include <QColorSpace>
#include <QImage>
#include <QObject>
#include <QPointF>
#include <QRect>
#include <QtQml/qqmlregistration.h>
#include <memory>

#include "gui/quick/render/colortransformplan.h"
#include "utils/coloradjustments.h"
#include "utils/hdrsource.h"

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

// Sharpening applied while sampling the image (ports of the widget viewer's
// res/shaders/filter.frag). Never applied at 1:1 scale or to pixels that are
// not fully opaque.
enum class Sharpening {
  None,
  // AMD FidelityFX Contrast Adaptive Sharpening (the viewer's "CAS" filter).
  Cas,
  // Luma unsharp mask (the viewer's "Smart (GPU)" filter).
  Smart,
};
Q_ENUM_NS(Sharpening)

// Resampling kernel that replaces texture sampling while the view is settled
// and the image is not shown 1:1.
enum class Resampling {
  // Texture sampling (and the exact-ratio downsample below 1:1).
  None,
  // Magic Kernel Sharp 2021, separable, same kernel as the CPU
  // ImageLib::scaled_MKS2021() (the viewer's "MKS2021 (GPU)" filter).
  Mks2021,
};
Q_ENUM_NS(Resampling)

// HDR tone mapping operators, the values of ::ToneMapOperator
// (utils/hdrtonemapper.h) and of Settings::hdrToneMappingOperator().
enum class ToneMapOperator {
  // ITU-R BT.2408 luminance knee with a tanh roll-off.
  Bt2408,
  ReinhardJodie,
  // Narkowicz fit of the ACES filmic curve.
  AcesFilmic,
  // Uncharted 2 filmic curve.
  Hable,
};
Q_ENUM_NS(ToneMapOperator)

// How the image is mapped onto the item.
enum class Projection {
  // The image as a flat picture at ImagePlacement.
  Flat,
  // A 360 x 180 degree equirectangular panorama seen from its centre with a
  // PanoramaCamera (the widget viewer's panorama mode, panorama.frag).
  Equirectangular,
};
Q_ENUM_NS(Projection)
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

// View direction of RenderEnums::Projection::Equirectangular, in degrees, as
// PanoramaView (components/viewtransform) holds it, with the signs of the
// widget viewer's panorama.frag; fov is the vertical field of view.
struct PanoramaCamera {
  // PanoramaView::kDefaultFov.
  static constexpr float kDefaultFov = 90.0f;

  float yaw = 0.0f;
  float pitch = 0.0f;
  float fov = kDefaultFov;

  friend bool operator==(const PanoramaCamera &,
                         const PanoramaCamera &) = default;
};

// Filtering of the image: resampling kernel, sharpening and colour
// adjustments.
struct ImageFilter {
  // Defaults of the "casSharpening" / "casContrast" settings.
  static constexpr float kDefaultCasSharpening = 1.0f;
  static constexpr float kDefaultCasContrast = 0.0f;

  RenderEnums::Resampling resampling = RenderEnums::Resampling::None;
  RenderEnums::Sharpening sharpening = RenderEnums::Sharpening::None;
  // CAS strength in [0, 1]; 0 disables CAS.
  float casSharpening = kDefaultCasSharpening;
  // CAS contrast adaptation in [0, 1].
  float casContrast = kDefaultCasContrast;
  ColorAdjustments colorAdjustments;

  friend bool operator==(const ImageFilter &, const ImageFilter &) = default;
};

// HDR tone mapping of HDR images (isHdrImage()), as HdrToneMapper does on
// the CPU (Settings::hdrToneMapping*()).
struct ToneMapping {
  // BT.2408 reference white; also HdrToneMapper's value for a white level
  // that is not positive.
  static constexpr float kDefaultWhiteNits = 203.0f;

  // Off: the stored samples are clamped to [0, 1] and shown as sRGB, like
  // the CPU's fallback conversion.
  bool enabled = true;
  RenderEnums::ToneMapOperator op = RenderEnums::ToneMapOperator::Bt2408;
  // Luminance in nits that is shown as SDR white.
  float whiteNits = kDefaultWhiteNits;

  friend bool operator==(const ToneMapping &, const ToneMapping &) = default;
};

// Display colour management (Settings::colorManagementEnabled()). target is
// the display colour space ColorManager::getTargetColorSpace() returns; the
// renderer never asks ColorManager itself.
struct ColorManagement {
  bool enabled = false;
  QColorSpace target;

  friend bool operator==(const ColorManagement &,
                         const ColorManagement &) = default;
};

// How ImageRenderer turns the uploaded source pixels into the displayed
// texture (res/shaders/rhi/convert.frag), resolved by ImageRenderItem on the
// GUI thread. Inactive for an SDR image already in the display colour space:
// the image is then uploaded and drawn as it is.
struct SourceConversion {
  // The image is HDR: its samples are decoded with hdrEncoding and tone
  // mapped (or clamped) into linear sRGB, which is then the source of the
  // colour transform.
  bool hdr = false;
  HdrSourceEncoding hdrEncoding;
  // HDR images only; the default otherwise.
  ToneMapping toneMapping;
  // Identity, Parametric or Lut; an Unsupported plan is resolved to
  // Identity by the item, which reports it.
  ColorTransformPlan color;
  // The table of a Lut plan; a Lut plan without one is not applied yet.
  std::shared_ptr<const ColorLut> lut;

  [[nodiscard]] bool appliesColorTransform() const {
    return color.kind == ColorTransformKind::Parametric ||
           (color.kind == ColorTransformKind::Lut && lut);
  }
  [[nodiscard]] bool isActive() const {
    return hdr || appliesColorTransform();
  }

  friend bool operator==(const SourceConversion &,
                         const SourceConversion &) = default;
};

// One image the renderer uploads into textures: the image itself and how its
// pixels become displayed texels.
struct LayerSource {
  // Immutable and shared with the GUI thread; null when nothing is shown.
  std::shared_ptr<const QImage> image;
  // Incremented by every new image; the renderer re-uploads when it differs
  // from the generation of its textures. A new image of the same size and
  // format is uploaded into the existing textures.
  quint64 generation = 0;
  SourceConversion conversion;
  // The image is a frame of an animation: the converted sources of SDR
  // images are kept so that the next frame is uploaded into them too.
  bool animationFrame = false;
};

// AI-upscaled part of the image (Upscaler), drawn over the area of the image
// it was made from.
struct UpscaledCrop {
  LayerSource source;
  // Area of the image in source pixels that the crop image covers.
  QRect sourceRect;
};

// Everything one frame of ImageRenderer depends on.
struct RenderFrame {
  LayerSource image;
  // No crop when crop.source.image is null; never drawn in panorama mode.
  UpscaledCrop crop;
  ImagePlacement placement;
  RenderSettings settings;
  ImageFilter filter;
  RenderEnums::Projection projection = RenderEnums::Projection::Flat;
  PanoramaCamera panorama;
  // The view is not being zoomed, panned or animated. Only then does the
  // renderer spend the extra passes of the exact-ratio downsample and of the
  // resampling kernel.
  bool settled = false;
  // QQuickWindow::effectiveDevicePixelRatio() of the item's window.
  qreal devicePixelRatio = 1.0;
};
