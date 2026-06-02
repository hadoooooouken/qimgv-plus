#include "panoramagraphicsitem.h"
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QOpenGLWidget>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

PanoramaGraphicsItem::PanoramaGraphicsItem(QGraphicsItem *parent)
    : QGraphicsObject(parent)
{
    setFlag(ItemHasNoContents, false);
}

PanoramaGraphicsItem::~PanoramaGraphicsItem()
{
    if (mProgram) delete mProgram;
    if (mTexture) delete mTexture;
}

void PanoramaGraphicsItem::setPixmap(std::shared_ptr<QPixmap> pixmap)
{
    mPixmap = pixmap;
    if (mTexture) {
        delete mTexture;
        mTexture = nullptr;
    }
    prepareGeometryChange();
    update();
}

void PanoramaGraphicsItem::setViewParameters(float yaw, float pitch, float fov)
{
    mYaw = yaw;
    mPitch = pitch;
    mFov = fov;
    update();
}

void PanoramaGraphicsItem::setColorAdjustments(float brightness, float contrast, float saturation, float hue, float exposure, float temperature, float tint)
{
    mBrightness = brightness;
    mContrast = contrast;
    mSaturation = saturation;
    mHue = hue;
    mExposure = exposure;
    mTemperature = temperature;
    mTint = tint;
    update();
}

QRectF PanoramaGraphicsItem::boundingRect() const
{
    // Return a huge bounding rect to ensure it's always visible in the scene
    return QRectF(0, 0, 200000, 200000);
}

void PanoramaGraphicsItem::initShader()
{
    if (mInitialized) return;
    initializeOpenGLFunctions();
    
    mProgram = new QOpenGLShaderProgram();
    const char *vsrc = 
        "attribute highp vec4 vertex;\n"
        "attribute highp vec2 texCoordAttr;\n"
        "varying highp vec2 texCoord;\n"
        "uniform highp mat4 matrix;\n"
        "void main() {\n"
        "   gl_Position = matrix * vertex;\n"
        "   texCoord = texCoordAttr;\n"
        "}\n";
        
    mProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, vsrc);
    if (!mProgram->addShaderFromSourceFile(QOpenGLShader::Fragment, ":/res/shaders/panorama.frag")) {
        qDebug() << "Panorama shader fragment error:" << mProgram->log();
    }
    if (!mProgram->link()) {
        qDebug() << "Panorama shader link error:" << mProgram->log();
    }
    
    mInitialized = true;
}

void PanoramaGraphicsItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    if (!mPixmap || mPixmap->isNull()) return;

    QOpenGLWidget *glWidget = qobject_cast<QOpenGLWidget*>(widget);
    if (!glWidget) {
        // Fallback (not really a panorama, just a flat image)
        painter->drawPixmap(0, 0, *mPixmap);
        return;
    }

    initShader();
    
    if (!mTexture) {
        mTexture = new QOpenGLTexture(mPixmap->toImage());
        mTexture->setMagnificationFilter(QOpenGLTexture::Linear);
        mTexture->setMinificationFilter(QOpenGLTexture::Linear);
        mTexture->setWrapMode(QOpenGLTexture::Repeat); // Important for panoramas
    }

    painter->beginNativePainting();

    mProgram->bind();
    mTexture->bind();

    // Use identity matrix since we are using NDC vertices directly
    QMatrix4x4 matrix;
    mProgram->setUniformValue("matrix", matrix);
    mProgram->setUniformValue("tex", 0);
    
    float yawRad = (float)(mYaw * M_PI / 180.0);
    float pitchRad = (float)(mPitch * M_PI / 180.0);
    float fovRad = (float)(mFov * M_PI / 180.0);
    float aspect = (float)widget->width() / (float)widget->height();

    mProgram->setUniformValue("yaw", yawRad);
    mProgram->setUniformValue("pitch", pitchRad);
    mProgram->setUniformValue("fov", fovRad);
    mProgram->setUniformValue("aspect", aspect);

    mProgram->setUniformValue("brightness", mBrightness);
    mProgram->setUniformValue("contrast", mContrast);
    mProgram->setUniformValue("saturation", mSaturation);
    mProgram->setUniformValue("exposure", mExposure);
    mProgram->setUniformValue("temperature", mTemperature);
    mProgram->setUniformValue("tint", mTint);
    
    // Convert hue degrees to radians
    float hueRad = (float)(mHue * M_PI / 180.0);
    mProgram->setUniformValue("hue", hueRad);

    // Use NDC coordinates directly to fill the viewport
    GLfloat vertices[] = {
        -1.0f, -1.0f,
         1.0f, -1.0f,
        -1.0f,  1.0f,
         1.0f,  1.0f
    };
    // Use 0..1 coordinates and transform in shader to avoid issues
    GLfloat texCoords[] = {
        0.0f, 1.0f,
        1.0f, 1.0f,
        0.0f, 0.0f,
        1.0f, 0.0f
    };

    mProgram->enableAttributeArray("vertex");
    mProgram->setAttributeArray("vertex", GL_FLOAT, vertices, 2);
    mProgram->enableAttributeArray("texCoordAttr");
    mProgram->setAttributeArray("texCoordAttr", GL_FLOAT, texCoords, 2);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    mProgram->disableAttributeArray("vertex");
    mProgram->disableAttributeArray("texCoordAttr");

    mTexture->release();
    mProgram->release();

    painter->endNativePainting();
}
