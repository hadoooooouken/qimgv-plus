#pragma once

#include <QSize>
#include <QString>
#include <cstdint>

#include "utils/coloradjustments.h"

// What one batch conversion does to every selected file. Shared by
// BatchConverter (which runs it) and the batch converter dialog (which
// builds it, see batchjobrules.h); free of Settings and of the
// converter's image code.

enum class AspectFitMode : uint8_t {
    Auto,   // Fit into the target box, constrained by whichever side is tighter (per file).
    Width,  // Scale is derived from the target width only; height follows proportionally.
    Height  // Scale is derived from the target height only; width follows proportionally.
};

enum class RotationAngle : uint16_t {
    Rotate0 = 0,    // No rotation (default).
    Rotate90 = 90,
    Rotate180 = 180,
    Rotate270 = 270
};

// State of one file of a batch, as BatchConverter reports it.
enum class BatchItemState : uint8_t {
    Pending,     // Selected, not started yet.
    Processing,  // Handed to a worker.
    Done,        // Converted, or skipped because the destination exists.
    Failed,
    Stopped      // The batch was cancelled before the file was converted.
};

struct BatchJob {
    QString format;
    int quality = 90;
    bool doResize = false;
    bool resizeByPercent = false;
    double resizePercent = 100.0;
    QSize targetSize;
    bool keepAspectRatio = true;
    AspectFitMode aspectFitMode = AspectFitMode::Auto;
    bool useUpscayl = false;
    QString upscaylModel;
    int scalingFilter = 0;
    RotationAngle rotation = RotationAngle::Rotate0;
    bool flipHorizontal = false;
    bool flipVertical = false;
    ColorAdjustments colorAdjustments;
    QString pattern;
    bool overwrite = false;
    QString outputDir;
    bool createSubfolder = false;
};
