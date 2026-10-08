#pragma once

#include "gui/quick/render/renderframe.h"
#include "settings_types.h"

// How the GPU renderer shows the image for one of the viewer's scaling
// filters (Settings::scalingFilter()).
struct ImageFilterMode {
  RenderEnums::TextureSampling sampling =
      RenderEnums::TextureSampling::Trilinear;
  RenderEnums::Sharpening sharpening = RenderEnums::Sharpening::None;
  RenderEnums::Resampling resampling = RenderEnums::Resampling::None;

  friend bool operator==(const ImageFilterMode &,
                         const ImageFilterMode &) = default;
};

// Same choices as the widget viewer (ImageViewerV2 / FilterPixmapItem):
//  - Nearest: nearest texel, no sharpening;
//  - Bilinear: trilinear minification, no sharpening;
//  - Cas / SmartGpu: trilinear with CAS / smart sharpening;
//  - Mks2021Gpu: trilinear while the view moves, the MKS2021 resampling
//    kernel once it is settled;
//  - Smart / Mks2021: the CPU display scaling filters of the Scaler
//    component. The renderer shows them with trilinear sampling and the
//    exact-ratio downsample, without sharpening.
[[nodiscard]] ImageFilterMode imageFilterModeFor(ScalingFilter filter);
