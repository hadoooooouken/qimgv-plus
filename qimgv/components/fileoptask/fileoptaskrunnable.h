#pragma once

#include <QDir>
#include <QFileInfo>
#include <QList>
#include <QMetaType>
#include <QObject>
#include <QPointer>
#include <QRunnable>
#include <QString>
#include <atomic>
#include <memory>

#include "gui/dialogs/filereplacedialog.h"
#include "utils/fileoperations.h"

class DirectoryModel;
class MW;

// A single copy/move request submitted to FileOpController. paths are the
// top-level items the person selected (files and/or directories); directory
// entries are expanded recursively by FileOperationTask::run().
struct FileOpRequest {
    QList<QString> paths;
    QString destDirectory;
    bool isMove = false;
};

// Emitted after each individual file is copied or moved, so the caller can
// surface lightweight progress feedback. There is intentionally no running
// "total": computing one upfront would require a separate recursive
// pre-walk of the same tree FileOperationTask is about to walk anyway,
// doubling I/O for what is meant to stay a lightweight status line.
struct FileOpProgress {
    QString currentPath;
    int filesDone = 0;
    bool isMove = false;
};

// Emitted once when a submitted FileOpRequest is done, either because every
// path was processed or because the person hit Cancel in FileReplaceDialog
// (there is no separate progress-UI Cancel control yet).
struct FileOpSummary {
    bool cancelled = false;
    int filesProcessed = 0;
    bool isMove = false;
};

Q_DECLARE_METATYPE(FileOpProgress)
Q_DECLARE_METATYPE(FileOpSummary)

// Lives on the GUI thread; FileOperationTask reports through it from a pool
// thread, so connections made from its signals are auto-queued by Qt based
// on the receiving object's thread affinity. Mirrors LoaderTaskNotifier.
class FileOpTaskNotifier final : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    // May be called by pool threads; receivers must use queued connections.
    void reportProgress(FileOpProgress progress);
    void reportFinished(FileOpSummary summary);

signals:
    void progressReported(FileOpProgress progress);
    void operationFinished(FileOpSummary summary);
};

// Recursively copies or moves FileOpRequest::paths into destDirectory on a
// QThreadPool worker thread.
//
// GUI-owned state is never touched directly from run(): the file-replace
// confirmation dialog (MW::fileReplaceDialog()) and the DirectoryModel calls
// that also perform DirectoryManager bookkeeping (moveFileTo(), removeDir())
// are marshalled back onto the GUI thread via QMetaObject::invokeMethod(...,
// Qt::BlockingQueuedConnection), which blocks this worker thread until they
// return - safe here since the owning pool's maxThreadCount is 1, so nothing
// else on that pool is waiting on this thread. Plain filesystem calls
// (FileOperations::copyFileTo(), FileOperations::removeFile()) don't touch
// DirectoryManager and run directly on this thread, same as before.
class FileOperationTask final : public QRunnable {
public:
    FileOperationTask(FileOpRequest request,
                      QPointer<DirectoryModel> model,
                      QPointer<MW> mw,
                      FileOpTaskNotifier &notifier,
                      std::shared_ptr<std::atomic<bool>> cancelled);

    void run() override;

private:
    void processCopy(const QString &path, const QString &destDirectory,
                     DialogResult &overwriteFiles);
    void processMove(const QString &path, const QString &destDirectory,
                     DialogResult &overwriteFiles);
    [[nodiscard]] bool isCancelled() const;

    FileOpRequest request;
    QPointer<DirectoryModel> model;
    QPointer<MW> mw;
    FileOpTaskNotifier &notifier;
    std::shared_ptr<std::atomic<bool>> cancelled;
    int filesProcessed = 0;
};
