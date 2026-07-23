#include "windowswatcher_p.h"
#include "windowsworker.h"

QString lastError() {
    char buffer[1024];
    DWORD lastError = GetLastError();
    int res = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM,
                             nullptr,
                             lastError,
                             LANG_SYSTEM_DEFAULT,
                             buffer,
                             sizeof(buffer),
                             nullptr);

    QString line = QString(__FILE__) + "::" + QString::number(__LINE__) + ": ";
    return res == 0 ? QString::number(GetLastError()) : line + buffer;
}

WindowsWatcherPrivate::WindowsWatcherPrivate(WindowsWatcher* qq)
    : DirectoryWatcherPrivate(qq, new WindowsWorker())
{
    auto windowsWorker = static_cast<WindowsWorker*>(worker.data());

    connect(windowsWorker, &WindowsWorker::notifyEvent,
            this, &WindowsWatcherPrivate::dispatchNotify);
}

HANDLE WindowsWatcherPrivate::requestDirectoryHandle(const QString& path)
{
    HANDLE hDirectory;

    do {
        hDirectory = CreateFileW(
                    path.toStdWString().c_str(),
                    FILE_LIST_DIRECTORY,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    nullptr,
                    OPEN_EXISTING,
                    FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
                    nullptr);

        if (hDirectory == INVALID_HANDLE_VALUE)
        {
            if (GetLastError() == ERROR_SHARING_VIOLATION)
            {
                qWarning() << "ERROR_SHARING_VIOLATION waiting for 1 sec";
                QThread::sleep(1);
            }
            else
            {
                qWarning() << lastError();
                return INVALID_HANDLE_VALUE;
            }
        }
    } while (hDirectory == INVALID_HANDLE_VALUE);
    return hDirectory;
}

void WindowsWatcherPrivate::dispatchNotify(int action, const QString &name) {
    Q_Q(WindowsWatcher);

    switch (action)
    {
        case FILE_ACTION_ADDED:
            emit q->fileCreated(name);
            break;

        case FILE_ACTION_MODIFIED:
            emit q->fileModified(name);
            break;

        case FILE_ACTION_REMOVED:
            emit q->fileDeleted(name);
            break;

        case FILE_ACTION_RENAMED_NEW_NAME:
            emit q->fileRenamed(oldFileName, name);
            break;

        case FILE_ACTION_RENAMED_OLD_NAME:
            oldFileName = name;
            break;

        default:
            qWarning() << "Some error, notify->Action" << action;
    }
}

WindowsWatcher::WindowsWatcher()
    : DirectoryWatcher(new WindowsWatcherPrivate(this))
{
    Q_D(WindowsWatcher);
    connect(d->workerThread.data(), &QThread::started, d->worker.data(), &WatcherWorker::run);
    d->worker.data()->moveToThread(d->workerThread.data());

    auto windowsWorker = static_cast<WindowsWorker*>(d->worker.data());

    connect(windowsWorker, &WindowsWorker::finished, d->workerThread.data(), &QThread::quit);

    connect(windowsWorker, &WindowsWorker::started, this, &WindowsWatcher::observingStarted);
    connect(windowsWorker, &WindowsWorker::finished, this, &WindowsWatcher::observingStopped);

    connect(windowsWorker, &WindowsWorker::finished, this, [this, d]() {
        if (d->pendingRestart) {
            d->pendingRestart = false;
            if (!d->currentDirectory.isEmpty()) {
                setWatchPath(d->currentDirectory);
                observe();
            }
        }
    });
}

WindowsWatcher::WindowsWatcher(const QString& path)
    : DirectoryWatcher(new WindowsWatcherPrivate(this))
{
    Q_D(WindowsWatcher);
    setWatchPath(path);
}

void WindowsWatcher::setWatchPath(const QString &path) {
    Q_D(WindowsWatcher);
    DirectoryWatcher::setWatchPath(path);
    HANDLE hDirectory = d->requestDirectoryHandle(path);
    if (hDirectory == INVALID_HANDLE_VALUE)
    {
        qWarning() << "requestDirectoryHandle: INVALID_HANDLE_VALUE";
        return;
    }

    auto windowsWorker = static_cast<WindowsWorker*>(d->worker.data());
    windowsWorker->setDirectoryHandle(hDirectory);
}
