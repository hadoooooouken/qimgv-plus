#pragma once

#include <QObject>
#include <QThreadPool>
#include <QThread>
#include <QMutex>
#include <QSet>
#include <QString>
#include <memory>
#include "components/cache/cache.h"
#include "scalerrequest.h"
#include "scalerrunnable.h"

class Scaler : public QObject {
    Q_OBJECT
public:
    explicit Scaler(Cache *_cache, QObject *parent = nullptr);
    ~Scaler();

signals:
    void scalingFinished(QImage result, ScalerRequest request);
    void acceptScalingResult(QImage image, ScalerRequest req);
    void startBufferedRequest();

public slots:
    void requestScaled(ScalerRequest req);
    void clear();

private slots:
    void onTaskStart(ScalerRequest req);
    void onTaskFinish(QImage scaled, ScalerRequest req);
    void slotStartBufferedRequest();
    void slotForwardScaledResult(QImage image, ScalerRequest req);

private:
    QThreadPool *pool;
    std::unique_ptr<ScalerRunnable> runnable;
    bool buffered, running;
    ScalerRequest bufferedRequest, startedRequest;
    bool mCleared = false;

    Cache *cache;
    QSet<QString> reservedPaths;

    void startRequest(ScalerRequest req);
    void updateReservations();

    QSemaphore sem{1};
};

