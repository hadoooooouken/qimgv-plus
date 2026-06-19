#include "upscalerrunnable.h"
#include "upscaler.h"
#include <QCoreApplication>
#include <QMetaObject>
#include <QDebug>

UpscalerRunnable::UpscalerRunnable(Upscaler *upscaler, const UpscalerTaskParams &params, std::shared_ptr<std::atomic<bool>> abortFlag)
    : upscaler(upscaler), params(params), abortFlag(abortFlag) {}

void UpscalerRunnable::run() {
#ifdef USE_UPSCAYL
    if (upscaler->isRequestStale(params.generation)) {
        QMetaObject::invokeMethod(upscaler, "onTaskAborted", Qt::QueuedConnection,
                                  Q_ARG(QString, params.path), Q_ARG(QSize, params.targetSize), Q_ARG(uint64_t, params.generation));
        return;
    }

    QString appDir = QCoreApplication::applicationDirPath();
    if (!UpscaylScaler::getInstance()->init(appDir)) {
        qWarning() << "[Upscayl] background init() failed";
        QMetaObject::invokeMethod(upscaler, "onTaskAborted", Qt::QueuedConnection,
                                  Q_ARG(QString, params.path), Q_ARG(QSize, params.targetSize), Q_ARG(uint64_t, params.generation));
        return;
    }

    QImage upscaled = UpscaylScaler::getInstance()->upscale(params.croppedImage, abortFlag.get());
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
#else
    QMetaObject::invokeMethod(upscaler, "onTaskAborted", Qt::QueuedConnection,
                              Q_ARG(QString, params.path), Q_ARG(QSize, params.targetSize), Q_ARG(uint64_t, params.generation));
#endif
}
