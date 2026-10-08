#pragma once

#include <QColor>
#include <QPointF>
#include <QQuickRhiItem>
#include <QSize>
#include <QtQml/qqmlregistration.h>
#include <memory>

#include "gui/quick/render/renderframe.h"
#include "gui/quick/render/rendererrorchannel.h"

class QImage;

// GPU image view of the Qt Quick UI: draws one image through QRhi with
// premultiplied alpha, mipmaps and nearest / bilinear / trilinear sampling,
// over a background colour and an optional transparency checkerboard.
// Images larger than the GPU texture size limit are split into tiles
// (TileGrid) instead of falling back to the CPU.
//
// The ImageFilter adds CAS or smart sharpening and colour adjustments. While
// `settled` is true and the image is shown below 1:1, the renderer replaces
// the mip chain with an exact-ratio downsample; whoever drives the view sets
// settled to false during zoom, pan, resize and animation playback.
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
  Q_PROPERTY(RenderEnums::Sharpening sharpening READ sharpening WRITE setSharpening NOTIFY imageFilterChanged FINAL)
  Q_PROPERTY(qreal casSharpening READ casSharpening WRITE setCasSharpening NOTIFY imageFilterChanged FINAL)
  Q_PROPERTY(qreal casContrast READ casContrast WRITE setCasContrast NOTIFY imageFilterChanged FINAL)
  Q_PROPERTY(bool settled READ isSettled WRITE setSettled NOTIFY settledChanged FINAL)

public:
  explicit ImageRenderItem(QQuickItem *parent = nullptr);

  // Shows image, or nothing for a null pointer or a null image. The image is
  // shared with the render thread and must not be modified afterwards.
  void setImage(std::shared_ptr<const QImage> image);
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
  [[nodiscard]] RenderEnums::Sharpening sharpening() const;
  void setSharpening(RenderEnums::Sharpening sharpening);
  [[nodiscard]] qreal casSharpening() const;
  void setCasSharpening(qreal sharpening);
  [[nodiscard]] qreal casContrast() const;
  void setCasContrast(qreal contrast);
  void setColorAdjustments(const ColorAdjustments &adjustments);

  [[nodiscard]] bool isSettled() const;
  void setSettled(bool settled);

  // Render-thread side, called from ImageRenderer::synchronize() only.
  [[nodiscard]] RenderFrame frameSnapshot() const;
  // Channel through which the renderer reports errors; they are emitted as
  // renderError() on the GUI thread.
  [[nodiscard]] std::shared_ptr<RenderErrorChannel> errorChannel() const;

signals:
  void samplingChanged();
  void backgroundColorChanged();
  void transparencyGridChanged();
  void placementChanged();
  void imageChanged();
  void imageFilterChanged();
  void settledChanged();
  void renderError(const QString &message);

protected:
  QQuickRhiItemRenderer *createRenderer() override;

private:
  void applySettings(const RenderSettings &settings);

  std::shared_ptr<const QImage> mImage;
  quint64 mImageGeneration = 0;
  ImagePlacement mPlacement;
  RenderSettings mSettings;
  ImageFilter mFilter;
  bool mSettled = false;
  std::shared_ptr<RenderErrorChannel> mErrorChannel;
};
