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

    void setPixmap(std::shared_ptr<QPixmap> pixmap);
    
    void setViewParameters(float yaw, float pitch, float fov);
    void setColorAdjustments(float brightness, float contrast, float saturation, float hue, float exposure, float temperature, float tint);
    
    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    void initShader();
    
    std::shared_ptr<QPixmap> mPixmap;
    QOpenGLShaderProgram *mProgram = nullptr;
    QOpenGLTexture *mTexture = nullptr;
    bool mInitialized = false;
    
    float mYaw = 0.0f;
    float mPitch = 0.0f;
    float mFov = 90.0f;

    float mBrightness = 0.0f;
    float mContrast = 1.0f;
    float mSaturation = 1.0f;
    float mHue = 0.0f;
    float mExposure = 0.0f;
    float mTemperature = 0.0f;
    float mTint = 0.0f;
};
