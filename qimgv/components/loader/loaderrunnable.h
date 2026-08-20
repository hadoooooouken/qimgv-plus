#pragma once

#include <QObject>
#include <QRunnable>
#include "utils/imagefactory.h"

struct ImageLoadRequest {
    QString path;
    quint64 taskId = 0;
    DecodeContext decodeContext;
};

class LoaderRunnable: public QObject, public QRunnable
{
    Q_OBJECT
public:
    explicit LoaderRunnable(ImageLoadRequest request,
                            QObject *parent = nullptr);
    void run();
private:
    ImageLoadRequest request;
signals:
    void finished(quint64 taskId, std::shared_ptr<Image> image);
};
