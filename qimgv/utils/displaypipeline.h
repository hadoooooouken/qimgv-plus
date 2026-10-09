#pragma once

// Where the displayed pixels of a static image are prepared.
enum class DisplayPipeline {
    // On the CPU at load: HDR tone mapping and colour management produce the
    // displayed image (the widget viewer).
    Cpu,
    // On the GPU by the viewer: the decoded image is displayed as it is (HDR
    // stays HDR); the CPU prepares an SDR copy only when one is needed for
    // editing, saving or copying (the Qt Quick viewer).
    Gpu
};
