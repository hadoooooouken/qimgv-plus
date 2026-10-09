#pragma once

#include <QColor>
#include <QColorSpace>
#include <QPointF>
#include <QQuickRhiItem>
#include <QRect>
#include <QSize>
#include <QtQml/qqmlregistration.h>
#include <memory>

#include "gui/quick/render/renderframe.h"
#include "gui/quick/render/rendererrorchannel.h"
#include "gui/quick/render/renderstatistics.h"

class ColorLutBuilder;
class QImage;

// GPU image view of the Qt Quick UI: draws one image through QRhi with
// premultiplied alpha, mipmaps and nearest / bilinear / trilinear sampling,
// over a background colour and an optional transparency checkerboard.
// Images larger than the GPU texture size limit are split into tiles
// (TileGrid) instead of falling back to the CPU.
//
// HDR images (isHdrImage()) are tone mapped on the GPU with the item's
// ToneMapping, and with ColorManagement enabled every image is converted into
// the display colour space it names (SourceConversion). Changing either only
// re-runs the renderer's conversion pass; the image is not decoded again.
// Colour spaces that need a lookup table get it from a ColorLutBuilder on a
// worker thread; until it is ready the image is shown without the colour
// transform.
//
// The ImageFilter adds CAS or smart sharpening and colour adjustments. While
// `settled` is true and the image is shown below 1:1, the renderer replaces
// the mip chain with an exact-ratio downsample; with the MKS2021 resampling
// kernel selected, it instead resamples the visible part of the image with
// that kernel at any scale other than 1:1. Whoever drives the view sets
// settled to false during zoom, pan, resize and animation playback.
//
// Frames of an animation (AnimationPlayer) are passed with
// ImageUpdate::AnimationFrame: a frame of the same size and format is
// uploaded into the textures of the previous one.
//
// An upscaled crop (setUpscaledCrop()) is drawn over the area of the image
// it was made from, with the same filtering and conversion. With the
// Equirectangular projection the image is shown as a 360 degree panorama
// from the PanoramaCamera instead (the crop is then not drawn).
//
// The item only holds GUI-thread state; ImageRenderer renders on the render
// thread from the RenderFrame snapshot taken in synchronize(). Rendering
// errors (resource creation, unsupported formats, missing shaders) are
// reported through renderError() on the GUI thread.
//
// GUI thread only, except frameSnapshot() and errorChannel(), which
// ImageRenderer calls from QQuickRhiItemRenderer::synchronize() while the GUI
// thread is blocked.
class ImageRenderItem : public QQuickRhiItem {
  Q_OBJECT
  QML_ELEMENT
  Q_PROPERTY(RenderEnums::TextureSampling sampling READ sampling WRITE setSampling NOTIFY samplingChanged FINAL)
  Q_PROPERTY(QColor backgroundColor READ backgroundColor WRITE setBackgroundColor NOTIFY backgroundColorChanged FINAL)
  Q_PROPERTY(bool transparencyGrid READ transparencyGrid WRITE setTransparencyGrid NOTIFY transparencyGridChanged FINAL)
  Q_PROPERTY(QPointF imagePosition READ imagePosition WRITE setImagePosition NOTIFY placementChanged FINAL)
  Q_PROPERTY(qreal imageScale READ imageScale WRITE setImageScale NOTIFY placementChanged FINAL)
  Q_PROPERTY(QSize imageSize READ imageSize NOTIFY imageChanged FINAL)
  Q_PROPERTY(RenderEnums::Resampling resampling READ resampling WRITE setResampling NOTIFY imageFilterChanged FINAL)
  Q_PROPERTY(RenderEnums::Sharpening sharpening READ sharpening WRITE setSharpening NOTIFY imageFilterChanged FINAL)
  Q_PROPERTY(qreal casSharpening READ casSharpening WRITE setCasSharpening NOTIFY imageFilterChanged FINAL)
  Q_PROPERTY(qreal casContrast READ casContrast WRITE setCasContrast NOTIFY imageFilterChanged FINAL)
  Q_PROPERTY(bool settled READ isSettled WRITE setSettled NOTIFY settledChanged FINAL)
  Q_PROPERTY(bool toneMapping READ isToneMappingEnabled WRITE setToneMappingEnabled NOTIFY toneMappingChanged FINAL)
  Q_PROPERTY(RenderEnums::ToneMapOperator toneMapOperator READ toneMapOperator WRITE setToneMapOperator NOTIFY toneMappingChanged FINAL)
  Q_PROPERTY(qreal hdrWhiteLevel READ hdrWhiteLevel WRITE setHdrWhiteLevel NOTIFY toneMappingChanged FINAL)
  Q_PROPERTY(RenderEnums::Projection projection READ projection WRITE setProjection NOTIFY projectionChanged FINAL)
  Q_PROPERTY(qreal panoramaYaw READ panoramaYaw WRITE setPanoramaYaw NOTIFY panoramaCameraChanged FINAL)
  Q_PROPERTY(qreal panoramaPitch READ panoramaPitch WRITE setPanoramaPitch NOTIFY panoramaCameraChanged FINAL)
  Q_PROPERTY(qreal panoramaFov READ panoramaFov WRITE setPanoramaFov NOTIFY panoramaCameraChanged FINAL)
  Q_PROPERTY(bool hasUpscaledCrop READ hasUpscaledCrop NOTIFY upscaledCropChanged FINAL)

public:
  // What a setImage() call shows.
  enum class ImageUpdate {
    // A different image: the upscaled crop is dropped.
    NewImage,
    // The next frame of the animation being shown: the crop is kept, and
    // textures (and converted sources) are reused when the frame has the
    // same size and format.
    AnimationFrame,
  };

  explicit ImageRenderItem(QQuickItem *parent = nullptr);

  // Shows image, or nothing for a null pointer or a null image. The image is
  // shared with the render thread and must not be modified afterwards.
  void setImage(std::shared_ptr<const QImage> image,
                ImageUpdate update = ImageUpdate::NewImage);
  [[nodiscard]] QSize imageSize() const;

  void setPlacement(const ImagePlacement &placement);
  [[nodiscard]] const ImagePlacement &placement() const;
  [[nodiscard]] QPointF imagePosition() const;
  void setImagePosition(QPointF position);
  [[nodiscard]] qreal imageScale() const;
  void setImageScale(qreal scale);

  void setRenderSettings(const RenderSettings &settings);
  [[nodiscard]] const RenderSettings &renderSettings() const;
  [[nodiscard]] RenderEnums::TextureSampling sampling() const;
  void setSampling(RenderEnums::TextureSampling sampling);
  [[nodiscard]] QColor backgroundColor() const;
  void setBackgroundColor(const QColor &color);
  [[nodiscard]] bool transparencyGrid() const;
  void setTransparencyGrid(bool enabled);

  void setImageFilter(const ImageFilter &filter);
  [[nodiscard]] const ImageFilter &imageFilter() const;
  [[nodiscard]] RenderEnums::Resampling resampling() const;
  void setResampling(RenderEnums::Resampling resampling);
  [[nodiscard]] RenderEnums::Sharpening sharpening() const;
  void setSharpening(RenderEnums::Sharpening sharpening);
  [[nodiscard]] qreal casSharpening() const;
  void setCasSharpening(qreal sharpening);
  [[nodiscard]] qreal casContrast() const;
  void setCasContrast(qreal contrast);
  void setColorAdjustments(const ColorAdjustments &adjustments);

  [[nodiscard]] bool isSettled() const;
  void setSettled(bool settled);

  void setToneMapping(const ToneMapping &toneMapping);
  [[nodiscard]] const ToneMapping &toneMapping() const;
  [[nodiscard]] bool isToneMappingEnabled() const;
  void setToneMappingEnabled(bool enabled);
  [[nodiscard]] RenderEnums::ToneMapOperator toneMapOperator() const;
  void setToneMapOperator(RenderEnums::ToneMapOperator op);
  // White level in nits.
  [[nodiscard]] qreal hdrWhiteLevel() const;
  void setHdrWhiteLevel(qreal nits);

  // target: ColorManager::getTargetColorSpace() of the caller.
  void setColorManagement(const ColorManagement &colorManagement);
  [[nodiscard]] const ColorManagement &colorManagement() const;
  // The conversion the renderer applies to the current image.
  [[nodiscard]] const SourceConversion &sourceConversion() const;

  // Shows crop (an upscaled copy of sourceRect of the image, in source
  // pixels) over that area; replaces a previous crop. A null or empty crop or
  // an empty sourceRect clears it.
  void setUpscaledCrop(std::shared_ptr<const QImage> crop,
                       const QRect &sourceRect);
  void clearUpscaledCrop();
  [[nodiscard]] bool hasUpscaledCrop() const;
  [[nodiscard]] const SourceConversion &cropConversion() const;

  [[nodiscard]] RenderEnums::Projection projection() const;
  void setProjection(RenderEnums::Projection projection);
  void setPanoramaCamera(const PanoramaCamera &camera);
  [[nodiscard]] const PanoramaCamera &panoramaCamera() const;
  // Degrees.
  [[nodiscard]] qreal panoramaYaw() const;
  void setPanoramaYaw(qreal yaw);
  [[nodiscard]] qreal panoramaPitch() const;
  void setPanoramaPitch(qreal pitch);
  [[nodiscard]] qreal panoramaFov() const;
  void setPanoramaFov(qreal fov);

  // GPU work done by the renderer so far.
  [[nodiscard]] RenderStatisticsSnapshot statistics() const;

  // Render-thread side, called from ImageRenderer::synchronize() only.
  [[nodiscard]] RenderFrame frameSnapshot() const;
  // Channel through which the renderer reports errors; they are emitted as
  // renderError() on the GUI thread.
  [[nodiscard]] std::shared_ptr<RenderErrorChannel> errorChannel() const;
  // Counters the renderer writes.
  [[nodiscard]] std::shared_ptr<RenderStatistics> statisticsChannel() const;

signals:
  void samplingChanged();
  void backgroundColorChanged();
  void transparencyGridChanged();
  void placementChanged();
  void imageChanged();
  void imageFilterChanged();
  void settledChanged();
  void toneMappingChanged();
  void colorManagementChanged();
  void projectionChanged();
  void panoramaCameraChanged();
  void upscaledCropChanged();
  void renderError(const QString &message);

protected:
  QQuickRhiItemRenderer *createRenderer() override;

private:
  // What the conversion of one image depends on, detected once per image.
  struct SourceTraits {
    bool hdr = false;
    HdrSourceEncoding hdrEncoding;
    QColorSpace colorSpace;

    [[nodiscard]] static SourceTraits of(const QImage *image);
  };

  void applySettings(const RenderSettings &settings);
  // Resolves the conversions of the image and the crop from their traits,
  // the tone mapping and the colour management; requests lookup tables when
  // a plan needs one.
  void refreshConversion();
  [[nodiscard]] SourceConversion resolveConversion(const QImage *image,
                                                   const SourceTraits &traits);
  // GUI-thread failures of the conversion setup, reported like render
  // errors, once per distinct message.
  void reportConversionError(const QString &message);

  std::shared_ptr<const QImage> mImage;
  quint64 mImageGeneration = 0;
  bool mImageIsAnimationFrame = false;
  SourceTraits mImageTraits;
  std::shared_ptr<const QImage> mCrop;
  quint64 mCropGeneration = 0;
  QRect mCropSourceRect;
  SourceTraits mCropTraits;
  SourceConversion mCropConversion;
  RenderEnums::Projection mProjection = RenderEnums::Projection::Flat;
  PanoramaCamera mPanoramaCamera;
  ImagePlacement mPlacement;
  RenderSettings mSettings;
  ImageFilter mFilter;
  bool mSettled = false;
  ToneMapping mToneMapping;
  ColorManagement mColorManagement;
  SourceConversion mConversion;
  // Child object; built tables outlive image changes.
  ColorLutBuilder *mLutBuilder = nullptr;
  QString mLastConversionError;
  std::shared_ptr<RenderErrorChannel> mErrorChannel;
  std::shared_ptr<RenderStatistics> mStatistics;
};
