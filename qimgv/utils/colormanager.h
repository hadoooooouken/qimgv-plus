#ifndef COLORMANAGER_H
#define COLORMANAGER_H

#include <QImage>
#include <QColorSpace>
#include <QGuiApplication>
#include <QScreen>
#include <QFile>
#include "../settings.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wingdi.h>
#endif

#include <QPointF>
#include <QMutex>
#include <QMutexLocker>

class ColorManager {
private:
    inline static QColorSpace cachedTargetSpace;
    inline static bool isCached = false;
    inline static QMutex mutex;

public:
    static void invalidateCache() {
        QMutexLocker locker(&mutex);
        isCached = false;
        cachedTargetSpace = QColorSpace();
    }

    static QColorSpace getTargetColorSpace() {
        QMutexLocker locker(&mutex);
        if (isCached) {
            return cachedTargetSpace;
        }

        QString profileType = settings->monitorColorProfileType();
        QColorSpace targetSpace;

        if (profileType == "System") {
#ifdef _WIN32
            HDC hdc = GetDC(NULL);
            if (hdc) {
                WCHAR profilePath[MAX_PATH];
                DWORD pathSize = MAX_PATH;
                if (GetICMProfileW(hdc, &pathSize, profilePath)) {
                    QFile file(QString::fromWCharArray(profilePath));
                    if (file.open(QIODevice::ReadOnly)) {
                        targetSpace = QColorSpace::fromIccProfile(file.readAll());
                    }
                }
                ReleaseDC(NULL, hdc);
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
                }
            }
        }

        cachedTargetSpace = targetSpace;
        isCached = true;
        return cachedTargetSpace;
    }

    static QImage applyColorManagement(const QImage &srcImage) {
        if (srcImage.isNull()) return srcImage;
        if (!settings->colorManagementEnabled()) {
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
};

#endif // COLORMANAGER_H
