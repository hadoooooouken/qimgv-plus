#include "directorywatcher_p.h"

#include "windows/windowswatcher.h"

#define TAG         "[DirectoryWatcher]"

DirectoryWatcherPrivate::DirectoryWatcherPrivate(DirectoryWatcher* qq, WatcherWorker* w) :
    q_ptr(qq),
    worker(w),
    workerThread(new QThread())
{
}

DirectoryWatcher::~DirectoryWatcher() {
    delete d_ptr;
    d_ptr = nullptr;
}

// Move this function to some creational class
DirectoryWatcher *DirectoryWatcher::newInstance()
{
    DirectoryWatcher* watcher;

    watcher = new WindowsWatcher();

    return watcher;
}

void DirectoryWatcher::setWatchPath(const QString& path) {
    Q_D(DirectoryWatcher);
    d->currentDirectory = path;
}

QString DirectoryWatcher::watchPath() const {
    Q_D(const DirectoryWatcher);
    return d->currentDirectory;
}

void DirectoryWatcher::observe()
{
    Q_D(DirectoryWatcher);
    if(!d->workerThread->isRunning()) {
        d->pendingRestart = false;
        d->worker->setRunning(true);
        d->workerThread->start();
    } else if(!d->worker->isWorkerRunning()) {
        // Thread is currently shutting down from a previous stop request.
        // Queue restart when worker finishes exiting.
        d->pendingRestart = true;
        d->worker->setRunning(true);
    }
}

void DirectoryWatcher::stopObserving()
{
    Q_D(DirectoryWatcher);
    d->pendingRestart = false;
    d->worker->setRunning(false);
}

bool DirectoryWatcher::isObserving()
{
    Q_D(DirectoryWatcher);
    return d->workerThread->isRunning();
}

DirectoryWatcher::DirectoryWatcher(DirectoryWatcherPrivate* ptr) {
    d_ptr = ptr;
}
