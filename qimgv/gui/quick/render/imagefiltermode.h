#pragma once

#include "gui/quick/render/renderframe.h"
#include "settings_types.h"

// How the GPU renderer shows the image for one of the viewer's scaling
// filters (Settings::scalingFilter()).
struct ImageFilterMode {
  RenderEnums::TextureSampling sampling =
      RenderEnums::TextureSampling::Trilinear;
  RenderEnums::Sharpening sharpening = RenderEnums::Sharpening::None;

  friend bool operator==(const ImageFilterMode &,
                         const ImageFilterMode &) = default;
};

// Same choices as the widget viewer (ImageViewerV2 / FilterPixmapItem):
//  - Nearest: nearest texel, no sharpening;
//  - Bilinear: trilinear minification, no sharpening;
//  - Cas / SmartGpu: trilinear with CAS / smart sharpening;
//  - Smart / Mks2021: the CPU display scaling filters. Until their GPU ports
//    (S1.3) they are shown with trilinear sampling and the exact-ratio
//    downsample, without sharpening.
[[nodiscard]] ImageFilterMode imageFilterModeFor(ScalingFilter filter);
