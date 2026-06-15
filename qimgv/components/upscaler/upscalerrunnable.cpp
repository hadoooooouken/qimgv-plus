#include "upscalerrunnable.h"
#include "upscaler.h"
#include <QCoreApplication>
#include <QMetaObject>
#include <QDebug>

UpscalerRunnable::UpscalerRunnable(Upscaler *upscaler, const UpscalerTaskParams &params)
    : upscaler(upscaler), params(params) {}

void UpscalerRunnable::run() {
#ifdef USE_UPSCAYL
    if (upscaler->isRequestStale(params.path, params.targetSize)) {
        QMetaObject::invokeMethod(upscaler, "onTaskAborted", Qt::QueuedConnection,
                                  Q_ARG(QString, params.path), Q_ARG(QSize, params.targetSize));
        return;
    }

    QString appDir = QCoreApplication::applicationDirPath();
    if (!UpscaylScaler::getInstance()->init(appDir)) {
        qWarning() << "[Upscayl] background init() failed";
        QMetaObject::invokeMethod(upscaler, "onTaskAborted", Qt::QueuedConnection,
                                  Q_ARG(QString, params.path), Q_ARG(QSize, params.targetSize));
        return;
    }

    QImage upscaled = UpscaylScaler::getInstance()->upscale(params.croppedImage);
    if (upscaled.isNull()) {
        QMetaObject::invokeMethod(upscaler, "onTaskAborted", Qt::QueuedConnection,
                                  Q_ARG(QString, params.path), Q_ARG(QSize, params.targetSize));
        return;
    }

    if (upscaler->isRequestStale(params.path, params.targetSize)) {
        QMetaObject::invokeMethod(upscaler, "onTaskAborted", Qt::QueuedConnection,
                                  Q_ARG(QString, params.path), Q_ARG(QSize, params.targetSize));
        return;
    }

    QMetaObject::invokeMethod(upscaler, "onTaskFinished", Qt::QueuedConnection,
                              Q_ARG(QImage, upscaled),
                              Q_ARG(QRect, params.origCrop),
                              Q_ARG(QString, params.path),
                              Q_ARG(QSize, params.targetSize));
#else
    QMetaObject::invokeMethod(upscaler, "onTaskAborted", Qt::QueuedConnection,
                              Q_ARG(QString, params.path), Q_ARG(QSize, params.targetSize));
#endif
}
