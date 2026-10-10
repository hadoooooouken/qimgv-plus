#pragma once

#include <QList>
#include <QSize>
#include <QString>
#include <QStringView>

#include <array>
#include <cstddef>
#include <cstdint>

#include "batchjob.h"
#include "settings_types.h"

// Rules of the batch converter dialog, shared by the widget dialog and the
// Qt Quick view-model: the offered output formats and their quality scales,
// the resize rules and limits, the colour adjustment sliders, the checks
// before a batch starts, the job that is handed to BatchConverter, and the
// texts both dialogs show. Free of Settings and of widgets; the texts keep
// the translation contexts of the widget dialog.
namespace BatchJobRules {

//--- output format -----------------------------------------------------------
struct OutputFormat {
    QString label;      // "JPEG (*.jpg *.jpeg *.jpe *.jfif)"
    QString extension;  // "jpg", the BatchJob::format
};

// The formats in the order the dialogs list them; JPEG first (the default).
[[nodiscard]] QList<OutputFormat> outputFormats();

enum class QualityKind : uint8_t {
    None,         // The format has no quality setting.
    Compression,  // PNG compression level.
    Quality       // Lossy quality.
};

struct QualityScale {
    QualityKind kind = QualityKind::None;
    int minimum = 0;
    int maximum = 0;
};

// The stored save qualities each format starts from.
struct SaveQualityDefaults {
    int jpeg = 0;
    int png = 0;
    int modern = 0;  // WebP, JPEG-XL and AVIF
};

[[nodiscard]] QualityScale qualityScaleFor(QStringView extension);
// The quality a format starts from; -1 for a format without a quality
// setting (the dialogs keep the previous value then).
[[nodiscard]] int defaultQualityFor(QStringView extension, const SaveQualityDefaults &defaults);
// Tool tip of the quality controls; empty for QualityKind::None.
[[nodiscard]] QString qualityToolTip(QualityKind kind);

//--- resize ------------------------------------------------------------------
inline constexpr double kMinimumPercent = 1.0;
inline constexpr double kMaximumPercent = 1600.0;
inline constexpr double kDefaultPercent = 100.0;
inline constexpr int kPercentDecimals = 1;
inline constexpr int kMinimumSide = 1;
inline constexpr int kMaximumSide = 65535;

// Size the resize fields start from when no source image size is known.
[[nodiscard]] QSize fallbackOriginalSize();

// Target of a percent of original. Each side is truncated, as the widget
// dialog did, and kept within kMinimumSide - kMaximumSide.
[[nodiscard]] QSize percentTarget(QSize original, double percent);
// Target after the width (or height) field was set to value: the other side
// follows in the original's aspect ratio when keepAspectRatio (truncated, in
// single precision, as the widget dialog did), otherwise it is kept.
[[nodiscard]] QSize widthEdited(QSize original, QSize target, int width, bool keepAspectRatio);
[[nodiscard]] QSize heightEdited(QSize original, QSize target, int height, bool keepAspectRatio);

struct CommonSize {
    QString label;
    QSize size;  // Invalid for "Original size".
};

// "Original size" first, then the common screen sizes.
[[nodiscard]] QList<CommonSize> commonSizes();

struct ScalingFilterOption {
    QString label;
    ScalingFilter filter = QI_FILTER_BILINEAR;
};

[[nodiscard]] QList<ScalingFilterOption> scalingFilters();
// Magic Kernel Sharp 2021.
[[nodiscard]] int defaultScalingFilterIndex();

// Largest target the converter accepts.
struct ResolutionLimits {
    int maximumSide = 0;
    qint64 maximumPixels = 0;
};

[[nodiscard]] ResolutionLimits resolutionLimitsFor(bool useUpscayl);
[[nodiscard]] bool exceedsResolutionLimits(QSize target, bool useUpscayl);

//--- transform ---------------------------------------------------------------
// The rotation choices in the order of their radio buttons.
inline constexpr std::array<RotationAngle, 4> kRotations{
    RotationAngle::Rotate0, RotationAngle::Rotate90, RotationAngle::Rotate180,
    RotationAngle::Rotate270};

[[nodiscard]] QString rotationLabel(RotationAngle angle);

//--- colour adjustments -----------------------------------------------------
enum class ColorSlider : uint8_t {
    Exposure,
    Contrast,
    Brightness,
    Saturation,
    Hue,
    Temperature,
    Tint,
    Count
};

inline constexpr std::size_t kColorSliderCount = static_cast<std::size_t>(ColorSlider::Count);

// One colour adjustment control: a slider of `step` units between minimum
// and maximum, with a spin box showing `decimals` decimals and suffix.
struct ColorSliderSpec {
    QString label;
    double minimum = 0.0;
    double maximum = 0.0;
    double defaultValue = 0.0;
    double step = 1.0;
    int decimals = 0;
    QString suffix;
};

using ColorSliderValues = std::array<double, kColorSliderCount>;

// In ColorSlider order.
[[nodiscard]] QList<ColorSliderSpec> colorSliders();
[[nodiscard]] ColorSliderValues defaultColorValues();
// The adjustments of the slider values (percent sliders become fractions).
[[nodiscard]] ColorAdjustments colorAdjustmentsFor(const ColorSliderValues &values);

//--- output ------------------------------------------------------------------
[[nodiscard]] QString defaultPattern();
[[nodiscard]] QString patternHelp();
// False for a pattern that could leave the output folder: "..", a leading
// slash or backslash, or a drive.
[[nodiscard]] bool isPatternSafe(const QString &pattern);
// The output folder a batch starts with: preferredDirectory when it is an
// existing folder, otherwise the folder of the first file.
[[nodiscard]] QString initialOutputDirectory(const QString &preferredDirectory,
                                             const QString &firstFilePath);

//--- start -------------------------------------------------------------------
// The dialog state the converter starts from: the job as edited (its
// colorAdjustments are ignored) plus the colour adjustment section.
struct BatchJobDraft {
    BatchJob job;
    bool colorEnabled = false;
    ColorSliderValues colorValues = defaultColorValues();
};

// The job handed to BatchConverter: Upscayl only with resizing, the colour
// adjustments only when their section is enabled, the output folder
// trimmed.
[[nodiscard]] BatchJob finalJob(const BatchJobDraft &draft);

enum class StartProblem : uint8_t {
    None,
    InvalidOutputDirectory,
    UnsafePattern,
    NothingSelected,
    ResolutionLimitExceeded
};

struct StartCheck {
    QString outputDirectory;
    QString pattern;
    int selectedCount = 0;
    bool resize = false;
    bool useUpscayl = false;
    QSize targetSize;
};

// The first problem that keeps the batch from starting, in the order the
// widget dialog checked them.
[[nodiscard]] StartProblem checkStart(const StartCheck &check);

struct Message {
    QString title;
    QString text;
};

[[nodiscard]] Message startProblemMessage(StartProblem problem, const StartCheck &check);

//--- texts -------------------------------------------------------------------
[[nodiscard]] QString itemStateText(BatchItemState state);
// "JPG • 1920x1080 • 1.2 MB": format, image size and file size of a source.
[[nodiscard]] QString sourceInfoText(const QString &format, QSize imageSize, qint64 fileSize);
[[nodiscard]] QString selectionText(int selectedCount, qint64 selectedBytes);
[[nodiscard]] QString readyText();
[[nodiscard]] QString processingText();
[[nodiscard]] QString processedText(int processed, int total);
[[nodiscard]] QString stoppingText();
[[nodiscard]] QString finishedText(int succeeded, int failed);
[[nodiscard]] QString stoppedText(int succeeded, int failed);
[[nodiscard]] QString abortedText();
[[nodiscard]] Message completedMessage(int succeeded, int failed, int total);
[[nodiscard]] Message startFailedMessage(const QString &reason);

} // namespace BatchJobRules
