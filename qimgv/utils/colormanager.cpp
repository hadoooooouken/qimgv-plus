#include "colormanager.h"
#include "../settings.h"
#include <QMutex>
#include <QMutexLocker>
#include <QFile>
#include <QPointF>
#include <QDebug>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wingdi.h>
#endif

namespace {

class DefaultColorManager : public IColorManager {
public:
    void invalidateCache() override {
        QMutexLocker locker(&mutex);
        isCached = false;
        cachedTargetSpace = QColorSpace();
    }

    QColorSpace getTargetColorSpace() override {
        QMutexLocker locker(&mutex);
        if (isCached) {
            return cachedTargetSpace;
        }

        if (!settings) {
            qWarning() << "ColorManager: settings object is null, unable to load color profile.";
            return QColorSpace();
        }

        QString profileType = settings->monitorColorProfileType();
        QColorSpace targetSpace;

        if (profileType == "System") {
#ifdef _WIN32
            HDC hdc = GetDC(nullptr);
            bool success = false;
            if (hdc) {
                WCHAR profilePath[MAX_PATH];
                DWORD pathSize = MAX_PATH;
                if (GetICMProfileW(hdc, &pathSize, profilePath)) {
                    QString pathStr = QString::fromWCharArray(profilePath);
                    QFile file(pathStr);
                    if (file.open(QIODevice::ReadOnly)) {
                        targetSpace = QColorSpace::fromIccProfile(file.readAll());
                        if (targetSpace.isValid()) {
                            success = true;
                        } else {
                            qWarning() << "ColorManager: Loaded system profile from" << pathStr << "but it is not a valid QColorSpace.";
                        }
                    } else {
                        qWarning() << "ColorManager: Failed to open system profile file at" << pathStr;
                    }
                } else {
                    qWarning() << "ColorManager: GetICMProfileW failed with error code" << GetLastError();
                }
                ReleaseDC(nullptr, hdc);
            } else {
                qWarning() << "ColorManager: GetDC(nullptr) failed.";
            }
            if (!success) {
                targetSpace = QColorSpace(QColorSpace::SRgb);
                qWarning() << "ColorManager: Falling back to sRGB for System profile.";
            }
#else
            targetSpace = QColorSpace(QColorSpace::SRgb);
#endif
        } else if (profileType == "sRGB") {
            targetSpace = QColorSpace(QColorSpace::SRgb);
        } else if (profileType == "DisplayP3") {
            targetSpace = QColorSpace(QColorSpace::DisplayP3);
        } else if (profileType == "AdobeRGB") {
            targetSpace = QColorSpace(QColorSpace::AdobeRgb);
        } else if (profileType == "Rec2020") {
            targetSpace = QColorSpace(QPointF(0.3127, 0.3290), QPointF(0.708, 0.292), QPointF(0.170, 0.797), QPointF(0.131, 0.046), QColorSpace::TransferFunction::Gamma, 2.2f);
        } else if (profileType == "ProPhoto") {
            targetSpace = QColorSpace(QPointF(0.3457, 0.3586), QPointF(0.7347, 0.2653), QPointF(0.1596, 0.8404), QPointF(0.0366, 0.0000), QColorSpace::TransferFunction::Gamma, 1.8f);
        } else if (profileType == "LinearSRGB") {
            targetSpace = QColorSpace(QColorSpace::SRgbLinear);
        } else if (profileType == "Custom") {
            QString path = settings->monitorColorProfilePath();
            if (!path.isEmpty()) {
                QFile file(path);
                if (file.open(QIODevice::ReadOnly)) {
                    targetSpace = QColorSpace::fromIccProfile(file.readAll());
                    if (!targetSpace.isValid()) {
                        qWarning() << "ColorManager: Loaded custom profile from" << path << "but it is not a valid QColorSpace.";
                    }
                } else {
                    qWarning() << "ColorManager: Failed to open custom profile file at" << path;
                }
            } else {
                qWarning() << "ColorManager: Custom profile type selected but monitorColorProfilePath is empty.";
            }
        }

        cachedTargetSpace = targetSpace;
        isCached = true;
        return cachedTargetSpace;
    }

    QImage applyColorManagement(const QImage &srcImage) override {
        if (srcImage.isNull()) return srcImage;

        if (!settings || !settings->colorManagementEnabled()) {
            return srcImage;
        }

        QColorSpace targetSpace = getTargetColorSpace();

        if (targetSpace.isValid()) {
            QColorSpace srcSpace = srcImage.colorSpace();
            // If the source image doesn't have a valid color space, assume sRGB (industry standard)
            if (!srcSpace.isValid()) {
                srcSpace = QColorSpace(QColorSpace::SRgb);
            }
            if (srcSpace != targetSpace) {
                QImage img = srcImage;
                img.setColorSpace(srcSpace);
                return img.convertedToColorSpace(targetSpace);
            }
        }
        return srcImage;
    }

private:
    QColorSpace cachedTargetSpace;
    bool isCached = false;
    QMutex mutex;
};

// Thread-safe access to default instance
DefaultColorManager* defaultInstance() {
    static DefaultColorManager instance;
    return &instance;
}

IColorManager* g_customInstance = nullptr;
QMutex g_instanceMutex;

IColorManager* activeInstance() {
    QMutexLocker locker(&g_instanceMutex);
    if (g_customInstance) {
        return g_customInstance;
    }
    return defaultInstance();
}

} // namespace

void ColorManager::invalidateCache() {
    activeInstance()->invalidateCache();
}

QColorSpace ColorManager::getTargetColorSpace() {
    return activeInstance()->getTargetColorSpace();
}

QImage ColorManager::applyColorManagement(const QImage &srcImage) {
    return activeInstance()->applyColorManagement(srcImage);
}

void ColorManager::setInstance(IColorManager *newInstance) {
    QMutexLocker locker(&g_instanceMutex);
    g_customInstance = newInstance;
}
