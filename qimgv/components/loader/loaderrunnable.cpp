#include "loaderrunnable.h"

#include <QElapsedTimer>

#include <utility>

LoaderRunnable::LoaderRunnable(ImageLoadRequest request, QObject *parent)
    : QObject(parent), request(std::move(request)) {
}

void LoaderRunnable::run() {
    //QElapsedTimer t;
    //t.start();
    auto image = ImageFactory::createImage(request.path, request.decodeContext);
    //qDebug() << "L: " << t.elapsed();
    emit finished(request.taskId, std::move(image));
}
