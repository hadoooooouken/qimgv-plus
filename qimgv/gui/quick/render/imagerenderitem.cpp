#include "imagerenderitem.h"

#include <QDebug>
#include <QImage>
#include <QQuickWindow>
#include <utility>

#include "gui/quick/render/colorlutbuilder.h"
#include "gui/quick/render/imagerenderer.h"
#include "utils/hdrsource.h"
#include "utils/hdrtonemapper.h"

namespace {
using namespace Qt::StringLiterals;

constexpr int kOpaqueAlpha = 255;

// RenderEnums::ToneMapOperator carries the values of the CPU tone mapper's
// operators (and of Settings::hdrToneMappingOperator()).
static_assert(static_cast<int>(RenderEnums::ToneMapOperator::Bt2408) ==
              static_cast<int>(ToneMapOperator::Bt2408));
static_assert(static_cast<int>(RenderEnums::ToneMapOperator::ReinhardJodie) ==
              static_cast<int>(ToneMapOperator::ReinhardJodie));
static_assert(static_cast<int>(RenderEnums::ToneMapOperator::AcesFilmic) ==
              static_cast<int>(ToneMapOperator::AcesFilmic));
static_assert(static_cast<int>(RenderEnums::ToneMapOperator::Hable) ==
              static_cast<int>(ToneMapOperator::Hable));
} // namespace

//------------------------------------------------------------------------------
ImageRenderItem::ImageRenderItem(QQuickItem *parent)
    : QQuickRhiItem(parent),
      mErrorChannel(std::make_shared<RenderErrorChannel>(
          this, [this](const QString &message) {
            qWarning().noquote() << "ImageRenderItem:" << message;
            emit renderError(message);
          })) {
  setAlphaBlending(mSettings.backgroundColor.alpha() < kOpaqueAlpha);
  mLutBuilder = new ColorLutBuilder(this);
  connect(mLutBuilder, &ColorLutBuilder::lutReady, this,
          &ImageRenderItem::refreshConversion);
  connect(mLutBuilder, &ColorLutBuilder::lutFailed, this,
          &ImageRenderItem::reportConversionError);
}

//------------------------------------------------------------------------------
QQuickRhiItemRenderer *ImageRenderItem::createRenderer() {
  // Owned by QQuickRhiItem, which deletes it on the render thread.
  return new ImageRenderer();
}

//------------------------------------------------------------------------------
void ImageRenderItem::setImage(std::shared_ptr<const QImage> image) {
  if (image && image->isNull())
    image.reset();
  const QSize previousSize = imageSize();
  mImage = std::move(image);
  ++mImageGeneration;
  mImageIsHdr = mImage && isHdrImage(*mImage);
  mHdrEncoding = mImageIsHdr ? detectHdrSourceEncoding(*mImage)
                             : HdrSourceEncoding{};
  mImageColorSpace = mImage ? mImage->colorSpace() : QColorSpace();
  refreshConversion();
  update();
  if (imageSize() != previousSize)
    emit imageChanged();
}

QSize ImageRenderItem::imageSize() const {
  return mImage ? mImage->size() : QSize();
}

//------------------------------------------------------------------------------
void ImageRenderItem::setPlacement(const ImagePlacement &placement) {
  if (mPlacement == placement)
    return;
  mPlacement = placement;
  update();
  emit placementChanged();
}

const ImagePlacement &ImageRenderItem::placement() const { return mPlacement; }

QPointF ImageRenderItem::imagePosition() const { return mPlacement.position; }

void ImageRenderItem::setImagePosition(QPointF position) {
  ImagePlacement placement = mPlacement;
  placement.position = position;
  setPlacement(placement);
}

qreal ImageRenderItem::imageScale() const { return mPlacement.scale; }

void ImageRenderItem::setImageScale(qreal scale) {
  ImagePlacement placement = mPlacement;
  placement.scale = scale;
  setPlacement(placement);
}

//------------------------------------------------------------------------------
void ImageRenderItem::setRenderSettings(const RenderSettings &settings) {
  applySettings(settings);
}

const RenderSettings &ImageRenderItem::renderSettings() const {
  return mSettings;
}

RenderEnums::TextureSampling ImageRenderItem::sampling() const {
  return mSettings.sampling;
}

void ImageRenderItem::setSampling(RenderEnums::TextureSampling sampling) {
  RenderSettings settings = mSettings;
  settings.sampling = sampling;
  applySettings(settings);
}

QColor ImageRenderItem::backgroundColor() const {
  return mSettings.backgroundColor;
}

void ImageRenderItem::setBackgroundColor(const QColor &color) {
  RenderSettings settings = mSettings;
  settings.backgroundColor = color;
  applySettings(settings);
}

bool ImageRenderItem::transparencyGrid() const {
  return mSettings.transparencyGrid;
}

void ImageRenderItem::setTransparencyGrid(bool enabled) {
  RenderSettings settings = mSettings;
  settings.transparencyGrid = enabled;
  applySettings(settings);
}

void ImageRenderItem::applySettings(const RenderSettings &settings) {
  if (mSettings == settings)
    return;
  const RenderSettings previous = std::exchange(mSettings, settings);
  // The rendered texture is only blended into the scene when the background
  // is translucent; an opaque item is cheaper to composite.
  setAlphaBlending(mSettings.backgroundColor.alpha() < kOpaqueAlpha);
  update();
  if (previous.sampling != mSettings.sampling)
    emit samplingChanged();
  if (previous.backgroundColor != mSettings.backgroundColor)
    emit backgroundColorChanged();
  if (previous.transparencyGrid != mSettings.transparencyGrid)
    emit transparencyGridChanged();
}

//------------------------------------------------------------------------------
void ImageRenderItem::setImageFilter(const ImageFilter &filter) {
  if (mFilter == filter)
    return;
  mFilter = filter;
  update();
  emit imageFilterChanged();
}

const ImageFilter &ImageRenderItem::imageFilter() const { return mFilter; }

RenderEnums::Resampling ImageRenderItem::resampling() const {
  return mFilter.resampling;
}

void ImageRenderItem::setResampling(RenderEnums::Resampling resampling) {
  ImageFilter filter = mFilter;
  filter.resampling = resampling;
  setImageFilter(filter);
}

RenderEnums::Sharpening ImageRenderItem::sharpening() const {
  return mFilter.sharpening;
}

void ImageRenderItem::setSharpening(RenderEnums::Sharpening sharpening) {
  ImageFilter filter = mFilter;
  filter.sharpening = sharpening;
  setImageFilter(filter);
}

qreal ImageRenderItem::casSharpening() const { return mFilter.casSharpening; }

void ImageRenderItem::setCasSharpening(qreal sharpening) {
  ImageFilter filter = mFilter;
  filter.casSharpening = static_cast<float>(sharpening);
  setImageFilter(filter);
}

qreal ImageRenderItem::casContrast() const { return mFilter.casContrast; }

void ImageRenderItem::setCasContrast(qreal contrast) {
  ImageFilter filter = mFilter;
  filter.casContrast = static_cast<float>(contrast);
  setImageFilter(filter);
}

void ImageRenderItem::setColorAdjustments(const ColorAdjustments &adjustments) {
  ImageFilter filter = mFilter;
  filter.colorAdjustments = adjustments;
  setImageFilter(filter);
}

//------------------------------------------------------------------------------
bool ImageRenderItem::isSettled() const { return mSettled; }

void ImageRenderItem::setSettled(bool settled) {
  if (mSettled == settled)
    return;
  mSettled = settled;
  update();
  emit settledChanged();
}

//------------------------------------------------------------------------------
void ImageRenderItem::setToneMapping(const ToneMapping &toneMapping) {
  if (mToneMapping == toneMapping)
    return;
  mToneMapping = toneMapping;
  refreshConversion();
  emit toneMappingChanged();
}

const ToneMapping &ImageRenderItem::toneMapping() const { return mToneMapping; }

bool ImageRenderItem::isToneMappingEnabled() const {
  return mToneMapping.enabled;
}

void ImageRenderItem::setToneMappingEnabled(bool enabled) {
  ToneMapping toneMapping = mToneMapping;
  toneMapping.enabled = enabled;
  setToneMapping(toneMapping);
}

RenderEnums::ToneMapOperator ImageRenderItem::toneMapOperator() const {
  return mToneMapping.op;
}

void ImageRenderItem::setToneMapOperator(RenderEnums::ToneMapOperator op) {
  ToneMapping toneMapping = mToneMapping;
  toneMapping.op = op;
  setToneMapping(toneMapping);
}

qreal ImageRenderItem::hdrWhiteLevel() const { return mToneMapping.whiteNits; }

void ImageRenderItem::setHdrWhiteLevel(qreal nits) {
  ToneMapping toneMapping = mToneMapping;
  toneMapping.whiteNits = static_cast<float>(nits);
  setToneMapping(toneMapping);
}

void ImageRenderItem::setColorManagement(
    const ColorManagement &colorManagement) {
  if (mColorManagement == colorManagement)
    return;
  mColorManagement = colorManagement;
  refreshConversion();
  emit colorManagementChanged();
}

const ColorManagement &ImageRenderItem::colorManagement() const {
  return mColorManagement;
}

const SourceConversion &ImageRenderItem::sourceConversion() const {
  return mConversion;
}

void ImageRenderItem::refreshConversion() {
  SourceConversion conversion;
  if (mImage) {
    conversion.hdr = mImageIsHdr;
    if (mImageIsHdr) {
      conversion.hdrEncoding = mHdrEncoding;
      conversion.toneMapping = mToneMapping;
    }
    if (mColorManagement.enabled) {
      // HDR images reach the colour transform as sRGB, like the output of
      // HdrToneMapper; untagged images are sRGB, like in ColorManager.
      const QColorSpace source =
          mImageIsHdr || !mImageColorSpace.isValid()
              ? QColorSpace(QColorSpace::SRgb)
              : mImageColorSpace;
      const QColorSpace &target = mColorManagement.target;
      conversion.color = planColorTransform(source, target);
      if (conversion.color.kind == ColorTransformKind::Unsupported) {
        reportConversionError(
            u"The image is shown without colour management: %1"_s.arg(
                conversion.color.reason));
        conversion.color = ColorTransformPlan{};
      } else if (conversion.color.kind == ColorTransformKind::Lut) {
        conversion.lut = mLutBuilder->find(source, target);
        if (!conversion.lut)
          mLutBuilder->request(source, target);
      }
    }
  }
  if (conversion == mConversion)
    return;
  mConversion = std::move(conversion);
  update();
}

void ImageRenderItem::reportConversionError(const QString &message) {
  if (message == mLastConversionError)
    return;
  mLastConversionError = message;
  qWarning().noquote() << "ImageRenderItem:" << message;
  emit renderError(message);
}

//------------------------------------------------------------------------------
RenderFrame ImageRenderItem::frameSnapshot() const {
  RenderFrame frame;
  frame.image = mImage;
  frame.imageGeneration = mImageGeneration;
  frame.placement = mPlacement;
  frame.settings = mSettings;
  frame.filter = mFilter;
  frame.conversion = mConversion;
  frame.settled = mSettled;
  if (const QQuickWindow *itemWindow = window())
    frame.devicePixelRatio = itemWindow->effectiveDevicePixelRatio();
  return frame;
}

std::shared_ptr<RenderErrorChannel> ImageRenderItem::errorChannel() const {
  return mErrorChannel;
}
