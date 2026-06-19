#pragma once

#include <QGraphicsObject>
#include <QOpenGLShaderProgram>
#include <QOpenGLFunctions>
#include <QOpenGLTexture>
#include <memory>
#include <QVector2D>
#include <QVector3D>

class PanoramaGraphicsItem : public QGraphicsObject, protected QOpenGLFunctions
{
    Q_OBJECT
public:
    explicit PanoramaGraphicsItem(QGraphicsItem *parent = nullptr);
    ~PanoramaGraphicsItem();

    void setImage(std::shared_ptr<const QImage> image);
    
    void setViewParameters(float yaw, float pitch, float fov);
    void setColorAdjustments(float exposure, float contrast, float brightness, float temperature, float tint, float saturation, float hue);
    
    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

private:
    void initShader();
    void releaseGlResources(bool forceRelease = false);
    class QOpenGLWidget* findGlWidget() const;
    
    std::shared_ptr<const QImage> mImage;
    std::unique_ptr<QOpenGLShaderProgram> mProgram;
    std::unique_ptr<QOpenGLTexture> mTexture;
    bool mInitialized = false;
    bool mShaderFailed = false;
    bool mTextureDirty = false;
    
    float mYaw = 0.0f;
    float mPitch = 0.0f;
    float mFov = 90.0f;

    float mExposure = 0.0f;
    float mContrast = 1.0f;
    float mBrightness = 0.0f;
    float mTemperature = 0.0f;
    float mTint = 0.0f;
    float mSaturation = 1.0f;
    float mHue = 0.0f;
};
