#include "loader.h"

namespace {
constexpr int MaxLoaderThreads = 3;
}

Loader::Loader() {
    pool = new QThreadPool(this);
    pool->setMaxThreadCount(MaxLoaderThreads);
}

Loader::~Loader() {
    clearTasks();
    qDeleteAll(tasks);
    tasks.clear();
}

void Loader::clearTasks() {
    clearPool();
    pool->waitForDone();
}

bool Loader::isBusy() const {
    return (tasks.count() != 0);
}

bool Loader::isLoading(QString path) {
    return tasks.contains(path);
}

std::shared_ptr<Image> Loader::load(QString path) {
    return ImageFactory::createImage(path);
}

// clears all buffered tasks before loading
void Loader::loadAsyncPriority(QString path) {
    clearPool();
    doLoadAsync(path, 1);
}

void Loader::loadAsync(QString path) {
    doLoadAsync(path, 0);
}

void Loader::doLoadAsync(QString path, int priority) {
    if(tasks.contains(path)) {
        return;
    }

    auto runnable = new LoaderRunnable(path);
    runnable->setAutoDelete(false);
    tasks.insert(path, runnable);
    connect(runnable, &LoaderRunnable::finished, this, &Loader::onLoadFinished, Qt::UniqueConnection);
    pool->start(runnable, priority);
}

void Loader::onLoadFinished(std::shared_ptr<Image> image, const QString &path) {
    auto task = tasks.take(path);
    delete task;
    if(!image || !image->isLoaded())
        emit loadFailed(path);
    else
        emit loadFinished(image, path);
}

void Loader::clearPool() {
    auto keys = tasks.keys();
    for (const auto &key : keys) {
        if (tasks.contains(key)) {
            auto runnable = tasks.value(key);
            if (pool->tryTake(runnable)) {
                delete tasks.take(key);
            }
        }
    }
}
