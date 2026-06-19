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

#ifdef USE_UPSCAYL
#include "realesrgan.h"

class UpscaylScaler {
public:
    static UpscaylScaler *getInstance() {
        static UpscaylScaler instance;
        return &instance;
    }

    bool init(const QString &appDir, const QString &modelName = QString());
    QImage upscale(const QImage &inputImage, const std::atomic<bool> *abortFlag = nullptr);
    void destroy();

    ~UpscaylScaler() = default;

private:
    UpscaylScaler();
    std::unique_ptr<RealESRGAN> realesrgan;
    QString loadedModel;
    QMutex mutex;
};
#endif

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
