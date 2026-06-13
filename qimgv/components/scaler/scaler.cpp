#include "scaler.h"
#include "utils/colormanager.h"
#include <QDebug>

#define SCALER_DEBUG 0

#define SCALER_TRACE(msg) \
    do { \
        if (SCALER_DEBUG) { \
            qDebug() << "[Scaler]" << msg; \
        } \
    } while (0)

/* What this should do in theory:
 * 1 request comes
 * 2 we run it
 * 3a if during scaling no new requests came, we return the result and forget about it. end.
 * 3b if some requests did come, by the end of current task we dispose of its result,
 *    start the last task that came and ignore the middle ones.
 */

Scaler::Scaler(Cache *_cache, QObject *parent)
    : QObject(parent),
      buffered(false),
      running(false),
      currentRequestTimestamp(0),
      cache(_cache)
{
    pool = new QThreadPool(this);
    pool->setMaxThreadCount(1);
    runnable = std::make_unique<ScalerRunnable>();
    runnable->setAutoDelete(false);
    connect(this, &Scaler::startBufferedRequest, this, &Scaler::slotStartBufferedRequest, Qt::DirectConnection);
    connect(runnable.get(), &ScalerRunnable::started, this, &Scaler::onTaskStart, Qt::DirectConnection);
    connect(runnable.get(), &ScalerRunnable::finished, this, &Scaler::onTaskFinish, Qt::DirectConnection);
    connect(this, &Scaler::acceptScalingResult, this, &Scaler::slotForwardScaledResult, Qt::QueuedConnection);
}

Scaler::~Scaler() {
    clear();
    if(pool) {
        pool->waitForDone();
    }
    disconnect(runnable.get(), nullptr, this, nullptr);
    disconnect(this, nullptr, nullptr, nullptr);
}

/*
 * State transition matrix for requestScaled and updateReservations:
 *
 * | running | buffered | same-image relationship                    | Action / Reconciled Cache State
 * |---------|----------|---------------------------------------------|---------------------------------
 * | false   | false    | N/A                                         | Reserve new req, start task.
 * | false   | true     | new req == buffered req                     | Keep buffered req, do nothing.
 * | false   | true     | new req != buffered req                     | Release old buffered, reserve new buffered.
 * | true    | false    | new req == running req                      | Keep running req (shared ref), do nothing.
 * | true    | false    | new req != running req                      | Reserve new buffered req.
 * | true    | true     | new req == buffered req                     | Keep buffered req, do nothing.
 * | true    | true     | new req != buffered req (new == running)    | Release old buffered req, keep running req.
 * | true    | true     | new req != buffered req (new != running)    | Release old buffered req, reserve new buffered req.
 *
 * The updateReservations() function automatically reconciles all of these states by checking
 * the union of the running and buffered request image paths and adjusting cache locks.
 */

void Scaler::updateReservations() {
    QSet<QString> desired;
    if (running && startedRequest.image) {
        desired.insert(startedRequest.image->filePath());
    }
    if (buffered && bufferedRequest.image) {
        desired.insert(bufferedRequest.image->filePath());
    }

    // Release paths that are no longer needed
    for (auto it = reservedPaths.begin(); it != reservedPaths.end(); ) {
        if (!desired.contains(*it)) {
            SCALER_TRACE("Releasing cache reservation for:" << *it);
            cache->release(*it);
            it = reservedPaths.erase(it);
        } else {
            ++it;
        }
    }

    // Reserve paths that are now needed
    for (const QString &path : desired) {
        if (!reservedPaths.contains(path)) {
            SCALER_TRACE("Reserving cache for:" << path);
            cache->reserve(path);
            reservedPaths.insert(path);
        }
    }
}

void Scaler::requestScaled(ScalerRequest req) {
    sem.acquire(1);
    SCALER_TRACE("requestScaled for image:" << (req.image ? req.image->fileName() : "null"));
    if(!running) {
        bufferedRequest = req;
        buffered = true;
        updateReservations();
        startRequest(req);
    } else {
        bufferedRequest = req;
        buffered = true;
        updateReservations();
    }
    sem.release(1);
}

void Scaler::clear() {
    sem.acquire(1);
    buffered = false;
    bufferedRequest = ScalerRequest();
    mCleared = true;
    updateReservations();
    sem.release(1);
}


void Scaler::onTaskStart(ScalerRequest req) {
    sem.acquire(1);
    running = true;
    mCleared = false;
    if(buffered && bufferedRequest == req) {
        buffered = false;
    }
    startedRequest = req;
    SCALER_TRACE("onTaskStart for image:" << (req.image ? req.image->fileName() : "null"));
    updateReservations();
    sem.release(1);
}

void Scaler::onTaskFinish(QImage scaled, ScalerRequest req) {
    sem.acquire(1);
    running = false;
    SCALER_TRACE("onTaskFinish for image:" << (req.image ? req.image->fileName() : "null"));
    if(mCleared) {
        mCleared = false;
        updateReservations();
        sem.release(1);
        return;
    }
    if(buffered) {
        emit startBufferedRequest();
        updateReservations();
        sem.release(1);
    } else {
        updateReservations();
        sem.release(1);
        emit acceptScalingResult(scaled, req);
    }
}


void Scaler::slotStartBufferedRequest() {
    startRequest(bufferedRequest);
}

void Scaler::slotForwardScaledResult(QImage image, ScalerRequest req) {
    QPixmap pixmap = QPixmap::fromImage(ColorManager::applyColorManagement(image));
    emit scalingFinished(pixmap, req);
}

void Scaler::startRequest(ScalerRequest req) {
    runnable->setRequest(req);
    pool->start(runnable.get());
}
