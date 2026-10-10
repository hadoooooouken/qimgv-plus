#include "settingsscales.h"

#include <QCoreApplication>
#include <QtMath>

namespace {
using namespace Qt::StringLiterals;

// The texts keep the translation context of the widget dialog.
constexpr char kContext[] = "SettingsDialog";

constexpr float kPercentScale = 100.f;
constexpr qreal kOpacityPercentScale = 100.0;
constexpr float kBaseMouseScrollingSpeed = 0.5f;
constexpr float kMouseScrollingSpeedStep = 0.25f;
constexpr int kFactorDecimals = 2;

constexpr int kFastPngCompression = 3;
constexpr int kBalancedPngCompression = 6;
} // namespace

namespace SettingsScales {

SettingsRanges ranges() {
    SettingsRanges r;
    r.autoResizeLimit = {.from = kAutoResizeLimitMinStep,
                         .to = kAutoResizeLimitMaxStep,
                         .step = 1,
                         .pageStep = kAutoResizeLimitPageStep};
    r.panelHideDelay = {.from = kMinPanelHideDelayMs,
                        .to = kMaxPanelHideDelayMs,
                        .step = kPanelHideDelayStepMs,
                        .pageStep = kPanelHideDelayStepMs};
    r.panelSize = {.from = kPanelSizeMinStep,
                   .to = kPanelSizeMaxStep,
                   .step = 1,
                   .pageStep = kPanelSizePageStep};
    r.slideshowInterval = {.from = kMinSlideshowIntervalMs,
                           .to = kMaxSlideshowIntervalMs,
                           .step = 1,
                           .pageStep = 1};
    r.expandLimit = {.from = 0, .to = kMaxExpandLimit, .step = 1, .pageStep = 1};
    r.zoomStep = {.from = kMinZoomStepPercent,
                  .to = kMaxZoomStepPercent,
                  .step = 1,
                  .pageStep = kZoomStepPageStep};
    r.casSharpening = {.from = kMinCasSharpeningPercent, .to = kMaxPercent, .step = 1, .pageStep = 1};
    r.casContrast = {.from = 0, .to = kMaxPercent, .step = 1, .pageStep = 1};
    r.opacity = {.from = 0, .to = kMaxPercent, .step = 1, .pageStep = 1};
    r.mouseScrollingSpeed = {.from = 0, .to = kMaxMouseScrollingSpeedStep, .step = 1, .pageStep = 1};
    r.thumbnailerThreads = {.from = kMinThumbnailerThreads,
                            .to = kMaxThumbnailerThreads,
                            .step = 1,
                            .pageStep = 1};
    r.thumbnailResolution = {.from = kMinThumbnailResolution,
                             .to = kMaxThumbnailResolution,
                             .step = kThumbnailResolutionStep,
                             .pageStep = kThumbnailResolutionStep};
    r.thumbnailCacheSize = {.from = 0,
                            .to = kMaxThumbnailCacheSizeMB,
                            .step = kThumbnailCacheSizeStepMB,
                            .pageStep = kThumbnailCacheSizeStepMB};
    r.quality = {.from = 0, .to = kMaxPercent, .step = 1, .pageStep = kQualityPageStep};
    r.pngCompression = {.from = 0, .to = kMaxPngCompression, .step = 1, .pageStep = 1};
    r.memoryLimit = {.from = kMinMemoryLimitMB,
                     .to = kMaxMemoryLimitMB,
                     .step = kMemoryLimitStepMB,
                     .pageStep = kMemoryLimitStepMB};
    r.upscaylLimit = {.from = kMinUpscaylLimitPercent,
                      .to = kMaxUpscaylLimitPercent,
                      .step = kUpscaylLimitStepPercent,
                      .pageStep = kUpscaylLimitPageStep};
    return r;
}

int snapped(int value, int step) {
    if (step <= 0)
        return value;
    return ((value + step / 2) / step) * step;
}

int autoResizeLimitStep(int percent) {
    return percent / kAutoResizeLimitPercentPerStep;
}

int autoResizeLimitPercent(int step) {
    return step * kAutoResizeLimitPercentPerStep;
}

int panelSizeStep(int pixels) {
    return pixels / kPanelSizePixelsPerStep;
}

int panelSizePixels(int step) {
    return step * kPanelSizePixelsPerStep;
}

int zoomStepPercent(float zoomStep) {
    return qRound(zoomStep * kPercentScale);
}

float zoomStep(int percent) {
    return static_cast<float>(percent) / kPercentScale;
}

int mouseScrollingSpeedStep(float speed) {
    return qRound((speed - kBaseMouseScrollingSpeed) / kMouseScrollingSpeedStep);
}

float mouseScrollingSpeed(int step) {
    return kBaseMouseScrollingSpeed + static_cast<float>(step) * kMouseScrollingSpeedStep;
}

int opacityPercent(qreal opacity) {
    return qRound(opacity * kOpacityPercentScale);
}

qreal opacity(int percent) {
    return static_cast<qreal>(percent) / kOpacityPercentScale;
}

int casPercent(float value) {
    return qRound(value * kPercentScale);
}

float casValue(int percent) {
    return static_cast<float>(percent) / kPercentScale;
}

QString autoResizeLimitText(int step) {
    return QString::number(autoResizeLimitPercent(step)) + u'%';
}

QString panelHideDelayText(int ms) {
    return QCoreApplication::translate(kContext, QT_TRANSLATE_NOOP("SettingsDialog", "%1 ms")).arg(ms);
}

QString expandLimitText(int limit) {
    if (limit == 0)
        return u"-"_s;
    return QString::number(limit) + u'x';
}

QString zoomStepText(int percent) {
    return QString::number(zoomStep(percent), 'f', kFactorDecimals) + u'x';
}

QString casValueText(int percent) {
    return QString::number(casValue(percent), 'f', kFactorDecimals);
}

QString percentText(int percent) {
    return QString::number(percent) + u'%';
}

QString mouseScrollingSpeedText(int step) {
    return QString::number(mouseScrollingSpeed(step), 'f', kFactorDecimals) + u'x';
}

QString thumbnailResolutionText(int pixels) {
    return QString::number(pixels) + u" px"_s;
}

QString pngCompressionText(int level) {
    const char *description = nullptr;
    if (level == 0)
        description = QT_TRANSLATE_NOOP("SettingsDialog", "None (Uncompressed)");
    else if (level <= kFastPngCompression)
        description = QT_TRANSLATE_NOOP("SettingsDialog", "Fast");
    else if (level <= kBalancedPngCompression)
        description = QT_TRANSLATE_NOOP("SettingsDialog", "Balanced");
    else
        description = QT_TRANSLATE_NOOP("SettingsDialog", "Maximum");
    return u"Level %1 (%2)"_s.arg(level).arg(QCoreApplication::translate(kContext, description));
}

} // namespace SettingsScales
