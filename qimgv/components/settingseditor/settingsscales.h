#pragma once

#include <QMetaType>
#include <QString>

// Ranges of the settings dialog's sliders and spin boxes, the conversions
// between their units and the stored settings, and the texts shown next to
// them. The conversions are those of the former widget dialog, so existing
// configurations keep their meaning.

// Range of one slider or spin box, in its own units.
struct SettingsRange {
    Q_GADGET
    Q_PROPERTY(int from MEMBER from FINAL)
    Q_PROPERTY(int to MEMBER to FINAL)
    Q_PROPERTY(int step MEMBER step FINAL)
    Q_PROPERTY(int pageStep MEMBER pageStep FINAL)

public:
    int from = 0;
    int to = 0;
    int step = 1;
    int pageStep = 1;

    friend bool operator==(const SettingsRange &, const SettingsRange &) = default;
};

// Every range of the dialog, for QML (`editor.ranges.zoomStep.to`).
struct SettingsRanges {
    Q_GADGET
    Q_PROPERTY(SettingsRange autoResizeLimit MEMBER autoResizeLimit FINAL)
    Q_PROPERTY(SettingsRange panelHideDelay MEMBER panelHideDelay FINAL)
    Q_PROPERTY(SettingsRange panelSize MEMBER panelSize FINAL)
    Q_PROPERTY(SettingsRange slideshowInterval MEMBER slideshowInterval FINAL)
    Q_PROPERTY(SettingsRange expandLimit MEMBER expandLimit FINAL)
    Q_PROPERTY(SettingsRange zoomStep MEMBER zoomStep FINAL)
    Q_PROPERTY(SettingsRange casSharpening MEMBER casSharpening FINAL)
    Q_PROPERTY(SettingsRange casContrast MEMBER casContrast FINAL)
    Q_PROPERTY(SettingsRange opacity MEMBER opacity FINAL)
    Q_PROPERTY(SettingsRange mouseScrollingSpeed MEMBER mouseScrollingSpeed FINAL)
    Q_PROPERTY(SettingsRange thumbnailerThreads MEMBER thumbnailerThreads FINAL)
    Q_PROPERTY(SettingsRange thumbnailResolution MEMBER thumbnailResolution FINAL)
    Q_PROPERTY(SettingsRange thumbnailCacheSize MEMBER thumbnailCacheSize FINAL)
    Q_PROPERTY(SettingsRange quality MEMBER quality FINAL)
    Q_PROPERTY(SettingsRange pngCompression MEMBER pngCompression FINAL)
    Q_PROPERTY(SettingsRange memoryLimit MEMBER memoryLimit FINAL)
    Q_PROPERTY(SettingsRange upscaylLimit MEMBER upscaylLimit FINAL)

public:
    SettingsRange autoResizeLimit;
    SettingsRange panelHideDelay;
    SettingsRange panelSize;
    SettingsRange slideshowInterval;
    SettingsRange expandLimit;
    SettingsRange zoomStep;
    SettingsRange casSharpening;
    SettingsRange casContrast;
    SettingsRange opacity;
    SettingsRange mouseScrollingSpeed;
    SettingsRange thumbnailerThreads;
    SettingsRange thumbnailResolution;
    SettingsRange thumbnailCacheSize;
    SettingsRange quality;
    SettingsRange pngCompression;
    SettingsRange memoryLimit;
    SettingsRange upscaylLimit;

    friend bool operator==(const SettingsRanges &, const SettingsRanges &) = default;
};

namespace SettingsScales {

// The ranges of the widget dialog. Settings checks that the thumbnailer and
// panel hide delay ranges match its own limits.
inline constexpr int kAutoResizeLimitMinStep = 6;
inline constexpr int kAutoResizeLimitMaxStep = 20;
inline constexpr int kAutoResizeLimitPageStep = 2;
inline constexpr int kAutoResizeLimitPercentPerStep = 5;

inline constexpr int kMinPanelHideDelayMs = 0;
inline constexpr int kMaxPanelHideDelayMs = 2'000;
inline constexpr int kPanelHideDelayStepMs = 100;

inline constexpr int kPanelSizeMinStep = 13;
inline constexpr int kPanelSizeMaxStep = 32;
inline constexpr int kPanelSizePageStep = 5;
inline constexpr int kPanelSizePixelsPerStep = 8;

inline constexpr int kMinSlideshowIntervalMs = 500;
inline constexpr int kMaxSlideshowIntervalMs = 65'535;

inline constexpr int kMaxExpandLimit = 4;

inline constexpr int kMinZoomStepPercent = 1;
inline constexpr int kMaxZoomStepPercent = 50;
inline constexpr int kZoomStepPageStep = 10;

inline constexpr int kMinCasSharpeningPercent = 1;
inline constexpr int kMaxPercent = 100;

inline constexpr int kMaxMouseScrollingSpeedStep = 6;

inline constexpr int kMinThumbnailerThreads = 1;
inline constexpr int kMaxThumbnailerThreads = 32;

inline constexpr int kMinThumbnailResolution = 128;
inline constexpr int kMaxThumbnailResolution = 512;
inline constexpr int kThumbnailResolutionStep = 16;

inline constexpr int kMaxThumbnailCacheSizeMB = 102'400;
inline constexpr int kThumbnailCacheSizeStepMB = 128;

inline constexpr int kQualityPageStep = 5;
inline constexpr int kMaxPngCompression = 9;

inline constexpr int kMinMemoryLimitMB = 512;
inline constexpr int kMaxMemoryLimitMB = 8'192;
inline constexpr int kMemoryLimitStepMB = 512;

inline constexpr int kMinUpscaylLimitPercent = 100;
inline constexpr int kMaxUpscaylLimitPercent = 400;
inline constexpr int kUpscaylLimitStepPercent = 5;
inline constexpr int kUpscaylLimitPageStep = 25;

[[nodiscard]] SettingsRanges ranges();

// value moved to the nearest multiple of step (halves round up), as the
// widget dialog's snapping sliders do.
[[nodiscard]] int snapped(int value, int step);

// Auto resize limit: steps of 5 % of the screen area.
[[nodiscard]] int autoResizeLimitStep(int percent);
[[nodiscard]] int autoResizeLimitPercent(int step);
// Thumbnail panel preview size: steps of 8 px.
[[nodiscard]] int panelSizeStep(int pixels);
[[nodiscard]] int panelSizePixels(int step);
// Zoom step: percent of the zoom factor.
[[nodiscard]] int zoomStepPercent(float zoomStep);
[[nodiscard]] float zoomStep(int percent);
// Mouse scrolling speed: 0.5x plus steps of 0.25x.
[[nodiscard]] int mouseScrollingSpeedStep(float speed);
[[nodiscard]] float mouseScrollingSpeed(int step);
// Window and thumbnail bar opacity in percent.
[[nodiscard]] int opacityPercent(qreal opacity);
[[nodiscard]] qreal opacity(int percent);
// FidelityFX-CAS sharpness and contrast in percent.
[[nodiscard]] int casPercent(float value);
[[nodiscard]] float casValue(int percent);

// Texts next to the sliders.
[[nodiscard]] QString autoResizeLimitText(int step);
[[nodiscard]] QString panelHideDelayText(int ms);
[[nodiscard]] QString expandLimitText(int limit);
[[nodiscard]] QString zoomStepText(int percent);
[[nodiscard]] QString casValueText(int percent);
[[nodiscard]] QString percentText(int percent);
[[nodiscard]] QString mouseScrollingSpeedText(int step);
[[nodiscard]] QString thumbnailResolutionText(int pixels);
[[nodiscard]] QString pngCompressionText(int level);

} // namespace SettingsScales
