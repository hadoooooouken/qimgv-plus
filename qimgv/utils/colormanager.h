#ifndef COLORMANAGER_H
#define COLORMANAGER_H

#include <QImage>
#include <QColorSpace>

class ColorManager {
public:
    static void invalidateCache();
    static QColorSpace getTargetColorSpace();
    static QImage applyColorManagement(const QImage &srcImage);
};

#endif // COLORMANAGER_H
