#ifndef COLORMANAGER_H
#define COLORMANAGER_H

#include <QImage>
#include <QColorSpace>

class IColorManager {
public:
    virtual ~IColorManager() = default;
    virtual void invalidateCache() = 0;
    virtual QColorSpace getTargetColorSpace() = 0;
    virtual QImage applyColorManagement(const QImage &srcImage) = 0;
};

class ColorManager {
public:
    static void invalidateCache();
    static QColorSpace getTargetColorSpace();
    static QImage applyColorManagement(const QImage &srcImage);

    // Dependency injection interface
    static void setInstance(IColorManager *newInstance);
};

#endif // COLORMANAGER_H
