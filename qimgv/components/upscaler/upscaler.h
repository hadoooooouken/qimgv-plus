#pragma once

#include <QObject>
#include <QImage>
#include <QRect>
#include <QString>
#include <QSize>
#include <QTimer>
#include <QMutex>
#include <memory>
#include <atomic>
#include <QThreadPool>
#include "sourcecontainers/image.h"
#include "realesrgan.h"

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

    static qint64 getMaxOutputPixelsBudget();

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

    // Constant parameters to avoid magic numbers
    static constexpr int kDebounceIntervalMs = 100;
    static constexpr int kMaxCropDimension = 1280;
    static constexpr int kDummyTileSize = 512;
    static constexpr qint64 kMaxUpscalePixels = 64LL * 1024 * 1024;
    static constexpr int kDefaultScale = 4;
    static constexpr int kPrePadding = 10;

    QTimer upscaylTimer;
    bool upscaylActive = false;
    bool upscaylPendingRun = false;

    // Cached copy of the settings that actually affect preload/init, so
    // readSettings() can ignore notifications that don't change any of them.
    bool preloadSettingsBaselineValid = false;
    bool lastUseUpscayl = false;
    bool lastPreloadUpscayl = false;
    QString lastUpscaylModel;

    // Guards against queueing more than one preload/destroy task at a time.
    // Held via shared_ptr so the background task can safely clear it even if
    // this Upscaler is destroyed before the task (queued on the global pool)
    // finishes.
    std::shared_ptr<std::atomic<bool>> preloadTaskBusy;

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
