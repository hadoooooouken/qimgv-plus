#pragma once

#include <QPixmap>
#include <QDebug>

enum ShrIcon {
    SHR_ICON_ERROR,
    SHR_ICON_LOADING
};

class SharedResources
{
public:
    SharedResources();
    static SharedResources* getInstance();

    // Disable copy
    SharedResources(const SharedResources&) = delete;
    SharedResources& operator=(const SharedResources&) = delete;

    ~SharedResources();

    QPixmap getPixmap(ShrIcon icon, qreal dpr);
};

extern SharedResources *shrRes;
