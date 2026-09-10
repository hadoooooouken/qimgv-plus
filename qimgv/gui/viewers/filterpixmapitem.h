#pragma once

#include <QGraphicsItem>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QImage>
#include <memory>
#include "settings_types.h"

class FilterPixmapItem : public QGraphicsItem, protected QOpenGLFunctions {
public:
    explicit FilterPixmapItem(QGraphicsItem *parent = nullptr);
    ~FilterPixmapItem();

    void setImage(const QImage &image);
    QImage image() const { return mImage; }

    void setOffset(const QPointF &offset);
    void setOffset(qreal x, qreal y);
    QPointF offset() const { return mOffset; }

    void setTransformationMode(Qt::TransformationMode mode);
    Qt::TransformationMode transformationMode() const { return mTransformationMode; }

    void setColorAdjustments(float exposure, float contrast, float brightness, float temperature, float tint, float saturation, float hue);
    void setCasSettings(float sharpening, float contrast);
    void setScalingFilter(ScalingFilter filter);

    QRectF boundingRect() const override;

protected:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

private:
    static constexpr float kDownscaleThreshold = 0.999f;
    static constexpr double kMinimumTransformScale = 0.001;
    static constexpr double kOneToOneScaleTolerance = 0.001;
    static constexpr qreal kMinimumDevicePixelRatio = 1.0;

    float mExposure = 0.0f;    // -3.0f to 3.0f
    float mContrast = 1.0f;   // 0.0f to 3.0f
    float mBrightness = 0.0f; // -1.0f to 1.0f
    float mTemperature = 0.0f; // -0.5f to 0.5f
    float mTint = 0.0f;        // -0.5f to 0.5f
    float mSaturation = 1.0f; // 0.0f to 2.0f
    float mHue = 0.0f;        // -180.0f to 180.0f (degrees)
    float mCasSharpening = 0.0f;
    float mCasContrast = 0.0f;
    ScalingFilter mScalingFilter = QI_FILTER_BILINEAR;

    bool mInitialized = false;
    bool mShaderFailed = false;
    std::unique_ptr<QOpenGLShaderProgram> mProgram;
    std::unique_ptr<QOpenGLTexture> mTexture;

    QImage mImage;
    // Premultiplied-alpha copy of mImage, kept in sync in setImage(). Used by
    // the CPU fallbackPaint() smooth draw, and by the GL texture upload for
    // every image except alpha-bearing FP16 ones (see mImagePremultipliedFp16
    // below): interpolating straight (non-premultiplied) alpha lets RGB baked
    // into fully-transparent source pixels bleed a dark/light fringe into
    // opaque neighbors, while premultiplied alpha makes those transparent
    // pixels exactly (0,0,0,0). Left null for images with no alpha channel,
    // where premultiplication is a no-op. Always Format_ARGB32_Premultiplied,
    // even when mImage is FP16 - this copy exists purely for the CPU raster
    // fallback path, which is not part of the precision this class otherwise
    // protects for FP16 sources (see setImage()).
    QImage mImagePremultiplied;
    // FP16-precision counterpart of mImagePremultiplied, populated only when
    // mImage is an alpha-bearing FP16 format (Format_RGBA16FPx4 /
    // _Premultiplied). Used as the GL upload source instead of
    // mImagePremultiplied so alpha-bearing HDR images never pass through an
    // 8-bit premultiply step before reaching the GPU. Always
    // Format_RGBA16FPx4_Premultiplied when non-null. Left null whenever
    // mImage is not an alpha-bearing FP16 image.
    QImage mImagePremultipliedFp16;
    QImage mLastImage;
    QPointF mOffset;
    Qt::TransformationMode mTransformationMode = Qt::SmoothTransformation;

    void initShader();
    void releaseGlResources(bool forceRelease = false);
    class QOpenGLWidget* findGlWidget() const;
};
