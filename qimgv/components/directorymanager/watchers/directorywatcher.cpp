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
    if(!isObserving()) {
        // Reuse worker instance
        d->worker->setRunning(true);
        d->workerThread->start();
    }
    //qDebug() << TAG << "Observing path:" << d->currentDirectory;
}

void DirectoryWatcher::stopObserving()
{
    Q_D(DirectoryWatcher);
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
