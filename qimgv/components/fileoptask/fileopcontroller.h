#pragma once

#include <QObject>
#include <QPointer>
#include <QString>
#include <QThreadPool>
#include <atomic>
#include <memory>

#include "fileoptaskrunnable.h"

class DirectoryModel;
class MW;

// Runs interactive copy/move requests (Core::interactiveCopy()/
// interactiveMove()) off the GUI thread via a single-worker QThreadPool, so
// large directory trees no longer freeze the UI while they're copied or
// moved. See FileOperationTask for the actual recursive copy/move logic and
// for which calls get marshalled back onto the GUI thread.
class FileOpController final : public QObject {
    Q_OBJECT
public:
    explicit FileOpController(QPointer<DirectoryModel> model, QPointer<MW> mw,
                              QObject *parent = nullptr);
    ~FileOpController() override;

    void startCopy(QList<QString> paths, QString destDirectory);
    void startMove(QList<QString> paths, QString destDirectory);

signals:
    // Emitted after each individual file is copied/moved.
    void progress(FileOpProgress progress);
    // Emitted once per submitted request, when it completes or is cancelled.
    void finished(FileOpSummary summary);

private:
    void submit(FileOpRequest request);

    QPointer<DirectoryModel> model;
    QPointer<MW> mw;
    std::unique_ptr<QThreadPool> pool;
    FileOpTaskNotifier notifier;
    // Flipped to true only when FileOpController is destroyed, so any
    // in-flight FileOperationTask unwinds instead of dragging out app
    // shutdown. Submitted requests otherwise always run to completion; the
    // only cancellation path exposed to the person today is the Cancel
    // button on FileReplaceDialog, handled entirely inside FileOperationTask.
    std::shared_ptr<std::atomic<bool>> cancelled;
};
