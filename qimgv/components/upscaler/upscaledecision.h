#pragma once

// What Core does about the AI upscale of the visible area once the viewer
// has asked for it at a new size (after a CPU scale in the widget UI,
// directly in the Qt Quick UI).
enum class UpscaleAction {
    None,
    // Panorama: no upscale; drop the crop and the upscaler's state.
    HideCropAndReset,
    // Upscayl is off: drop the crop.
    HideCrop,
    // Start (or restart) the upscale of the visible area.
    Request,
    // The zoom is within the configured limit: mark the shown crop outdated.
    InvalidatePreview
};

struct UpscaleInputs {
    bool panoramaMode = false;
    bool useUpscayl = false;
    // The image is a static image (animations are never upscaled).
    bool staticImage = false;
    // Settings::upscaylLimitEnabled() / upscaylLimitValue(): upscale only
    // above this zoom, percent.
    bool limitEnabled = false;
    int limitPercent = 0;
    // Current zoom of the viewer, percent.
    float zoomPercent = 0.0f;
    // Width the image is displayed at and its own width, pixels.
    int displayedWidth = 0;
    int imageWidth = 0;
};

[[nodiscard]] UpscaleAction decideUpscale(const UpscaleInputs &inputs);
