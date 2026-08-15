#include "upscaler.h"
#include "upscalerrunnable.h"
#include "settings.h"
#include <QColorSpace>
#include <QDir>
#include <QFileInfo>
#include <QMutexLocker>
#include <QPainter>
#include <QThreadPool>
#include <QCoreApplication>
#include <QDebug>
#include <algorithm>
#include <initializer_list>
#include <limits>
#include <optional>
#include <windows.h>

namespace {

constexpr quint64 kBytesPerMebibyte = 1024ULL * 1024ULL;
constexpr quint64 kMinimumCpuReserveBytes = 512ULL * kBytesPerMebibyte;
constexpr quint64 kMinimumDeviceReserveBytes = 256ULL * kBytesPerMebibyte;
constexpr quint64 kRgbaBytesPerPixel = 4;
constexpr quint64 kCpuReserveDivisor = 4;
constexpr quint64 kDeviceReserveDivisor = 5;
constexpr quint64 kFallbackDeviceReserveDivisor = 2;
constexpr quint64 kMaximumReserveDivisor = 2;
// Fallback only: used when RealESRGAN::detectedScale() can't derive the
// scale from the loaded model's own ncnn graph (see initLocked()).
constexpr int kModelScale = 4;
constexpr int kModelPrepadding = 10;

struct CpuMemoryBudget final {
    quint64 availablePhysicalBytes = 0;
    quint64 availableCommitBytes = 0;
    quint64 usableBytes = 0;
};

struct DeviceMemoryBudget final {
    quint64 budgetBytes = 0;
    quint64 usageBytes = 0;
    quint64 usableBytes = 0;
    bool usageKnown = false;
    bool sharesSystemMemory = false;
};

struct UpscaleMemoryEstimate final {
    quint64 conversionBytes = 0;
    quint64 outputImageBytes = 0;
    quint64 backendCpuBytes = 0;
    quint64 totalCpuBytes = 0;
    quint64 deviceBytes = 0;
};

bool isAbortRequested(const std::atomic<bool> *abortFlag) noexcept {
    return abortFlag && abortFlag->load(std::memory_order_relaxed);
}

bool hasPackedRgbaStorage(const QImage &image) {
    const bool compatibleFormat =
        image.format() == QImage::Format_ARGB32 ||
        image.format() == QImage::Format_RGB32;
    const qsizetype packedBytesPerLine =
        static_cast<qsizetype>(image.width()) *
        static_cast<qsizetype>(kRgbaBytesPerPixel);
    return compatibleFormat && image.bytesPerLine() == packedBytesPerLine;
}

std::optional<quint64>
checkedProduct(std::initializer_list<quint64> factors) {
    quint64 product = 1;
    for (const quint64 factor : factors) {
        if (factor != 0 &&
            product > (std::numeric_limits<quint64>::max)() / factor) {
            return std::nullopt;
        }
        product *= factor;
    }
    return product;
}

std::optional<quint64>
checkedSum(std::initializer_list<quint64> terms) {
    quint64 sum = 0;
    for (const quint64 term : terms) {
        if (sum > (std::numeric_limits<quint64>::max)() - term) {
            return std::nullopt;
        }
        sum += term;
    }
    return sum;
}

quint64 calculateUsableBudget(quint64 availableBytes,
                              quint64 minimumReserveBytes,
                              quint64 reserveDivisor) {
    if (availableBytes == 0 || reserveDivisor == 0) {
        return 0;
    }

    const quint64 desiredReserve =
        (std::max)(minimumReserveBytes, availableBytes / reserveDivisor);
    const quint64 reserve =
        (std::min)(desiredReserve,
                   availableBytes / kMaximumReserveDivisor);
    return availableBytes - reserve;
}

std::optional<CpuMemoryBudget> queryCpuMemoryBudget() {
    MEMORYSTATUSEX memoryStatus = {};
    memoryStatus.dwLength =
        static_cast<DWORD>(sizeof(MEMORYSTATUSEX));
    if (!GlobalMemoryStatusEx(&memoryStatus)) {
        return std::nullopt;
    }

    CpuMemoryBudget budget;
    budget.availablePhysicalBytes = memoryStatus.ullAvailPhys;
    budget.availableCommitBytes = memoryStatus.ullAvailPageFile;
    const quint64 availableBytes =
        (std::min)(budget.availablePhysicalBytes,
                   budget.availableCommitBytes);
    budget.usableBytes =
        calculateUsableBudget(availableBytes, kMinimumCpuReserveBytes,
                              kCpuReserveDivisor);
    return budget;
}

std::optional<DeviceMemoryBudget>
makeDeviceMemoryBudget(const RealESRGAN::DeviceMemorySnapshot &snapshot) {
    if (!snapshot.valid || snapshot.budgetBytes == 0) {
        return std::nullopt;
    }

    DeviceMemoryBudget budget;
    budget.budgetBytes = snapshot.budgetBytes;
    budget.usageBytes = snapshot.usageBytes;
    budget.usageKnown = snapshot.usageKnown;
    budget.sharesSystemMemory = snapshot.sharesSystemMemory;

    const quint64 availableBytes =
        snapshot.usageKnown
            ? snapshot.usageBytes < snapshot.budgetBytes
                  ? snapshot.budgetBytes - snapshot.usageBytes
                  : 0
            : snapshot.budgetBytes;
    const quint64 reserveDivisor =
        snapshot.usageKnown ? kDeviceReserveDivisor
                            : kFallbackDeviceReserveDivisor;
    budget.usableBytes =
        calculateUsableBudget(availableBytes, kMinimumDeviceReserveBytes,
                              reserveDivisor);
    return budget;
}

std::optional<UpscaleMemoryEstimate>
estimateUpscaleMemory(const QImage &inputImage, quint64 outputWidth,
                      quint64 outputHeight, const RealESRGAN &backend) {
    const quint64 inputWidth = static_cast<quint64>(inputImage.width());
    const quint64 inputHeight = static_cast<quint64>(inputImage.height());
    const bool inputBufferRequired = !hasPackedRgbaStorage(inputImage);

    const auto inputImageBytes =
        checkedProduct({inputWidth, inputHeight, kRgbaBytesPerPixel});
    const auto outputImageBytes =
        checkedProduct({outputWidth, outputHeight, kRgbaBytesPerPixel});
    if (!inputImageBytes || !outputImageBytes) {
        return std::nullopt;
    }
    constexpr quint64 kMaximumQImageByteCount =
        static_cast<quint64>((std::numeric_limits<qsizetype>::max)());
    if (*outputImageBytes > kMaximumQImageByteCount) {
        return std::nullopt;
    }

    const RealESRGAN::ResourceEstimate backendEstimate =
        backend.estimateResources(
            RealESRGAN::ResourceRequest{inputImage.width(),
                                        inputImage.height()});
    if (!backendEstimate.valid) {
        return std::nullopt;
    }

    const quint64 conversionBytes =
        inputBufferRequired ? *inputImageBytes : 0;
    const auto totalCpuBytes =
        checkedSum({conversionBytes, *outputImageBytes,
                    backendEstimate.cpuWorkingBytes});
    if (!totalCpuBytes) {
        return std::nullopt;
    }

    UpscaleMemoryEstimate estimate;
    estimate.conversionBytes = conversionBytes;
    estimate.outputImageBytes = *outputImageBytes;
    estimate.backendCpuBytes = backendEstimate.cpuWorkingBytes;
    estimate.totalCpuBytes = *totalCpuBytes;
    estimate.deviceBytes = backendEstimate.deviceWorkingBytes;
    return estimate;
}

} // namespace


UpscaylScaler::UpscaylScaler() : realesrgan(nullptr) {}

bool UpscaylScaler::initLocked(const QString &appDir, const QString &requestedModelName, bool forceReinit) {
    const QString modelName = requestedModelName.trimmed();
    if (!forceReinit && realesrgan && loadedModel == modelName) {
        return true;
    }

    const QDir modelsDir(appDir + "/models");
    const QFileInfo paramFile(modelsDir.filePath(modelName + ".param"));
    const QFileInfo binFile(modelsDir.filePath(modelName + ".bin"));
    if (modelName.isEmpty() || !paramFile.isFile() || !paramFile.isReadable() ||
        !binFile.isFile() || !binFile.isReadable()) {
        qWarning() << "[Upscayl] Model files are missing or unreadable:"
                   << paramFile.absoluteFilePath() << binFile.absoluteFilePath();
        return false;
    }

    if (realesrgan) {
        realesrgan.reset();
        loadedModel = "";
    }

    realesrgan = std::make_unique<RealESRGAN>(-1, false);
    realesrgan->prepadding = kModelPrepadding;

    const int res = realesrgan->load(paramFile.absoluteFilePath().toStdWString(),
                                     binFile.absoluteFilePath().toStdWString());
    if (res != 0) {
        qWarning() << "[Upscayl] Failed to load model, error code:" << res;
        realesrgan.reset();
        loadedModel = "";
        return false;
    }

    realesrgan->scale = realesrgan->detectedScale().value_or(kModelScale);
    if (!realesrgan->detectedScale().has_value()) {
        qWarning() << "[Upscayl] Could not detect model scale from graph, falling back to"
                   << kModelScale << "for model:" << modelName;
    }

    {
        int autoTile = realesrgan->autoTilesize();
        realesrgan->tilesize = autoTile;
    }

    loadedModel = modelName;
    return true;
}

UpscaylInferenceResult UpscaylScaler::upscale(const UpscaylInferenceRequest &request) {
    QMutexLocker locker(&mutex);
    if (isAbortRequested(request.abortFlag)) {
        return {QImage(), UpscaylInferenceError::Aborted};
    }

    if (!initLocked(request.appDir, request.modelName)) {
        return {QImage(), UpscaylInferenceError::ModelLoadFailed};
    }

    ProcessingOutcome outcome = upscaleLocked(request.inputImage, request.abortFlag);

    // A dead Vulkan device (e.g. after a Windows TDR driver reset) fails
    // every subsequent attempt identically until something rebuilds
    // realesrgan; nothing previously did. Detect that specific failure
    // class and, once, force a full rebuild and retry. There is no partial
    // "reset just the device" API, so this reuses the same
    // destroy-and-reconstruct path initLocked() already takes on a model
    // change. No retry loop or backoff timer: if the driver has not
    // finished recovering yet, this attempt fails normally and the next
    // user-triggered upscale calls initLocked() fresh and self-heals
    // whenever the driver is actually ready.
    if (outcome.image.isNull() &&
        !isAbortRequested(request.abortFlag) &&
        RealESRGAN::isDeviceLossError(outcome.rawErrorCode)) {
        qWarning() << "[Upscayl] Vulkan device loss detected (error code:"
                   << outcome.rawErrorCode << "), attempting one recovery cycle";

        if (initLocked(request.appDir, request.modelName, /*forceReinit=*/true)) {
            outcome = upscaleLocked(request.inputImage, request.abortFlag);
        } else {
            qWarning() << "[Upscayl] Device-loss recovery failed to reinitialize the model";
        }
    }

    if (outcome.image.isNull()) {
        if (isAbortRequested(request.abortFlag)) {
            return {QImage(), UpscaylInferenceError::Aborted};
        }
        const UpscaylInferenceError error =
            RealESRGAN::isDeviceLossError(outcome.rawErrorCode)
                ? UpscaylInferenceError::DeviceLost
                : UpscaylInferenceError::ProcessingFailed;
        return {QImage(), error};
    }

    return {outcome.image, UpscaylInferenceError::None};
}

UpscaylScaler::ProcessingOutcome UpscaylScaler::upscaleLocked(const QImage &inputImage, const std::atomic<bool> *abortFlag) {
    if (!realesrgan) {
        qWarning() << "[Upscayl] upscale() called but realesrgan is null";
        return {};
    }

    if (isAbortRequested(abortFlag)) {
        return {};
    }

    const int inW = inputImage.width();
    const int inH = inputImage.height();
    if (inW <= 0 || inH <= 0) {
        qWarning() << "[Upscayl] upscale() image invalid size:" << inW << "x" << inH;
        return {};
    }

    const int scale = realesrgan->scale;
    if (scale <= 0) {
        qWarning() << "[Upscayl] upscale() invalid model scale:" << scale;
        return {};
    }

    const auto outW =
        checkedProduct({static_cast<quint64>(inW),
                        static_cast<quint64>(scale)});
    const auto outH =
        checkedProduct({static_cast<quint64>(inH),
                        static_cast<quint64>(scale)});
    constexpr quint64 kMaximumImageDimension =
        static_cast<quint64>((std::numeric_limits<int>::max)());
    if (!outW || !outH || *outW > kMaximumImageDimension ||
        *outH > kMaximumImageDimension) {
        qWarning() << "[Upscayl] upscale() output dimensions overflow:"
                   << inW << "x" << inH << "scale:" << scale;
        return {};
    }

    const auto memoryEstimate =
        estimateUpscaleMemory(inputImage, *outW, *outH, *realesrgan);
    if (!memoryEstimate) {
        qWarning() << "[Upscayl] upscale() resource estimate overflowed or is unavailable for:"
                   << inW << "x" << inH;
        return {};
    }

    const auto cpuBudget = queryCpuMemoryBudget();
    if (!cpuBudget) {
        qWarning() << "[Upscayl] upscale() could not query available system memory";
        return {};
    }

    const RealESRGAN::DeviceMemorySnapshot deviceSnapshot =
        realesrgan->getDeviceMemorySnapshot();
    const auto deviceBudget = makeDeviceMemoryBudget(deviceSnapshot);
    if (!deviceBudget) {
        qWarning() << "[Upscayl] upscale() could not query a Vulkan device memory budget";
        return {};
    }

    auto requiredCpuBytes =
        std::optional<quint64>(memoryEstimate->totalCpuBytes);
    if (deviceBudget->sharesSystemMemory) {
        requiredCpuBytes =
            checkedSum({*requiredCpuBytes, memoryEstimate->deviceBytes});
    }
    if (!requiredCpuBytes) {
        qWarning() << "[Upscayl] upscale() shared CPU/device memory estimate overflowed";
        return {};
    }
    if (*requiredCpuBytes > cpuBudget->usableBytes) {
        qWarning() << "[Upscayl] upscale() CPU memory budget exceeded:"
                   << "required bytes:" << *requiredCpuBytes
                   << "usable bytes:" << cpuBudget->usableBytes
                   << "available physical bytes:"
                   << cpuBudget->availablePhysicalBytes
                   << "available commit bytes:"
                   << cpuBudget->availableCommitBytes
                   << "conversion bytes:" << memoryEstimate->conversionBytes
                   << "output bytes:" << memoryEstimate->outputImageBytes
                   << "backend working bytes:"
                   << memoryEstimate->backendCpuBytes
                   << "shared device bytes:"
                   << (deviceBudget->sharesSystemMemory
                           ? memoryEstimate->deviceBytes
                           : 0);
        return {};
    }

    if (memoryEstimate->deviceBytes > deviceBudget->usableBytes) {
        qWarning() << "[Upscayl] upscale() device memory budget exceeded:"
                   << "required bytes:" << memoryEstimate->deviceBytes
                   << "usable bytes:" << deviceBudget->usableBytes
                   << "heap budget bytes:" << deviceBudget->budgetBytes
                   << "heap usage bytes:" << deviceBudget->usageBytes
                   << "usage known:" << deviceBudget->usageKnown
                   << "shares system memory:"
                   << deviceBudget->sharesSystemMemory;
        return {};
    }

    if (isAbortRequested(abortFlag)) {
        return {};
    }

    QImage imgRgba = inputImage;
    if (imgRgba.format() != QImage::Format_ARGB32 &&
        imgRgba.format() != QImage::Format_RGB32) {
        imgRgba = imgRgba.convertToFormat(QImage::Format_ARGB32);
    } else if (!hasPackedRgbaStorage(imgRgba)) {
        imgRgba = imgRgba.copy();
    }
    if (imgRgba.isNull() || !hasPackedRgbaStorage(imgRgba)) {
        qWarning() << "[Upscayl] upscale() failed to allocate a packed RGBA input buffer";
        return {};
    }

    QImage outImg(static_cast<int>(*outW), static_cast<int>(*outH),
                  QImage::Format_ARGB32);
    if (inputImage.colorSpace().isValid()) {
        outImg.setColorSpace(inputImage.colorSpace());
    }
    if (outImg.isNull() || !hasPackedRgbaStorage(outImg)) {
        qWarning() << "[Upscayl] upscale() failed to allocate output QImage of size:"
                   << *outW << "x" << *outH;
        return {};
    }

    const uchar *inputPixels = imgRgba.constBits();
    uchar *outputPixels = outImg.bits();
    if (!inputPixels || !outputPixels) {
        qWarning() << "[Upscayl] upscale() image storage is unavailable after allocation";
        return {};
    }

    const int ret =
        realesrgan->processPixels(inputPixels, inW, inH, outputPixels,
                                  static_cast<int>(*outW),
                                  static_cast<int>(*outH), abortFlag);

    if (ret != 0) {
        qWarning() << "[Upscayl] processPixels failed or aborted, code:" << ret;
        return {QImage(), ret};
    }

    return {outImg, 0};
}

void UpscaylScaler::destroy() {
    QMutexLocker locker(&mutex);
    if (realesrgan) {
        realesrgan.reset();
        loadedModel = "";
    }
}

class UpscalerPreloadState final {
public:
    struct Options final {
        bool upscalerEnabled = false;
        bool preloadEnabled = false;
        QString model;

        bool operator==(const Options &) const = default;

        [[nodiscard]] bool shouldPreload() const noexcept {
            return upscalerEnabled && preloadEnabled;
        }
    };

    struct Configuration final {
        Options options;
        uint64_t generation = 0;
    };
    QMutex mutex;
    std::shared_ptr<const Configuration> desiredConfiguration;
    uint64_t nextGeneration = 0;
    bool workerBusy = false;
    std::atomic<bool> shutdownRequested{false};
};

namespace {

constexpr int kPreloadWarmupDimension = 512;

void applyPreloadConfiguration(const UpscalerPreloadState::Configuration &configuration,
                               const std::atomic<bool> *shutdownFlag) {
    if (shutdownFlag && shutdownFlag->load(std::memory_order_relaxed)) {
        return;
    }
    if (!configuration.options.shouldPreload()) {
        UpscaylScaler::getInstance()->destroy();
        return;
    }

    const QString appDir = QCoreApplication::applicationDirPath();
    QImage dummy(kPreloadWarmupDimension,
                 kPreloadWarmupDimension,
                 QImage::Format_ARGB32);
    dummy.fill(Qt::black);
    const UpscaylInferenceResult warmupResult =
        UpscaylScaler::getInstance()->upscale(
            UpscaylInferenceRequest{appDir, configuration.options.model, dummy, shutdownFlag});
    if (!warmupResult.succeeded() &&
        (!shutdownFlag || !shutdownFlag->load(std::memory_order_relaxed))) {
        qWarning() << "[Upscayl] Model preload or warm-up failed";
    }
}

class UpscaylPreloadTask final : public QRunnable {
public:
    explicit UpscaylPreloadTask(std::shared_ptr<UpscalerPreloadState> preloadState)
        : state(std::move(preloadState)) {}

    void run() override {
        if (!state) {
            qWarning() << "[Upscayl] Preload worker started without shared state";
            return;
        }

        while (true) {
            if (state->shutdownRequested.load(std::memory_order_relaxed)) {
                QMutexLocker locker(&state->mutex);
                state->workerBusy = false;
                return;
            }

            std::shared_ptr<const UpscalerPreloadState::Configuration> configuration;
            {
                QMutexLocker locker(&state->mutex);
                configuration = state->desiredConfiguration;
                if (!configuration) {
                    state->workerBusy = false;
                    qWarning() << "[Upscayl] Preload worker has no desired configuration";
                    return;
                }
            }

            applyPreloadConfiguration(*configuration, &state->shutdownRequested);

            if (state->shutdownRequested.load(std::memory_order_relaxed)) {
                QMutexLocker locker(&state->mutex);
                state->workerBusy = false;
                return;
            }

            QMutexLocker locker(&state->mutex);
            if (!state->desiredConfiguration) {
                state->workerBusy = false;
                qWarning() << "[Upscayl] Desired preload configuration disappeared";
                return;
            }
            if (state->desiredConfiguration->generation == configuration->generation) {
                state->workerBusy = false;
                return;
            }
        }
    }

private:
    std::shared_ptr<UpscalerPreloadState> state;
};

} // namespace


Upscaler::Upscaler(QObject *parent) : QObject(parent) {
    pool = new QThreadPool(this);
    pool->setMaxThreadCount(1);
    upscaylTimer.setSingleShot(true);
    upscaylTimer.setInterval(kDebounceIntervalMs);
    connect(&upscaylTimer, &QTimer::timeout, this, &Upscaler::onUpscaylTimerTimeout);
    preloadState = std::make_shared<UpscalerPreloadState>();
    configuredModel = settings->upscaylModel();
}

Upscaler::~Upscaler() {
    if (currentAbortFlag) {
        currentAbortFlag->store(true, std::memory_order_relaxed);
    }
    if (preloadState) {
        preloadState->shutdownRequested.store(true, std::memory_order_relaxed);
        QMutexLocker locker(&preloadState->mutex);
        preloadState->desiredConfiguration.reset();
    }
    if (pool) {
        pool->waitForDone();
    }
    UpscaylScaler::getInstance()->destroy();
}

void Upscaler::requestUpscale(std::shared_ptr<Image> image, QSize targetSize, QString path) {
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
}

bool Upscaler::readSettings() {
    const QString newModel = settings->upscaylModel();
    const bool modelChanged = newModel != configuredModel;
    configuredModel = newModel;

    if (modelChanged) {
        invalidatePreview();
    }

    reconcilePreloadState(false);
    return modelChanged;
}

void Upscaler::reconcilePreloadState(bool forceReapply) {
    if (!preloadState) {
        qWarning() << "[Upscayl] Cannot reconcile preload state without shared state";
        return;
    }
    if (preloadState->shutdownRequested.load(std::memory_order_relaxed)) {
        return;
    }

    const UpscalerPreloadState::Options desiredOptions{
        settings->useUpscayl(),
        settings->preloadUpscayl(),
        settings->upscaylModel()
    };

    bool startWorker = false;
    {
        QMutexLocker locker(&preloadState->mutex);
        if (!forceReapply &&
            preloadState->desiredConfiguration &&
            preloadState->desiredConfiguration->options == desiredOptions) {
            return;
        }

        const uint64_t generation = ++preloadState->nextGeneration;
        preloadState->desiredConfiguration =
            std::make_shared<const UpscalerPreloadState::Configuration>(
                UpscalerPreloadState::Configuration{desiredOptions, generation});

        if (!preloadState->workerBusy) {
            preloadState->workerBusy = true;
            startWorker = true;
        }
    }

    if (startWorker) {
        QThreadPool::globalInstance()->start(new UpscaylPreloadTask(preloadState));
    }
}

void Upscaler::abortPreviewRequest() {
    currentGeneration.fetch_add(1, std::memory_order_relaxed);
    if (currentAbortFlag) {
        currentAbortFlag->store(true, std::memory_order_relaxed);
    }
    currentAbortFlag.reset();

    {
        QMutexLocker locker(&stateMutex);
        upscaylTimer.stop();
        pendingUpscaylImage.reset();
        pendingUpscaylPath = "";
        upscaylPendingRun = false;
    }
    emit upscaleAborted();
}

void Upscaler::invalidatePreview() {
    abortPreviewRequest();
    emit previewInvalidated();
}

void Upscaler::reset() {
    abortPreviewRequest();
    if (!settings->preloadUpscayl()) {
        reconcilePreloadState(true);
    }
}

bool Upscaler::isRequestStale(uint64_t taskGeneration) const {
    return taskGeneration != currentGeneration.load(std::memory_order_relaxed);
}

void Upscaler::onUpscaylTimerTimeout() {
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
        if (activeTaskGeneration.has_value()) {
            upscaylPendingRun = true;
        } else {
            triggerUpscaylProcessing(visibleRect, currentScale, dpr);
        }
    }
}

void Upscaler::triggerUpscaylProcessing(QRect visibleRect, double currentScale, double dpr) {
    if (!pendingUpscaylImage)
        return;

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
        return;
    }

    std::shared_ptr<const QImage> origQImage = pendingUpscaylImage->getImage();
    if (!origQImage || origQImage->isNull()) {
        return;
    }

    QImage croppedImg = origQImage->copy(origCrop);
    if (croppedImg.isNull()) {
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

    UpscalerTaskParams params;
    params.croppedImage = croppedImg;
    params.origCrop = origCrop;
    params.path = pendingUpscaylPath;
    params.targetSize = pendingUpscaylSize;
    params.modelName = configuredModel;
    params.generation = currentGeneration.load(std::memory_order_relaxed);
    activeTaskGeneration = params.generation;

    UpscalerRunnable *task = new UpscalerRunnable(this, params, currentAbortFlag);
    if (pool) {
        pool->start(task);
        emit upscaleStarted();
    } else {
        delete task;
        activeTaskGeneration.reset();
    }
}

void Upscaler::onTaskFinished(QImage cropImg, QRect origCrop, QString path, QSize targetSize, uint64_t taskGeneration) {
    if (activeTaskGeneration != taskGeneration) {
        return;
    }
    activeTaskGeneration.reset();

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
}

void Upscaler::onTaskAborted(QString path, QSize targetSize, uint64_t taskGeneration) {
    Q_UNUSED(path);
    Q_UNUSED(targetSize);
    if (activeTaskGeneration != taskGeneration) {
        return;
    }
    activeTaskGeneration.reset();
    emit upscaleAborted();

    if (upscaylPendingRun && pendingUpscaylImage) {
        upscaylPendingRun = false;
        onUpscaylTimerTimeout();
    }
}

void Upscaler::onTaskFailed(const QString &error, uint64_t taskGeneration) {
    if (activeTaskGeneration != taskGeneration) {
        return;
    }
    activeTaskGeneration.reset();
    emit upscaleFailed(error);

    if (upscaylPendingRun && pendingUpscaylImage) {
        upscaylPendingRun = false;
        onUpscaylTimerTimeout();
    }
}
