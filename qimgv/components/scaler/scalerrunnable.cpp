#include "scalerrunnable.h"

#include <QElapsedTimer>

ScalerRunnable::ScalerRunnable() {
}

void ScalerRunnable::setRequest(ScalerRequest r) {
    req = r;
}

void ScalerRunnable::run() {
    emit started(req);
    //QElapsedTimer t;
    //t.start();
    QImage *scaled = nullptr;
#ifdef USE_UPSCAYL
    if (settings->useUpscayl() && req.size.width() > req.image->width()) {
        // Skip CPU scaling when AI upscaling is active and we are zooming in
        scaled = new QImage();
    } else
#endif
    {
        if(req.filter == 0) {
            scaled = ImageLib::scaled(req.image->getImage(), req.size, QI_FILTER_NEAREST);
        } else {
            scaled = ImageLib::scaled(req.image->getImage(), req.size, req.filter);
        }
    }
    //qDebug() << ">> " << req.size << ": " << t.elapsed();
    emit finished(scaled, req);
}
