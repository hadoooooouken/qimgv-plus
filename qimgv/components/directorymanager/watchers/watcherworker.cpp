#include "watcherworker.h"
#include <QDebug>

WatcherWorker::WatcherWorker()
{
}

void WatcherWorker::setRunning(bool running) {
    isRunning.store(running, std::memory_order_relaxed);
}

bool WatcherWorker::isWorkerRunning() const {
    return isRunning.load(std::memory_order_relaxed);
}
