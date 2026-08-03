#include "upscalerrunnable.h"
#include "upscaler.h"
#include "utils/colormanager.h"
#include <QCoreApplication>
#include <QMetaObject>

UpscalerRunnable::UpscalerRunnable(Upscaler *upscaler, const UpscalerTaskParams &params, std::shared_ptr<std::atomic<bool>> abortFlag)
    : upscaler(upscaler), params(params), abortFlag(abortFlag) {}

void UpscalerRunnable::run() {
    if (upscaler->isRequestStale(params.generation)) {
        QMetaObject::invokeMethod(upscaler, "onTaskAborted", Qt::QueuedConnection,
                                  Q_ARG(QString, params.path), Q_ARG(QSize, params.targetSize), Q_ARG(uint64_t, params.generation));
        return;
    }

    const QString appDir = QCoreApplication::applicationDirPath();
    const UpscaylInferenceResult inferenceResult =
        UpscaylScaler::getInstance()->upscale(
            UpscaylInferenceRequest{appDir, params.modelName, params.croppedImage,
                                    abortFlag.get()});

    if (upscaler->isRequestStale(params.generation) ||
        inferenceResult.error == UpscaylInferenceError::Aborted) {
        QMetaObject::invokeMethod(upscaler, "onTaskAborted", Qt::QueuedConnection,
                                  Q_ARG(QString, params.path), Q_ARG(QSize, params.targetSize), Q_ARG(uint64_t, params.generation));
        return;
    }

    if (!inferenceResult.succeeded()) {
        QString errorMsg;
        if (inferenceResult.error == UpscaylInferenceError::ModelLoadFailed) {
            errorMsg = QCoreApplication::translate("Upscaler", "AI Model failed to load");
        } else {
            errorMsg = QCoreApplication::translate("Upscaler", "AI Upscaling processing failed");
        }
        QMetaObject::invokeMethod(upscaler, "onTaskFailed", Qt::QueuedConnection,
                                  Q_ARG(QString, errorMsg), Q_ARG(uint64_t, params.generation));
        return;
    }

    QImage upscaled = ColorManager::applyColorManagement(inferenceResult.image);

    if (upscaler->isRequestStale(params.generation)) {
        QMetaObject::invokeMethod(upscaler, "onTaskAborted", Qt::QueuedConnection,
                                  Q_ARG(QString, params.path), Q_ARG(QSize, params.targetSize), Q_ARG(uint64_t, params.generation));
        return;
    }

    QMetaObject::invokeMethod(upscaler, "onTaskFinished", Qt::QueuedConnection,
                              Q_ARG(QImage, upscaled),
                              Q_ARG(QRect, params.origCrop),
                              Q_ARG(QString, params.path),
                              Q_ARG(QSize, params.targetSize),
                              Q_ARG(uint64_t, params.generation));
}
