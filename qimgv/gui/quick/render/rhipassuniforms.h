#pragma once

#include <QRectF>
#include <QtGlobal>
#include <cstddef>

#include "gui/quick/render/renderframe.h"

// std140 mirrors of the uniform blocks of the RHI shaders in
// res/shaders/rhi, shared by ImageRenderer, RenderDevice and ImageLayer.
// Every offset is checked against the GLSL layout.

// Bindings shared by every shader: the uniform block, the main texture, and
// the second texture of the passes that read one (weight table or colour
// lookup table).
inline constexpr int kUniformBinding = 0;
inline constexpr int kTextureBinding = 1;
inline constexpr int kWeightTableBinding = 2;
inline constexpr int kColorLutBinding = 2;

// Fill helpers for vec4 members.
inline void setUniformRect(float (&target)[4], const QRectF &rect) {
  target[0] = static_cast<float>(rect.x());
  target[1] = static_cast<float>(rect.y());
  target[2] = static_cast<float>(rect.width());
  target[3] = static_cast<float>(rect.height());
}

inline void setUniformRow(float (&target)[4], const float (&row)[3]) {
  target[0] = row[0];
  target[1] = row[1];
  target[2] = row[2];
  target[3] = 0.0f;
}

// Value of the shader's sharpenMode for each RenderEnums::Sharpening.
static_assert(static_cast<int>(RenderEnums::Sharpening::None) == 0);
static_assert(static_cast<int>(RenderEnums::Sharpening::Cas) == 1);
static_assert(static_cast<int>(RenderEnums::Sharpening::Smart) == 2);

// Mirror of the TileParams block in image.vert/.frag. Defaults describe an
// unfiltered, unadjusted draw without checkerboard.
struct TileUniforms {
  static constexpr float kIdentityRow0[4] = {1.0f, 0.0f, 0.0f, 0.0f};
  static constexpr float kIdentityRow1[4] = {0.0f, 1.0f, 0.0f, 0.0f};
  static constexpr float kIdentityRow2[4] = {0.0f, 0.0f, 1.0f, 0.0f};
  static constexpr float kNeutralColorOffset = 0.0f;
  static constexpr qint32 kDisabled = 0;
  static constexpr qint32 kEnabled = 1;

  float mvp[16]{};
  float targetRect[4]{};
  float texRect[4]{};
  float checkerLight[4]{};
  float checkerDark[4]{};
  float colorRow0[4] = {kIdentityRow0[0], kIdentityRow0[1], kIdentityRow0[2],
                        kIdentityRow0[3]};
  float colorRow1[4] = {kIdentityRow1[0], kIdentityRow1[1], kIdentityRow1[2],
                        kIdentityRow1[3]};
  float colorRow2[4] = {kIdentityRow2[0], kIdentityRow2[1], kIdentityRow2[2],
                        kIdentityRow2[3]};
  float checkerOrigin[2]{};
  float texelStep[2]{};
  float checkerTile = 0.0f;
  float checkerFirstCell = 0.0f;
  float colorOffset = kNeutralColorOffset;
  float casSharpening = ImageFilter::kDefaultCasSharpening;
  float casContrast = ImageFilter::kDefaultCasContrast;
  qint32 checkerEnabled = kDisabled;
  qint32 colorEnabled = kDisabled;
  qint32 sharpenMode = static_cast<qint32>(RenderEnums::Sharpening::None);
  qint32 downscaleTaps = kDisabled;
  qint32 padding[3]{};
};
static_assert(offsetof(TileUniforms, targetRect) == 64);
static_assert(offsetof(TileUniforms, texRect) == 80);
static_assert(offsetof(TileUniforms, checkerLight) == 96);
static_assert(offsetof(TileUniforms, checkerDark) == 112);
static_assert(offsetof(TileUniforms, colorRow0) == 128);
static_assert(offsetof(TileUniforms, colorRow1) == 144);
static_assert(offsetof(TileUniforms, colorRow2) == 160);
static_assert(offsetof(TileUniforms, checkerOrigin) == 176);
static_assert(offsetof(TileUniforms, texelStep) == 184);
static_assert(offsetof(TileUniforms, checkerTile) == 192);
static_assert(offsetof(TileUniforms, checkerFirstCell) == 196);
static_assert(offsetof(TileUniforms, colorOffset) == 200);
static_assert(offsetof(TileUniforms, casSharpening) == 204);
static_assert(offsetof(TileUniforms, casContrast) == 208);
static_assert(offsetof(TileUniforms, checkerEnabled) == 212);
static_assert(offsetof(TileUniforms, colorEnabled) == 216);
static_assert(offsetof(TileUniforms, sharpenMode) == 220);
static_assert(offsetof(TileUniforms, downscaleTaps) == 224);
static_assert(sizeof(TileUniforms) % 16 == 0);

// Mirror of the PanoramaParams block in panorama.vert/.frag. One draw per
// image tile covers the whole colour buffer; the fragment shader keeps the
// rays that hit the tile's core.
struct PanoramaUniforms {
  static constexpr qint32 kDisabled = 0;
  static constexpr qint32 kEnabled = 1;

  float mvp[16]{};
  float colorRow0[4] = {TileUniforms::kIdentityRow0[0],
                        TileUniforms::kIdentityRow0[1],
                        TileUniforms::kIdentityRow0[2],
                        TileUniforms::kIdentityRow0[3]};
  float colorRow1[4] = {TileUniforms::kIdentityRow1[0],
                        TileUniforms::kIdentityRow1[1],
                        TileUniforms::kIdentityRow1[2],
                        TileUniforms::kIdentityRow1[3]};
  float colorRow2[4] = {TileUniforms::kIdentityRow2[0],
                        TileUniforms::kIdentityRow2[1],
                        TileUniforms::kIdentityRow2[2],
                        TileUniforms::kIdentityRow2[3]};
  // Core of the tile in normalized image coordinates: left, top, right,
  // bottom (half-open).
  float core[4]{};
  // Texture of the tile in normalized image coordinates: x, y, width,
  // height.
  float texRect[4]{};
  // Colour buffer size in device pixels.
  float targetSize[2]{};
  // tan(fov / 2) and the colour buffer's width / height.
  float tanHalfFov = 0.0f;
  float aspect = 0.0f;
  float sinYaw = 0.0f;
  float cosYaw = 1.0f;
  float sinPitch = 0.0f;
  float cosPitch = 1.0f;
  float colorOffset = TileUniforms::kNeutralColorOffset;
  qint32 colorEnabled = kDisabled;
  // The tile spans the whole image width and its sampler repeats
  // horizontally: no horizontal core test, the texture wraps around.
  qint32 wrapsHorizontally = kDisabled;
  qint32 padding = 0;
};
static_assert(offsetof(PanoramaUniforms, colorRow0) == 64);
static_assert(offsetof(PanoramaUniforms, colorRow1) == 80);
static_assert(offsetof(PanoramaUniforms, colorRow2) == 96);
static_assert(offsetof(PanoramaUniforms, core) == 112);
static_assert(offsetof(PanoramaUniforms, texRect) == 128);
static_assert(offsetof(PanoramaUniforms, targetSize) == 144);
static_assert(offsetof(PanoramaUniforms, tanHalfFov) == 152);
static_assert(offsetof(PanoramaUniforms, aspect) == 156);
static_assert(offsetof(PanoramaUniforms, sinYaw) == 160);
static_assert(offsetof(PanoramaUniforms, cosYaw) == 164);
static_assert(offsetof(PanoramaUniforms, sinPitch) == 168);
static_assert(offsetof(PanoramaUniforms, cosPitch) == 172);
static_assert(offsetof(PanoramaUniforms, colorOffset) == 176);
static_assert(offsetof(PanoramaUniforms, colorEnabled) == 180);
static_assert(offsetof(PanoramaUniforms, wrapsHorizontally) == 184);
static_assert(sizeof(PanoramaUniforms) % 16 == 0);
// A tile's uniform buffer holds either block; a tile is never drawn flat and
// as a panorama in the same frame.
static_assert(sizeof(PanoramaUniforms) <= sizeof(TileUniforms));

// Mirror of the ReduceParams block in boxreduce.vert/.frag.
struct ReduceUniforms {
  float mvp[16]{};
  float srcTexelSize[2]{};
  float dstSize[2]{};
  float ratio[2]{};
  float padding[2]{};
};
static_assert(offsetof(ReduceUniforms, srcTexelSize) == 64);
static_assert(offsetof(ReduceUniforms, dstSize) == 72);
static_assert(offsetof(ReduceUniforms, ratio) == 80);
static_assert(sizeof(ReduceUniforms) % 16 == 0);

// Mirror of the ResampleParams block in resample.vert/.frag.
struct ResampleUniforms {
  // Values of the shader's axis.
  static constexpr qint32 kAxisHorizontal = 0;
  static constexpr qint32 kAxisVertical = 1;
  static constexpr qint32 kIntermediatePass = 0;
  static constexpr qint32 kFinalPass = 1;

  float mvp[16]{};
  float dstSize[2]{};
  qint32 axis = kAxisHorizontal;
  qint32 clampMin = 0;
  qint32 clampMax = 0;
  qint32 crossOffset = 0;
  qint32 finalPass = kIntermediatePass;
  qint32 tapCount = 0;
};
static_assert(offsetof(ResampleUniforms, dstSize) == 64);
static_assert(offsetof(ResampleUniforms, axis) == 72);
static_assert(offsetof(ResampleUniforms, clampMin) == 76);
static_assert(offsetof(ResampleUniforms, clampMax) == 80);
static_assert(offsetof(ResampleUniforms, crossOffset) == 84);
static_assert(offsetof(ResampleUniforms, finalPass) == 88);
static_assert(offsetof(ResampleUniforms, tapCount) == 92);
static_assert(sizeof(ResampleUniforms) % 16 == 0);

// Mirror of the ConvertParams block in convert.vert/.frag.
struct ConvertUniforms {
  // Values of the shader's sourceMode.
  static constexpr qint32 kSourceEncoded = 0;
  static constexpr qint32 kSourceToneMapped = 1;
  // Values of the shader's colorMode.
  static constexpr qint32 kColorNone = 0;
  static constexpr qint32 kColorParametric = 1;
  static constexpr qint32 kColorLut = 2;

  float mvp[16]{};
  float primariesRow0[4]{};
  float primariesRow1[4]{};
  float primariesRow2[4]{};
  float colorRow0[4]{};
  float colorRow1[4]{};
  float colorRow2[4]{};
  float sourceCurve0[4]{};
  float sourceCurve1[4]{};
  float targetCurve0[4]{};
  float targetCurve1[4]{};
  float dstSize[2]{};
  float whiteScale = 0.0f;
  float lutScale = 0.0f;
  float lutOffset = 0.0f;
  qint32 sourceMode = kSourceEncoded;
  qint32 hdrTransfer = 0;
  qint32 toneMapOperator = 0;
  qint32 colorMode = kColorNone;
  qint32 padding[3]{};
};
static_assert(offsetof(ConvertUniforms, primariesRow0) == 64);
static_assert(offsetof(ConvertUniforms, primariesRow1) == 80);
static_assert(offsetof(ConvertUniforms, primariesRow2) == 96);
static_assert(offsetof(ConvertUniforms, colorRow0) == 112);
static_assert(offsetof(ConvertUniforms, colorRow1) == 128);
static_assert(offsetof(ConvertUniforms, colorRow2) == 144);
static_assert(offsetof(ConvertUniforms, sourceCurve0) == 160);
static_assert(offsetof(ConvertUniforms, sourceCurve1) == 176);
static_assert(offsetof(ConvertUniforms, targetCurve0) == 192);
static_assert(offsetof(ConvertUniforms, targetCurve1) == 208);
static_assert(offsetof(ConvertUniforms, dstSize) == 224);
static_assert(offsetof(ConvertUniforms, whiteScale) == 232);
static_assert(offsetof(ConvertUniforms, lutScale) == 236);
static_assert(offsetof(ConvertUniforms, lutOffset) == 240);
static_assert(offsetof(ConvertUniforms, sourceMode) == 244);
static_assert(offsetof(ConvertUniforms, hdrTransfer) == 248);
static_assert(offsetof(ConvertUniforms, toneMapOperator) == 252);
static_assert(offsetof(ConvertUniforms, colorMode) == 256);
static_assert(sizeof(ConvertUniforms) % 16 == 0);

// Values of the conversion shader's hdrTransfer and toneMapOperator.
static_assert(static_cast<int>(HdrTransfer::PQ) == 0);
static_assert(static_cast<int>(HdrTransfer::HLG) == 1);
static_assert(static_cast<int>(HdrTransfer::Linear) == 2);
static_assert(static_cast<int>(RenderEnums::ToneMapOperator::Bt2408) == 0);
static_assert(static_cast<int>(RenderEnums::ToneMapOperator::ReinhardJodie) == 1);
static_assert(static_cast<int>(RenderEnums::ToneMapOperator::AcesFilmic) == 2);
static_assert(static_cast<int>(RenderEnums::ToneMapOperator::Hable) == 3);
