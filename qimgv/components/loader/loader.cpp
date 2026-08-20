#include "loader.h"

#include <memory>
#include <utility>

#include <QDebug>

namespace {
constexpr int MaxLoaderThreads = 3;
}

Loader::Loader() {
    pool = new QThreadPool(this);
    pool->setMaxThreadCount(MaxLoaderThreads);
    qRegisterMetaType<ImageLoadCompletion>();
    connect(&taskNotifier, &LoaderTaskNotifier::taskCompleted, this,
            &Loader::onTaskCompleted, Qt::QueuedConnection);
}

Loader::~Loader() {
    clearTasks();
}

void Loader::clearTasks() {
    cancelTasks();
    pool->waitForDone();
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
    ImageLoadRequest request;
    request.path = path;
    request.taskId = taskId;
    request.decodeContext.cancellationToken =
        mCancellationSource.get_token();

    auto runnable =
        std::make_unique<LoaderRunnable>(std::move(request), taskNotifier);

    taskIdsByPath.insert(path, taskId);
    tasks.insert(taskId, TaskRecord{path});
    pool->start(runnable.release(), priority);
}

void Loader::onTaskCompleted(ImageLoadCompletion completion) {
    auto task = tasks.find(completion.taskId);
    if (task == tasks.end())
        return;

    const QString path = task->path;
    const bool cancelled = task->cancellationRequested;
    const auto indexedTask = taskIdsByPath.constFind(path);
    if (indexedTask != taskIdsByPath.cend() &&
        indexedTask.value() == completion.taskId) {
        taskIdsByPath.remove(path);
    }
    tasks.erase(task);

    if (cancelled)
        return;

    if (completion.status != ImageLoadCompletionStatus::Finished) {
        qWarning() << "Loader task was removed before execution" << path;
        emit loadFailed(path);
        return;
    }

    if(!completion.image || !completion.image->isLoaded())
        emit loadFailed(path);
    else
        emit loadFinished(std::move(completion.image), path);
}

quint64 Loader::nextTaskId()
{
    do {
        ++mNextTaskId;
    } while (tasks.contains(mNextTaskId));
    return mNextTaskId;
}

void Loader::cancelTasks() {
    for (TaskRecord &record : tasks)
        record.cancellationRequested = true;
    taskIdsByPath.clear();

    mCancellationSource.request_stop();
    pool->clear();
    mCancellationSource = std::stop_source{};
}
