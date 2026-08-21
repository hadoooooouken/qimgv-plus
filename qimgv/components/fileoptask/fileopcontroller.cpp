#include "fileopcontroller.h"

#include <utility>

namespace {
// Serializes copy/move requests so a second one queues behind the first
// instead of racing it on overlapping source/destination trees - mirrors
// Scaler's and Upscaler's single-worker pools.
constexpr int kFileOpThreadCount = 1;
} // namespace

FileOpController::FileOpController(QPointer<DirectoryModel> model, QPointer<MW> mw,
                                   QObject *parent)
    : QObject(parent),
      model(model),
      mw(mw),
      cancelled(std::make_shared<std::atomic<bool>>(false)) {
    pool = std::make_unique<QThreadPool>();
    pool->setMaxThreadCount(kFileOpThreadCount);
    qRegisterMetaType<FileOpProgress>();
    qRegisterMetaType<FileOpSummary>();
    connect(&notifier, &FileOpTaskNotifier::progressReported, this,
            &FileOpController::progress, Qt::QueuedConnection);
    connect(&notifier, &FileOpTaskNotifier::operationFinished, this,
            &FileOpController::finished, Qt::QueuedConnection);
}

FileOpController::~FileOpController() {
    cancelled->store(true);
    pool->waitForDone();
}

void FileOpController::startCopy(QList<QString> paths, QString destDirectory) {
    FileOpRequest request;
    request.paths = std::move(paths);
    request.destDirectory = std::move(destDirectory);
    request.isMove = false;
    submit(std::move(request));
}

void FileOpController::startMove(QList<QString> paths, QString destDirectory) {
    FileOpRequest request;
    request.paths = std::move(paths);
    request.destDirectory = std::move(destDirectory);
    request.isMove = true;
    submit(std::move(request));
}

void FileOpController::submit(FileOpRequest request) {
    if (request.paths.isEmpty())
        return;
    auto *task = new FileOperationTask(std::move(request), model, mw, notifier, cancelled);
    pool->start(task);
}
