#pragma once
#include <QImage>
#include <QPainter>
#include <QPixmapCache>
#include <QDebug>
#include <memory>
#include <numbers>
#include <QElapsedTimer>
#include <QProcess>
#include "sourcecontainers/documentinfo.h"
#include "settings_types.h"


struct ColorMatrix {
    float m[3][3];
    float offset;
};


class ImageLib {
    public:
        static constexpr float kAdjustEpsilon = 0.001f;
        static constexpr double kPi = std::numbers::pi;

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
        static QImage loadICO(const QString &path);
};


// Compile-time bicubic weight calculator (Mitchell-Netravali, a = -0.5)
// constexpr (not consteval) so the functions are also callable at runtime
// with a fractional-pixel offset while still folding when given a
// constant-expression argument.
namespace BicubicWeights {
    constexpr float w0(float t) noexcept { return -0.5f*t*t*t +      t*t - 0.5f*t; }
    constexpr float w1(float t) noexcept { return  1.5f*t*t*t - 2.5f*t*t          + 1.0f; }
    constexpr float w2(float t) noexcept { return -1.5f*t*t*t + 2.0f*t*t + 0.5f*t; }
    constexpr float w3(float t) noexcept { return  0.5f*t*t*t - 0.5f*t*t; }
} // namespace BicubicWeights
