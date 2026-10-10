#include "batchjobrules.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

#include <algorithm>

namespace BatchJobRules {
namespace {
using namespace Qt::StringLiterals;

constexpr int kPngMinimumCompression = 0;
constexpr int kPngMaximumCompression = 9;
constexpr int kMinimumQuality = 1;
constexpr int kMaximumQuality = 100;

constexpr QSize kFallbackOriginalSize(2560, 1440);
constexpr double kPercentPerUnit = 100.0;

constexpr ResolutionLimits kResolutionLimits{.maximumSide = 12288, .maximumPixels = 100'000'000};
// Upscayl output is larger than what the regular resize path is allowed to
// produce.
constexpr ResolutionLimits kUpscaylResolutionLimits{.maximumSide = 16384,
                                                    .maximumPixels = 268'435'456};
constexpr double kPixelsPerMegapixel = 1'000'000.0;

constexpr qint64 kBytesPerKilobyte = 1024;
constexpr qint64 kBytesPerMegabyte = kBytesPerKilobyte * kBytesPerKilobyte;
constexpr int kSizeDecimals = 1;

// Second character of a drive path ("C:").
constexpr qsizetype kDriveSeparatorIndex = 1;

bool isPngExtension(QStringView extension) {
    return extension == u"png";
}

bool isLossyExtension(QStringView extension) {
    return extension == u"jpg" || extension == u"webp" || extension == u"jxl" ||
           extension == u"avif";
}

int truncated(double value) {
    return static_cast<int>(value);
}

QSize clampedToSideRange(QSize size) {
    return {std::clamp(size.width(), kMinimumSide, kMaximumSide),
            std::clamp(size.height(), kMinimumSide, kMaximumSide)};
}

QString fileSizeText(qint64 bytes) {
    if (bytes > kBytesPerMegabyte)
        return QString::number(static_cast<double>(bytes) / kBytesPerMegabyte, 'f',
                               kSizeDecimals) +
               u" MB"_s;
    return QString::number(static_cast<double>(bytes) / kBytesPerKilobyte, 'f', kSizeDecimals) +
           u" KB"_s;
}
} // namespace

//--- output format -----------------------------------------------------------
QList<OutputFormat> outputFormats() {
    return {
        {u"JPEG (*.jpg *.jpeg *.jpe *.jfif)"_s, u"jpg"_s},
        {u"PNG (*.png)"_s, u"png"_s},
        {u"WebP (*.webp)"_s, u"webp"_s},
        {u"JPEG-XL (*.jxl)"_s, u"jxl"_s},
        {u"AVIF (*.avif *.avifs)"_s, u"avif"_s},
        {u"QOI (*.qoi)"_s, u"qoi"_s},
        {u"BMP (*.bmp)"_s, u"bmp"_s},
        {u"TIFF (*.tif *.tiff)"_s, u"tif"_s},
    };
}

QualityScale qualityScaleFor(QStringView extension) {
    if (isPngExtension(extension))
        return {.kind = QualityKind::Compression,
                .minimum = kPngMinimumCompression,
                .maximum = kPngMaximumCompression};
    if (isLossyExtension(extension))
        return {.kind = QualityKind::Quality, .minimum = kMinimumQuality, .maximum = kMaximumQuality};
    return {};
}

int defaultQualityFor(QStringView extension, const SaveQualityDefaults &defaults) {
    if (isPngExtension(extension))
        return defaults.png;
    if (extension == u"jpg")
        return defaults.jpeg;
    if (isLossyExtension(extension))
        return defaults.modern;
    return -1;
}

QString qualityToolTip(QualityKind kind) {
    switch (kind) {
    case QualityKind::Compression:
        return QCoreApplication::translate("BatchConverterDialog", "PNG Compression level (0 - none, 9 - max)");
    case QualityKind::Quality:
        return QCoreApplication::translate("BatchConverterDialog", "Quality (1 - lowest, 100 - highest)");
    case QualityKind::None:
        break;
    }
    return {};
}

//--- resize ------------------------------------------------------------------
QSize fallbackOriginalSize() {
    return kFallbackOriginalSize;
}

QSize percentTarget(QSize original, double percent) {
    const double scale = percent / kPercentPerUnit;
    return clampedToSideRange(
        {truncated(original.width() * scale), truncated(original.height() * scale)});
}

QSize widthEdited(QSize original, QSize target, int width, bool keepAspectRatio) {
    QSize edited(width, target.height());
    if (keepAspectRatio && original.width() > 0) {
        const float factor = static_cast<float>(width) / static_cast<float>(original.width());
        edited.setHeight(truncated(static_cast<float>(original.height()) * factor));
    }
    return clampedToSideRange(edited);
}

QSize heightEdited(QSize original, QSize target, int height, bool keepAspectRatio) {
    QSize edited(target.width(), height);
    if (keepAspectRatio && original.height() > 0) {
        const float factor = static_cast<float>(height) / static_cast<float>(original.height());
        edited.setWidth(truncated(static_cast<float>(original.width()) * factor));
    }
    return clampedToSideRange(edited);
}

QList<CommonSize> commonSizes() {
    const QList<QSize> sizes{{1280, 720},   {1366, 768},  {1440, 900},  {1440, 1050},
                             {1600, 1200},  {1920, 1080}, {1920, 1200}, {2560, 1080},
                             {2560, 1440},  {2560, 1600}, {3840, 1600}, {3840, 2160}};
    QList<CommonSize> result{{QCoreApplication::translate("BatchConverterDialog", "Original size"), QSize()}};
    for (const QSize size : sizes)
        result.append({u"%1 x %2"_s.arg(size.width()).arg(size.height()), size});
    return result;
}

QList<ScalingFilterOption> scalingFilters() {
    return {
        {QCoreApplication::translate("BatchConverterDialog", "Nearest"), QI_FILTER_NEAREST},
        {QCoreApplication::translate("BatchConverterDialog", "Bilinear"), QI_FILTER_BILINEAR},
        {QCoreApplication::translate("BatchConverterDialog", "Smart sharpen"), QI_FILTER_SMART},
        {QCoreApplication::translate("BatchConverterDialog", "Magic Kernel Sharp 2021"), QI_FILTER_MKS2021},
    };
}

int defaultScalingFilterIndex() {
    const QList<ScalingFilterOption> filters = scalingFilters();
    const auto found = std::ranges::find(filters, QI_FILTER_MKS2021, &ScalingFilterOption::filter);
    return static_cast<int>(std::distance(filters.begin(), found));
}

ResolutionLimits resolutionLimitsFor(bool useUpscayl) {
    return useUpscayl ? kUpscaylResolutionLimits : kResolutionLimits;
}

bool exceedsResolutionLimits(QSize target, bool useUpscayl) {
    const ResolutionLimits limits = resolutionLimitsFor(useUpscayl);
    return target.width() > limits.maximumSide || target.height() > limits.maximumSide ||
           static_cast<qint64>(target.width()) * target.height() > limits.maximumPixels;
}

//--- transform ---------------------------------------------------------------
QString rotationLabel(RotationAngle angle) {
    switch (angle) {
    case RotationAngle::Rotate0:
        return QCoreApplication::translate("BatchConverterDialog", "0°");
    case RotationAngle::Rotate90:
        return QCoreApplication::translate("BatchConverterDialog", "90°");
    case RotationAngle::Rotate180:
        return QCoreApplication::translate("BatchConverterDialog", "180°");
    case RotationAngle::Rotate270:
        return QCoreApplication::translate("BatchConverterDialog", "270°");
    }
    return {};
}

//--- colour adjustments -----------------------------------------------------
QList<ColorSliderSpec> colorSliders() {
    const QString percent = u"%"_s;
    const QString degree = u"°"_s;
    return {
        {QCoreApplication::translate("BatchConverterDialog", "Exposure:"), -2.0, 2.0, 0.0, 0.01, 2, {}},
        {QCoreApplication::translate("BatchConverterDialog", "Contrast:"), 0.0, 300.0, 100.0, 1.0, 0, percent},
        {QCoreApplication::translate("BatchConverterDialog", "Brightness:"), -100.0, 100.0, 0.0, 1.0, 0, percent},
        {QCoreApplication::translate("BatchConverterDialog", "Saturation:"), 0.0, 200.0, 100.0, 1.0, 0, percent},
        {QCoreApplication::translate("BatchConverterDialog", "Hue:"), -180.0, 180.0, 0.0, 1.0, 0, degree},
        {QCoreApplication::translate("BatchConverterDialog", "Temperature:"), -50.0, 50.0, 0.0, 1.0, 0, {}},
        {QCoreApplication::translate("BatchConverterDialog", "Tint:"), -50.0, 50.0, 0.0, 1.0, 0, {}},
    };
}

ColorSliderValues defaultColorValues() {
    ColorSliderValues values{};
    const QList<ColorSliderSpec> sliders = colorSliders();
    for (std::size_t i = 0; i < values.size(); ++i)
        values[i] = sliders[static_cast<qsizetype>(i)].defaultValue;
    return values;
}

ColorAdjustments colorAdjustmentsFor(const ColorSliderValues &values) {
    const auto value = [&values](ColorSlider slider) {
        return values[static_cast<std::size_t>(slider)];
    };
    const auto fraction = [&value](ColorSlider slider) {
        return static_cast<float>(value(slider) / kPercentPerUnit);
    };
    ColorAdjustments adjustments;
    adjustments.exposure = static_cast<float>(value(ColorSlider::Exposure));
    adjustments.contrast = fraction(ColorSlider::Contrast);
    adjustments.brightness = fraction(ColorSlider::Brightness);
    adjustments.saturation = fraction(ColorSlider::Saturation);
    adjustments.hue = static_cast<float>(value(ColorSlider::Hue));
    adjustments.temperature = fraction(ColorSlider::Temperature);
    adjustments.tint = fraction(ColorSlider::Tint);
    return adjustments;
}

//--- output ------------------------------------------------------------------
QString defaultPattern() {
    return QCoreApplication::translate("BatchConverterDialog", "{name}_converted");
}

QString patternHelp() {
    return QCoreApplication::translate("BatchConverterDialog", "Available: {name}, {ext}, {date}, {index}");
}

bool isPatternSafe(const QString &pattern) {
    const QString trimmed = pattern.trimmed();
    const bool drive = trimmed.size() > kDriveSeparatorIndex &&
                       trimmed[kDriveSeparatorIndex] == u':';
    return !trimmed.contains(u".."_s) && !trimmed.startsWith(u'/') && !trimmed.startsWith(u'\\') &&
           !drive;
}

QString initialOutputDirectory(const QString &preferredDirectory, const QString &firstFilePath) {
    if (!preferredDirectory.isEmpty()) {
        const QFileInfo preferred(preferredDirectory);
        if (preferred.exists() && preferred.isDir())
            return preferred.absoluteFilePath();
    }
    if (!firstFilePath.isEmpty())
        return QFileInfo(firstFilePath).absolutePath();
    return {};
}

//--- start -------------------------------------------------------------------
BatchJob finalJob(const BatchJobDraft &draft) {
    BatchJob job = draft.job;
    job.useUpscayl = job.doResize && job.useUpscayl;
    job.colorAdjustments = draft.colorEnabled ? colorAdjustmentsFor(draft.colorValues)
                                              : ColorAdjustments{};
    job.outputDir = job.outputDir.trimmed();
    return job;
}

StartProblem checkStart(const StartCheck &check) {
    const QString directory = check.outputDirectory.trimmed();
    if (directory.isEmpty() || !QDir(directory).exists())
        return StartProblem::InvalidOutputDirectory;
    if (!isPatternSafe(check.pattern))
        return StartProblem::UnsafePattern;
    if (check.selectedCount == 0)
        return StartProblem::NothingSelected;
    if (check.resize && exceedsResolutionLimits(check.targetSize, check.useUpscayl))
        return StartProblem::ResolutionLimitExceeded;
    return StartProblem::None;
}

Message startProblemMessage(StartProblem problem, const StartCheck &check) {
    switch (problem) {
    case StartProblem::InvalidOutputDirectory:
        return {QCoreApplication::translate("BatchConverterDialog", "Invalid Directory"), QCoreApplication::translate("BatchConverterDialog", "Please select a valid output directory.")};
    case StartProblem::UnsafePattern:
        return {QCoreApplication::translate("BatchConverterDialog", "Invalid Pattern"),
                QCoreApplication::translate("BatchConverterDialog", "Filename pattern cannot contain path traversal sequences (..) or "
                           "absolute paths.")};
    case StartProblem::NothingSelected:
        return {QCoreApplication::translate("BatchConverterDialog", "No files"),
                QCoreApplication::translate("BatchConverterDialog", "No files selected in the queue. Please check at least one file.")};
    case StartProblem::ResolutionLimitExceeded: {
        const ResolutionLimits limits = resolutionLimitsFor(check.useUpscayl);
        return {QCoreApplication::translate("BatchConverterDialog", "Resolution Limit Exceeded"),
                QCoreApplication::translate("BatchConverterDialog", "Target resolution (%1x%2) exceeds safety limits.\n\n"
                           "Maximum allowed dimension: %3 px\n"
                           "Maximum allowed pixel count: %4 MP\n\n"
                           "Please reduce the percentage or absolute size.")
                    .arg(check.targetSize.width())
                    .arg(check.targetSize.height())
                    .arg(limits.maximumSide)
                    .arg(static_cast<double>(limits.maximumPixels) / kPixelsPerMegapixel, 0, 'f', 0)};
    }
    case StartProblem::None:
        break;
    }
    return {};
}

//--- texts -------------------------------------------------------------------
QString itemStateText(BatchItemState state) {
    switch (state) {
    case BatchItemState::Pending:
        return QCoreApplication::translate("BatchItemWidget", "Pending");
    case BatchItemState::Processing:
        return QCoreApplication::translate("BatchConverter", "Processing...");
    case BatchItemState::Done:
        return QCoreApplication::translate("BatchConverter", "Done");
    case BatchItemState::Failed:
        return QCoreApplication::translate("BatchConverter", "Failed");
    case BatchItemState::Stopped:
        return QCoreApplication::translate("BatchConverter", "Stopped");
    }
    return {};
}

QString sourceInfoText(const QString &format, QSize imageSize, qint64 fileSize) {
    return u"%1 • %2x%3 • %4"_s.arg(format.toUpper())
        .arg(imageSize.width())
        .arg(imageSize.height())
        .arg(fileSizeText(fileSize));
}

QString selectionText(int selectedCount, qint64 selectedBytes) {
    return QCoreApplication::translate("BatchConverterDialog", "%1 files selected (%2 MB)")
        .arg(selectedCount)
        .arg(QString::number(static_cast<double>(selectedBytes) / kBytesPerMegabyte, 'f',
                             kSizeDecimals));
}

QString readyText() {
    return QCoreApplication::translate("BatchConverterDialog", "Ready to convert.");
}

QString processingText() {
    return QCoreApplication::translate("BatchConverterDialog", "Processing...");
}

QString processedText(int processed, int total) {
    return QCoreApplication::translate("BatchConverterDialog", "Processed %1 / %2 files.").arg(processed).arg(total);
}

QString stoppingText() {
    return QCoreApplication::translate("BatchConverterDialog", "Stopping...");
}

QString finishedText(int succeeded, int failed) {
    return QCoreApplication::translate("BatchConverterDialog", "Finished. Success: %1, Failed: %2").arg(succeeded).arg(failed);
}

QString stoppedText(int succeeded, int failed) {
    return QCoreApplication::translate("BatchConverterDialog", "Stopped by user. Success: %1, Failed: %2").arg(succeeded).arg(failed);
}

QString abortedText() {
    return QCoreApplication::translate("BatchConverterDialog", "Batch aborted.");
}

Message completedMessage(int succeeded, int failed, int total) {
    return {QCoreApplication::translate("BatchConverterDialog", "Batch Conversion Complete"),
            QCoreApplication::translate("BatchConverterDialog", "Batch process complete.\n\nSuccessfully converted: %1\nFailed: %2\nTotal "
                       "files: %3")
                .arg(succeeded)
                .arg(failed)
                .arg(total)};
}

Message startFailedMessage(const QString &reason) {
    return {QCoreApplication::translate("BatchConverterDialog", "Batch Conversion Failed"), reason};
}

} // namespace BatchJobRules
