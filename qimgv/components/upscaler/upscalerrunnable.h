#pragma once

#include <QRunnable>
#include <QImage>
#include <QRect>
#include <QString>
#include <QSize>

class Upscaler;

struct UpscalerTaskParams {
    QImage croppedImage;
    QRect origCrop;
    QString path;
    QSize targetSize;
};

class UpscalerRunnable : public QRunnable {
public:
    UpscalerRunnable(Upscaler *upscaler, const UpscalerTaskParams &params);
    void run() override;

private:
    Upscaler *upscaler;
    UpscalerTaskParams params;
};
