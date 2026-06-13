#include "sharedresources.h"



SharedResources *shrRes = nullptr;

SharedResources::SharedResources()
{
}

SharedResources::~SharedResources() {
    if (shrRes == this) {
        shrRes = nullptr;
    }
}

QPixmap SharedResources::getPixmap(ShrIcon icon, qreal dpr) {
    QPixmap *pixmap = (icon == ShrIcon::SHR_ICON_ERROR) ? &mLoadingErrorIcon72 : &mLoadingIcon72;
    if(!pixmap->isNull())
        return *pixmap;

    QString path;
    if(icon == ShrIcon::SHR_ICON_ERROR) {
        path = ":/res/icons/common/other/loading-error72.png";
    } else {
        path = ":/res/icons/common/other/loading72.png";
    }

    qreal pixmapDrawScale;
    if(dpr >= (1.0 + 0.001)) {
        path.replace(".", "@2x.");
        *pixmap = QPixmap(path);
        if(dpr >= (2.0 - 0.001))
            pixmapDrawScale = dpr;
        else
            pixmapDrawScale = 2.0;
        pixmap->setDevicePixelRatio(pixmapDrawScale);
    } else {
        *pixmap = QPixmap(path);
    }
    return *pixmap;
}

SharedResources *SharedResources::getInstance() {
    if(!shrRes) {
        shrRes = new SharedResources();
    }
    return shrRes;
}
