#pragma once
#include <QImage>
#include <QPainter>
#include <QPixmapCache>
#include <QDebug>
#include <memory>
#include <QElapsedTimer>
#include <QProcess>
#include "sourcecontainers/documentinfo.h"
#include "settings.h"


struct ColorMatrix {
    float m[3][3];
    float offset;
};


class ImageLib {
    public:
        static constexpr float kAdjustEpsilon = 0.001f;
        static constexpr double kPi = 3.14159265358979323846;

        static QImage rotatedRaw(const QImage *src, int grad);
        static QImage rotated(std::shared_ptr<const QImage> src, int grad);

        static QImage croppedRaw(const QImage *src, QRect newRect);
        static QImage cropped(std::shared_ptr<const QImage> src, QRect newRect);
        // Using global ScalingFilter from settings.h

        static QImage flippedHRaw(const QImage *src);
        static QImage flippedH(std::shared_ptr<const QImage> src);

        static QImage flippedVRaw(const QImage *src);
        static QImage flippedV(std::shared_ptr<const QImage> src);

        //static QImage *scaled(const QImage *source, QSize destSize, ScalingFilter filter);
        static QImage scaled(std::shared_ptr<const QImage> source, QSize destSize, ScalingFilter filter);

        static QImage scaled_Qt(std::shared_ptr<const QImage> source, QSize destSize, bool smooth);

        static QImage scaled_Smart(std::shared_ptr<const QImage> source, QSize destSize);

        static std::unique_ptr<const QImage> exifRotated(std::unique_ptr<const QImage> src, int orientation);
        static std::unique_ptr<QImage> exifRotated(std::unique_ptr<QImage> src, int orientation);
        static void recolor(QPixmap &pixmap, QColor color);
        static ColorMatrix getColorAdjustmentMatrix(float exposure, float contrast, float brightness, float temperature, float tint, float saturation, float hue);
        static QImage applyColorAdjustments(std::shared_ptr<const QImage> source, float exposure, float contrast, float brightness, float temperature, float tint, float saturation, float hue);
};

