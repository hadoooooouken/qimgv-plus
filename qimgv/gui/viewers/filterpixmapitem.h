#pragma once

#include <QGraphicsItem>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QImage>
#include <memory>
#include "settings_types.h"

class QOpenGLFramebufferObject;

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

    // Tells the item whether the view is currently settled (not actively
    // panning/zooming/resizing). Only while settled will the item spend the
    // extra one-shot GPU pass to build an exact-ratio downsample (see
    // buildPreciseDownsample()) instead of relying purely on hardware
    // box-mip trilinear minification. Driven by ImageViewerV2's existing
    // settle tracking; false by default (today's behavior, unchanged).
    void setSettled(bool settled);

    QRectF boundingRect() const override;

protected:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

private:
    static constexpr float kDownscaleThreshold = 0.999f;
    static constexpr double kMinimumTransformScale = 0.001;
    static constexpr double kOneToOneScaleTolerance = 0.001;
    static constexpr qreal kMinimumDevicePixelRatio = 1.0;

    bool mSettled = false;

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

    // Settle-triggered exact-ratio GPU box downsample (see
    // buildPreciseDownsample()). Rebuilt lazily in paint() whenever it goes
    // stale (target size or source image changed) while mSettled is true.
    // While not settled, or if it has never been built for the current
    // scale, paint() falls back to mTexture's ordinary box-mip/trilinear
    // minification exactly as before this feature existed.
    bool mReduceProgramFailed = false;
    std::unique_ptr<QOpenGLShaderProgram> mReduceProgram;
    std::unique_ptr<QOpenGLFramebufferObject> mPreciseDownsampleFbo;
    qint64 mPreciseDownsampleSourceCacheKey = -1;

    QImage mImage;
    // Premultiplied-alpha copy of mImage, kept in sync in setImage(). Both the
    // GL texture upload and the CPU fallbackPaint() smooth draw use this
    // instead of mImage directly: interpolating straight (non-premultiplied)
    // alpha lets RGB baked into fully-transparent source pixels bleed a
    // dark/light fringe into opaque neighbors, while premultiplied alpha
    // makes those transparent pixels exactly (0,0,0,0). Left null for images
    // with no alpha channel, where premultiplication is a no-op.
    QImage mImagePremultiplied;
    QImage mLastImage;
    QPointF mOffset;
    Qt::TransformationMode mTransformationMode = Qt::SmoothTransformation;

    void initShader();
    void ensureReduceProgram();
    // Uploads mImagePremultiplied to mTexture as raw premultiplied bytes,
    // bypassing QOpenGLTexture's QImage-based overloads (see .cpp for why:
    // they always convert to straight-alpha Format_RGBA8888 before upload).
    void uploadPremultipliedTexture(bool generateMips);
    void buildPreciseDownsample(int targetW, int targetH);
    void releaseGlResources(bool forceRelease = false);
    class QOpenGLWidget* findGlWidget() const;
};
