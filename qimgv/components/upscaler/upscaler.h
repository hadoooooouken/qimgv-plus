#pragma once

#include <QObject>
#include <QImage>
#include <QRect>
#include <QString>
#include <QSize>
#include <QTimer>
#include <QMutex>
#include <cstdint>
#include <memory>
#include <optional>
#include <atomic>
#include <QThreadPool>
#include "sourcecontainers/image.h"
#include "realesrgan.h"

class UpscalerPreloadState;

struct UpscaylInferenceRequest final {
    QString appDir;
    QString modelName;
    QImage inputImage;
    const std::atomic<bool> *abortFlag = nullptr;
};

enum class UpscaylInferenceError {
    None,
    Aborted,
    ModelLoadFailed,
    ProcessingFailed,
    // The Vulkan device was lost (e.g. after a Windows TDR driver reset)
    // and a single rebuild-and-retry recovery cycle also failed. Distinct
    // from ProcessingFailed for logging/diagnostics; existing callers that
    // only check succeeded() are unaffected.
    DeviceLost
};

struct UpscaylInferenceResult final {
    QImage image;
    UpscaylInferenceError error = UpscaylInferenceError::None;

    [[nodiscard]] bool succeeded() const noexcept {
        return error == UpscaylInferenceError::None && !image.isNull();
    }
};

class UpscaylScaler {
public:
    static UpscaylScaler *getInstance() {
        static UpscaylScaler instance;
        return &instance;
    }

    [[nodiscard]] UpscaylInferenceResult upscale(const UpscaylInferenceRequest &request);
    void destroy();

    ~UpscaylScaler() = default;

private:
    UpscaylScaler();

    // Outcome of a single upscaleLocked() attempt. rawErrorCode mirrors
    // RealESRGAN::process()'s return code when processPixels() was reached
    // and failed; it stays 0 (no code available) for earlier-stage failures
    // such as invalid input or budget checks, so it never gets misread as
    // a device-loss signal.
    struct ProcessingOutcome final {
        QImage image;
        int rawErrorCode = 0;
    };

    // forceReinit, when true, rebuilds `realesrgan` unconditionally even if
    // requestedModelName matches the already-loaded model. Used to recover
    // after a Vulkan device-loss error, since there is no partial "reset
    // just the device" API and a full object rebuild is the only way to
    // get a live Vulkan device/pipeline again.
    bool initLocked(const QString &appDir, const QString &requestedModelName, bool forceReinit = false);
    ProcessingOutcome upscaleLocked(const QImage &inputImage, const std::atomic<bool> *abortFlag);

    std::unique_ptr<RealESRGAN> realesrgan;
    QString loadedModel;
    QMutex mutex;
};

class Upscaler : public QObject {
    Q_OBJECT
public:
    explicit Upscaler(QObject *parent = nullptr);
    ~Upscaler();

    void requestUpscale(std::shared_ptr<Image> image, QSize targetSize, QString path);
    [[nodiscard]] bool readSettings();
    void invalidatePreview();
    void reset();
    bool isRequestStale(uint64_t taskGeneration) const;

signals:
    void upscaleStarted();
    void upscaleFinished(QImage cropImg, QRect origCrop, QString path, QSize targetSize);
    void upscaleAborted();
    void upscaleFailed(const QString &error);
    void previewInvalidated();
    void requestUpscaleParams(const QString &path, bool *ok, QRect *visibleRect, double *currentScale, double *dpr);

private slots:
    void onUpscaylTimerTimeout();
    void onTaskFinished(QImage cropImg, QRect origCrop, QString path, QSize targetSize, uint64_t taskGeneration);
    void onTaskAborted(QString path, QSize targetSize, uint64_t taskGeneration);
    void onTaskFailed(const QString &error, uint64_t taskGeneration);

private:
    void abortPreviewRequest();
    void triggerUpscaylProcessing(QRect visibleRect, double currentScale, double dpr);
    void reconcilePreloadState(bool forceReapply);

    // Constant parameters to avoid magic numbers
    static constexpr int kDebounceIntervalMs = 100;
    static constexpr int kMaxCropDimension = 1280;

    QTimer upscaylTimer;
    // Invalidation requests cancellation but the worker remains active until
    // its matching completion callback arrives.
    std::optional<uint64_t> activeTaskGeneration;
    bool upscaylPendingRun = false;

    // Shared with the global-pool worker so it can converge on the latest
    // immutable desired configuration even if this component is destroyed.
    std::shared_ptr<UpscalerPreloadState> preloadState;

    // pending request state
    std::shared_ptr<Image> pendingUpscaylImage;
    QSize pendingUpscaylSize;
    QString pendingUpscaylPath;

    QSize latestUpscaylSize;
    mutable QMutex stateMutex;

    std::atomic<uint64_t> currentGeneration{0};
    std::shared_ptr<std::atomic<bool>> currentAbortFlag;
    QThreadPool *pool = nullptr;
    QString configuredModel;
};
