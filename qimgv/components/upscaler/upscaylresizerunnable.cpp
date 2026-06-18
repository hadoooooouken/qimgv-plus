#include "upscaylresizerunnable.h"

#include "upscaler.h"
#include <QCoreApplication>

UpscaylResizeRunnable::UpscaylResizeRunnable(const UpscaylResizeRequest &request)
    : m_request(request) {}

void UpscaylResizeRunnable::run() {
#ifdef USE_UPSCAYL
    if (!m_request.sourceImage || m_request.sourceImage->isNull()) {
        emit finished(m_request.generation, m_request.path, QImage(), false, tr("Source image is empty."));
        return;
    }

    const QString appDir = QCoreApplication::applicationDirPath();
    if (!UpscaylScaler::getInstance()->init(appDir, m_request.modelName)) {
        emit finished(m_request.generation, m_request.path, QImage(), false, tr("Could not initialize AI model."));
        return;
    }

    QImage upscaled = UpscaylScaler::getInstance()->upscale(*m_request.sourceImage);
    if (upscaled.isNull()) {
        emit finished(m_request.generation, m_request.path, QImage(), false, tr("AI resize failed."));
        return;
    }

    if (upscaled.size() != m_request.targetSize) {
        std::shared_ptr<const QImage> upscaledPtr = std::make_shared<const QImage>(upscaled);
        upscaled = ImageLib::scaled(upscaledPtr, m_request.targetSize, m_request.filter);
    }

    if (upscaled.isNull()) {
        emit finished(m_request.generation, m_request.path, QImage(), false, tr("Final resize failed."));
        return;
    }

    emit finished(m_request.generation, m_request.path, upscaled, true, QString());
#else
    emit finished(m_request.generation, m_request.path, QImage(), false, tr("AI resize is disabled in this build."));
#endif
}
