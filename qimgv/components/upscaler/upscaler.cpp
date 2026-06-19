#include "upscaler.h"
#include "upscalerrunnable.h"
#include "settings.h"
#include <QFile>
#include <QMutexLocker>
#include <QPainter>
#include <QThreadPool>
#include <QCoreApplication>
#include <QDebug>

#ifdef USE_UPSCAYL

UpscaylScaler::UpscaylScaler() : realesrgan(nullptr) {}

bool UpscaylScaler::init(const QString &appDir, const QString &requestedModelName) {
    QMutexLocker locker(&mutex);
    QString modelName = requestedModelName.isEmpty() ? settings->upscaylModel() : requestedModelName;
    if (realesrgan && loadedModel == modelName) {
        return true;
    }
    if (realesrgan) {
        realesrgan.reset();
        loadedModel = "";
    }

    realesrgan = std::make_unique<RealESRGAN>(-1, false);
    realesrgan->scale = 4;
    realesrgan->prepadding = 10;

    {
        int autoTile = realesrgan->autoTilesize();
        realesrgan->tilesize = autoTile;
    }

    QString paramQStr = appDir + "/models/" + modelName + ".param";
    QString binQStr = appDir + "/models/" + modelName + ".bin";

    int res = realesrgan->load(paramQStr.toStdWString(), binQStr.toStdWString());
    if (res != 0) {
        qWarning() << "[Upscayl] Failed to load model, error code:" << res;
        realesrgan.reset();
        loadedModel = "";
        return false;
    }
    loadedModel = modelName;
    return true;
}

QImage UpscaylScaler::upscale(const QImage &inputImage, const std::atomic<bool> *abortFlag) {
    QMutexLocker locker(&mutex);
    if (!realesrgan) {
        qWarning() << "[Upscayl] upscale() called but realesrgan is null";
        return QImage();
    }

    if (abortFlag && abortFlag->load(std::memory_order_relaxed)) {
        return QImage();
    }

    QImage imgRgba = inputImage;
    if (imgRgba.format() != QImage::Format_ARGB32 && imgRgba.format() != QImage::Format_RGB32) {
        imgRgba = imgRgba.convertToFormat(QImage::Format_ARGB32);
    }
    qint64 inW = imgRgba.width();
    qint64 inH = imgRgba.height();

    constexpr qint64 kMaxUpscalePixels = 64LL * 1024 * 1024;
    if (inW <= 0 || inH <= 0 || (inW * inH) > kMaxUpscalePixels) {
        qWarning() << "[Upscayl] upscale() image too large or invalid size:" << inW << "x" << inH;
        return QImage();
    }

    int scale = realesrgan->scale;
    if (scale <= 0) {
        scale = 4;
    }

    qint64 outW = inW * scale;
    qint64 outH = inH * scale;

    constexpr qint64 kMaxIntVal = 2147483647; // std::numeric_limits<int>::max()
    if (outW > kMaxIntVal || outH > kMaxIntVal) {
        qWarning() << "[Upscayl] upscale() output size exceeds max integer:" << outW << "x" << outH;
        return QImage();
    }

    QImage outImg(static_cast<int>(outW), static_cast<int>(outH), QImage::Format_ARGB32);
    if (outImg.isNull()) {
        qWarning() << "[Upscayl] upscale() failed to allocate output QImage of size:" << outW << "x" << outH;
        return QImage();
    }

    int ret = realesrgan->processPixels(imgRgba.constBits(), static_cast<int>(inW), static_cast<int>(inH),
                                        outImg.bits(), static_cast<int>(outW), static_cast<int>(outH),
                                        abortFlag);

    if (ret != 0) {
        qWarning() << "[Upscayl] processPixels failed or aborted, code:" << ret;
        return QImage();
    }

    return outImg;
}

void UpscaylScaler::destroy() {
    QMutexLocker locker(&mutex);
    if (realesrgan) {
        realesrgan.reset();
        loadedModel = "";
    }
}

class UpscaylPreloadTask : public QRunnable {
public:
    void run() override {
        QString appDir = QCoreApplication::applicationDirPath();
        if (UpscaylScaler::getInstance()->init(appDir)) {
            QImage dummy(512, 512, QImage::Format_ARGB32);
            dummy.fill(Qt::black);
            QImage warmed = UpscaylScaler::getInstance()->upscale(dummy);
        }
    }
};

class UpscaylDestroyTask : public QRunnable {
public:
    void run() override {
        UpscaylScaler::getInstance()->destroy();
    }
};

#endif // USE_UPSCAYL

Upscaler::Upscaler(QObject *parent) : QObject(parent) {
#ifdef USE_UPSCAYL
    pool = new QThreadPool(this);
    pool->setMaxThreadCount(1);
    upscaylTimer.setSingleShot(true);
    upscaylTimer.setInterval(kDebounceIntervalMs);
    connect(&upscaylTimer, &QTimer::timeout, this, &Upscaler::onUpscaylTimerTimeout);
#endif
}

Upscaler::~Upscaler() {
#ifdef USE_UPSCAYL
    if (currentAbortFlag) {
        currentAbortFlag->store(true, std::memory_order_relaxed);
    }
    if (pool) {
        pool->waitForDone();
    }
    UpscaylScaler::getInstance()->destroy();
#endif
}

void Upscaler::requestUpscale(std::shared_ptr<Image> image, QSize targetSize, QString path) {
#ifdef USE_UPSCAYL
    currentGeneration.fetch_add(1, std::memory_order_relaxed);
    if (currentAbortFlag) {
        currentAbortFlag->store(true, std::memory_order_relaxed);
    }
    currentAbortFlag = std::make_shared<std::atomic<bool>>(false);

    QMutexLocker locker(&stateMutex);
    pendingUpscaylImage = image;
    pendingUpscaylSize = targetSize;
    pendingUpscaylPath = path;
    upscaylTimer.stop();
    upscaylTimer.start();
#else
    Q_UNUSED(image);
    Q_UNUSED(targetSize);
    Q_UNUSED(path);
#endif
}

void Upscaler::readSettings() {
#ifdef USE_UPSCAYL
    if (!settings->useUpscayl() || !settings->preloadUpscayl()) {
        QThreadPool::globalInstance()->start(new UpscaylDestroyTask());
    } else {
        QThreadPool::globalInstance()->start(new UpscaylPreloadTask());
    }
#endif
}

void Upscaler::reset() {
#ifdef USE_UPSCAYL
    currentGeneration.fetch_add(1, std::memory_order_relaxed);
    if (currentAbortFlag) {
        currentAbortFlag->store(true, std::memory_order_relaxed);
    }
    currentAbortFlag.reset();

    QMutexLocker locker(&stateMutex);
    upscaylTimer.stop();
    pendingUpscaylImage.reset();
    pendingUpscaylPath = "";
    upscaylActive = false;
    upscaylPendingRun = false;
    emit upscaleAborted();
    if (!settings->preloadUpscayl()) {
        QThreadPool::globalInstance()->start(new UpscaylDestroyTask());
    }
#endif
}

bool Upscaler::isRequestStale(uint64_t taskGeneration) const {
#ifdef USE_UPSCAYL
    return taskGeneration != currentGeneration.load(std::memory_order_relaxed);
#else
    Q_UNUSED(taskGeneration);
    return true;
#endif
}

void Upscaler::onUpscaylTimerTimeout() {
#ifdef USE_UPSCAYL
    bool ok = false;
    QRect visibleRect;
    double currentScale = 1.0;
    double dpr = 1.0;

    emit requestUpscaleParams(pendingUpscaylPath, &ok, &visibleRect, &currentScale, &dpr);

    if (!ok) {
        emit upscaleAborted();
        return;
    }

    QMutexLocker locker(&stateMutex);
    if (pendingUpscaylImage) {
        if (upscaylActive) {
            upscaylPendingRun = true;
        } else {
            triggerUpscaylProcessing(visibleRect, currentScale, dpr);
        }
    }
#endif
}

void Upscaler::triggerUpscaylProcessing(QRect visibleRect, double currentScale, double dpr) {
#ifdef USE_UPSCAYL
    if (!pendingUpscaylImage)
        return;

    upscaylActive = true;
    upscaylPendingRun = false;

    QRect origCrop = visibleRect;

    // Align width and height of origCrop to multiples of 2
    int alignedW = (origCrop.width() / 2) * 2;
    int alignedH = (origCrop.height() / 2) * 2;
    if (alignedW < 2) alignedW = 2;
    if (alignedH < 2) alignedH = 2;
    origCrop.setWidth(alignedW);
    origCrop.setHeight(alignedH);

    if (origCrop.isEmpty()) {
        upscaylActive = false;
        return;
    }

    std::shared_ptr<const QImage> origQImage = pendingUpscaylImage->getImage();
    if (!origQImage || origQImage->isNull()) {
        upscaylActive = false;
        return;
    }

    QImage croppedImg = origQImage->copy(origCrop);
    if (croppedImg.isNull()) {
        upscaylActive = false;
        return;
    }

    // Cap the crop resolution to prevent Vulkan/GPU OOM
    if (croppedImg.width() > kMaxCropDimension || croppedImg.height() > kMaxCropDimension) {
        double scaleFactor = (std::min)((double)kMaxCropDimension / origCrop.width(), (double)kMaxCropDimension / origCrop.height());
        croppedImg = croppedImg.scaled(kMaxCropDimension, kMaxCropDimension, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        int w = (croppedImg.width() / 2) * 2;
        int h = (croppedImg.height() / 2) * 2;
        if (w < 2) w = 2;
        if (h < 2) h = 2;
        if (w != croppedImg.width() || h != croppedImg.height()) {
            croppedImg = croppedImg.copy(0, 0, w, h);
        }
        origCrop.setWidth(qRound(w / scaleFactor));
        origCrop.setHeight(qRound(h / scaleFactor));
    }

    latestUpscaylSize = pendingUpscaylSize;

    double scaleX = dpr * currentScale;
    double scaleY = dpr * currentScale;

    QSize cropTargetSize(qRound(origCrop.width() * scaleX),
                         qRound(origCrop.height() * scaleY));

    emit upscaleStarted();

    UpscalerTaskParams params;
    params.croppedImage = croppedImg;
    params.origCrop = origCrop;
    params.path = pendingUpscaylPath;
    params.targetSize = pendingUpscaylSize;
    params.generation = currentGeneration.load(std::memory_order_relaxed);

    UpscalerRunnable *task = new UpscalerRunnable(this, params, currentAbortFlag);
    if (pool) {
        pool->start(task);
    } else {
        delete task;
        upscaylActive = false;
    }
#else
    Q_UNUSED(visibleRect);
    Q_UNUSED(currentScale);
    Q_UNUSED(dpr);
#endif
}

void Upscaler::onTaskFinished(QImage cropImg, QRect origCrop, QString path, QSize targetSize, uint64_t taskGeneration) {
#ifdef USE_UPSCAYL
    upscaylActive = false;

    if (cropImg.isNull()) {
        if (upscaylPendingRun && pendingUpscaylImage) {
            upscaylPendingRun = false;
            onUpscaylTimerTimeout();
        }
        return;
    }

    if (taskGeneration == currentGeneration.load(std::memory_order_relaxed)) {
        emit upscaleFinished(cropImg, origCrop, path, targetSize);
    }

    if (upscaylPendingRun && pendingUpscaylImage) {
        upscaylPendingRun = false;
        onUpscaylTimerTimeout();
    }
#else
    Q_UNUSED(cropImg);
    Q_UNUSED(origCrop);
    Q_UNUSED(path);
    Q_UNUSED(targetSize);
    Q_UNUSED(taskGeneration);
#endif
}

void Upscaler::onTaskAborted(QString path, QSize targetSize, uint64_t taskGeneration) {
#ifdef USE_UPSCAYL
    Q_UNUSED(path);
    Q_UNUSED(targetSize);
    Q_UNUSED(taskGeneration);
    upscaylActive = false;
    emit upscaleAborted();

    if (upscaylPendingRun && pendingUpscaylImage) {
        upscaylPendingRun = false;
        onUpscaylTimerTimeout();
    }
#else
    Q_UNUSED(path);
    Q_UNUSED(targetSize);
    Q_UNUSED(taskGeneration);
#endif
}
