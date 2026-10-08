#include "imagefiltermode.h"

ImageFilterMode imageFilterModeFor(ScalingFilter filter) {
  using RenderEnums::Resampling;
  using RenderEnums::Sharpening;
  using RenderEnums::TextureSampling;
  switch (filter) {
  case QI_FILTER_NEAREST:
    return {TextureSampling::Nearest, Sharpening::None, Resampling::None};
  case QI_FILTER_CAS:
    return {TextureSampling::Trilinear, Sharpening::Cas, Resampling::None};
  case QI_FILTER_SMART_GPU:
    return {TextureSampling::Trilinear, Sharpening::Smart, Resampling::None};
  case QI_FILTER_MKS2021_GPU:
    return {TextureSampling::Trilinear, Sharpening::None, Resampling::Mks2021};
  case QI_FILTER_BILINEAR:
  case QI_FILTER_SMART:
  case QI_FILTER_MKS2021:
    return {TextureSampling::Trilinear, Sharpening::None, Resampling::None};
  }
  // An out-of-range value read from a corrupted configuration file.
  return {TextureSampling::Trilinear, Sharpening::None, Resampling::None};
}
