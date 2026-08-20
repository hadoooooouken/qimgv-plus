#include "loaderrunnable.h"

#include <QElapsedTimer>

#include <utility>

void LoaderTaskNotifier::reportCompletion(ImageLoadCompletion completion) {
    emit taskCompleted(std::move(completion));
}

LoaderRunnable::LoaderRunnable(ImageLoadRequest request,
                               LoaderTaskNotifier &notifier)
    : request(std::move(request)), notifier(notifier) {
    completion.taskId = this->request.taskId;
    setAutoDelete(true);
}

LoaderRunnable::~LoaderRunnable() {
    notifier.reportCompletion(std::move(completion));
}

void LoaderRunnable::run() {
    //QElapsedTimer t;
    //t.start();
    completion.image =
        ImageFactory::createImage(request.path, request.decodeContext);
    completion.status = ImageLoadCompletionStatus::Finished;
    //qDebug() << "L: " << t.elapsed();
}
