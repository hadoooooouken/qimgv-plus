#include "watcherworker.h"
#include <QDebug>

WatcherWorker::WatcherWorker()
{
}

void WatcherWorker::setRunning(bool running) {
    isRunning.store(running, std::memory_order_relaxed);
}
