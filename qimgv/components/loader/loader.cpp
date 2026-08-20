#include "loader.h"

#include <utility>

namespace {
constexpr int MaxLoaderThreads = 3;
}

Loader::Loader() {
    pool = new QThreadPool(this);
    pool->setMaxThreadCount(MaxLoaderThreads);
}

Loader::~Loader() {
    clearTasks();
}

void Loader::clearTasks() {
    cancelTasks();
    pool->waitForDone();

    for (TaskRecord &record : tasks) {
        if (record.runnable)
            record.runnable->deleteLater();
    }
    tasks.clear();
    taskIdsByPath.clear();
}

bool Loader::isBusy() const {
    return (tasks.count() != 0);
}

bool Loader::isLoading(QString path) {
    return taskIdsByPath.contains(path);
}

std::shared_ptr<Image> Loader::load(QString path) {
    return ImageFactory::createImage(path);
}

// Cancels obsolete loads before starting the requested priority load.
void Loader::loadAsyncPriority(QString path) {
    cancelTasks();
    doLoadAsync(path, 1);
}

void Loader::loadAsync(QString path) {
    doLoadAsync(path, 0);
}

void Loader::doLoadAsync(QString path, int priority) {
    if(taskIdsByPath.contains(path)) {
        return;
    }

    const quint64 taskId = nextTaskId();
    std::stop_source cancellationSource;
    ImageLoadRequest request;
    request.path = path;
    request.taskId = taskId;
    request.decodeContext.cancellationToken = cancellationSource.get_token();

    auto *runnable = new LoaderRunnable(std::move(request), this);
    runnable->setAutoDelete(false);
    connect(runnable, &LoaderRunnable::finished, this,
            &Loader::onLoadFinished, Qt::QueuedConnection);

    taskIdsByPath.insert(path, taskId);
    tasks.insert(taskId,
                 TaskRecord{path, std::move(cancellationSource),
                            QPointer<LoaderRunnable>(runnable),
                            TaskPhase::Queued});
    pool->start(runnable, priority);
}

void Loader::onLoadFinished(quint64 taskId, std::shared_ptr<Image> image) {
    auto task = tasks.find(taskId);
    if (task == tasks.end())
        return;

    const QString path = task->path;
    const bool cancelled = task->cancellationSource.stop_requested();
    const auto indexedTask = taskIdsByPath.constFind(path);
    if (indexedTask != taskIdsByPath.cend() && indexedTask.value() == taskId)
        taskIdsByPath.remove(path);
    if (task->runnable)
        task->runnable->deleteLater();
    tasks.erase(task);

    if (cancelled)
        return;

    if(!image || !image->isLoaded())
        emit loadFailed(path);
    else
        emit loadFinished(image, path);
}

quint64 Loader::nextTaskId()
{
    do {
        ++mNextTaskId;
    } while (tasks.contains(mNextTaskId));
    return mNextTaskId;
}

void Loader::cancelTasks() {
    QList<quint64> removedTaskIds;
    for (auto it = tasks.begin(); it != tasks.end(); ++it) {
        const quint64 taskId = it.key();
        TaskRecord &record = it.value();
        record.cancellationSource.request_stop();

        const auto indexedTask = taskIdsByPath.constFind(record.path);
        if (indexedTask != taskIdsByPath.cend() &&
            indexedTask.value() == taskId) {
            taskIdsByPath.remove(record.path);
        }

        if (record.phase == TaskPhase::Queued && record.runnable &&
            pool->tryTake(record.runnable.data())) {
            record.runnable->deleteLater();
            removedTaskIds.append(taskId);
        } else {
            record.phase = TaskPhase::CancellationRequested;
        }
    }

    for (const quint64 taskId : std::as_const(removedTaskIds))
        tasks.remove(taskId);
}
