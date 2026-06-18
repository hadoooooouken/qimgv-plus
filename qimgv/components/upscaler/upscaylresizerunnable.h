#pragma once

#include <QObject>
#include <QImage>
#include <QRunnable>
#include <QSize>
#include <QString>
#include <memory>
#include "utils/imagelib.h"

struct UpscaylResizeRequest {
    QString path;
    QSize targetSize;
    ScalingFilter filter = QI_FILTER_BILINEAR;
    QString modelName;
    std::shared_ptr<const QImage> sourceImage;
    int generation = 0;
};

class UpscaylResizeRunnable : public QObject, public QRunnable {
    Q_OBJECT
public:
    explicit UpscaylResizeRunnable(const UpscaylResizeRequest &request);

    void run() override;

signals:
    void finished(int generation, QString path, QImage image, bool success, QString error);

private:
    UpscaylResizeRequest m_request;
};
