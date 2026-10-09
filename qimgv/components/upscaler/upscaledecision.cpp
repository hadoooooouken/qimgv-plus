#include "upscaledecision.h"

UpscaleAction decideUpscale(const UpscaleInputs &inputs) {
    if (inputs.panoramaMode)
        return UpscaleAction::HideCropAndReset;
    if (!inputs.useUpscayl)
        return UpscaleAction::HideCrop;
    if (!inputs.staticImage)
        return UpscaleAction::None;

    const bool aboveLimit =
        !inputs.limitEnabled || inputs.zoomPercent > inputs.limitPercent;
    if (inputs.displayedWidth > inputs.imageWidth && aboveLimit)
        return UpscaleAction::Request;
    if (!aboveLimit)
        return UpscaleAction::InvalidatePreview;
    return UpscaleAction::None;
}
