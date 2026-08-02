#pragma once

#include <QRunnable>
#include <QImage>
#include <QRect>
#include <QString>
#include <QSize>
#include <memory>
#include <atomic>

class Upscaler;

struct UpscalerTaskParams {
    QImage croppedImage;
    QRect origCrop;
    QString path;
    QSize targetSize;
    QString modelName;
    uint64_t generation = 0;
};

class UpscalerRunnable : public QRunnable {
public:
    UpscalerRunnable(Upscaler *upscaler, const UpscalerTaskParams &params, std::shared_ptr<std::atomic<bool>> abortFlag);
    void run() override;

private:
    Upscaler *upscaler;
    UpscalerTaskParams params;
    std::shared_ptr<std::atomic<bool>> abortFlag;
};
