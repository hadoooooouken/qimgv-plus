#pragma once

#include <QObject>
#include <QRunnable>
#include <QThread>
#include <QDebug>
#include "components/cache/cache.h"
#include "scalerrequest.h"
#include "utils/imagelib.h"
class ScalerRunnable : public QObject, public QRunnable
{
    Q_OBJECT
public:
    explicit ScalerRunnable();
    void setRequest(ScalerRequest r);
    void run();
signals:
    void started(ScalerRequest);
    void finished(QImage, ScalerRequest);

private:
    ScalerRequest req;
};
