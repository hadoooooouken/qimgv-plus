#include "scalerrunnable.h"
#include "settings.h"
#include "utils/colormanager.h"

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
    QImage scaled;
    if (settings->useUpscayl() && req.size.width() > req.image->width()) {
        // Skip CPU scaling when AI upscaling is active and we are zooming in
        scaled = QImage();
    } else
    {
        if(req.filter == 0) {
            scaled = ImageLib::scaled(req.image->getImage(), req.size, QI_FILTER_NEAREST);
        } else {
            scaled = ImageLib::scaled(req.image->getImage(), req.size, req.filter);
        }
    }
    scaled = ColorManager::applyColorManagement(scaled);
    //qDebug() << ">> " << req.size << ": " << t.elapsed();
    emit finished(scaled, req);
}
