#pragma once

#include <QGraphicsObject>
#include <QOpenGLShaderProgram>
#include <QOpenGLFunctions>
#include <QOpenGLTexture>
#include <memory>
#include <QVector2D>
#include <QVector3D>
#include "utils/coloradjustments.h"

class PanoramaGraphicsItem : public QGraphicsObject, protected QOpenGLFunctions
{
    Q_OBJECT
public:
    explicit PanoramaGraphicsItem(QGraphicsItem *parent = nullptr);
    ~PanoramaGraphicsItem();

    void setImage(std::shared_ptr<const QImage> image);
    
    void setViewParameters(float yaw, float pitch, float fov);
    void setColorAdjustments(const ColorAdjustments &adjustments);
    
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

    ColorAdjustments mColorAdjustments;
};
