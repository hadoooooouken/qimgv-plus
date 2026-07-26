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
#include <atomic>
#include <QThreadPool>
#include "sourcecontainers/image.h"
#include "realesrgan.h"

class UpscalerPreloadState;

class UpscaylScaler {
public:
    static UpscaylScaler *getInstance() {
        static UpscaylScaler instance;
        return &instance;
    }

    // didLoad (if provided) is set to true only when this call actually loaded
    // a model (i.e. it wasn't already loaded), so callers can skip redundant
    // warm-up work when the model was already resident.
    bool init(const QString &appDir, const QString &modelName = QString(), bool *didLoad = nullptr);
    QImage upscale(const QImage &inputImage, const std::atomic<bool> *abortFlag = nullptr);
    void destroy();

    ~UpscaylScaler() = default;

private:
    UpscaylScaler();
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
    void readSettings();
    void reset();
    bool isRequestStale(uint64_t taskGeneration) const;

signals:
    void upscaleStarted();
    void upscaleFinished(QImage cropImg, QRect origCrop, QString path, QSize targetSize);
    void upscaleAborted();
    void requestUpscaleParams(const QString &path, bool *ok, QRect *visibleRect, double *currentScale, double *dpr);

private slots:
    void onUpscaylTimerTimeout();
    void onTaskFinished(QImage cropImg, QRect origCrop, QString path, QSize targetSize, uint64_t taskGeneration);
    void onTaskAborted(QString path, QSize targetSize, uint64_t taskGeneration);

private:
    void triggerUpscaylProcessing(QRect visibleRect, double currentScale, double dpr);
    void reconcilePreloadState(bool forceReapply);

    // Constant parameters to avoid magic numbers
    static constexpr int kDebounceIntervalMs = 100;
    static constexpr int kMaxCropDimension = 1280;

    QTimer upscaylTimer;
    bool upscaylActive = false;
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
};
