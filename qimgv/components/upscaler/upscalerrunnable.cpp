#include "upscalerrunnable.h"
#include "upscaler.h"
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
    QImage upscaled = inferenceResult.image;
    if (upscaled.isNull()) {
        QMetaObject::invokeMethod(upscaler, "onTaskAborted", Qt::QueuedConnection,
                                  Q_ARG(QString, params.path), Q_ARG(QSize, params.targetSize), Q_ARG(uint64_t, params.generation));
        return;
    }

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
