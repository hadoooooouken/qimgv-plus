#include "panoramagraphicsitem.h"
#include "utils/imagelib.h"
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QOpenGLWidget>
#include <QOpenGLContext>
#include <QMatrix3x3>
#include <QFile>
#include <QTextStream>
#include <cmath>

PanoramaGraphicsItem::PanoramaGraphicsItem(QGraphicsItem *parent)
    : QGraphicsObject(parent)
{
    setFlag(ItemHasNoContents, false);
}

PanoramaGraphicsItem::~PanoramaGraphicsItem()
{
    // QOpenGLTexture and QOpenGLShaderProgram need a current context to release GPU resources.
    // If no context is current, we release them to avoid calling their destructors (which would make glDelete* calls),
    // preventing potential driver hangs/crashes at the cost of a CPU memory leak.
    if (!QOpenGLContext::currentContext()) {
        mTexture.release();
        mProgram.release();
    }
}

void PanoramaGraphicsItem::setPixmap(std::shared_ptr<QPixmap> pixmap)
{
    mPixmap = pixmap;
    mTexture.reset();
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

void PanoramaGraphicsItem::setColorAdjustments(float exposure, float contrast, float brightness, float temperature, float tint, float saturation, float hue)
{
    mExposure = exposure;
    mContrast = contrast;
    mBrightness = brightness;
    mTemperature = temperature;
    mTint = tint;
    mSaturation = saturation;
    mHue = hue;
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
    
    mProgram = std::make_unique<QOpenGLShaderProgram>();
    const char *vsrc = 
        "attribute highp vec4 vertex;\n"
        "attribute highp vec2 texCoordAttr;\n"
        "varying highp vec2 texCoord;\n"
        "uniform highp mat4 matrix;\n"
        "void main() {\n"
        "   gl_Position = matrix * vertex;\n"
        "   texCoord = texCoordAttr;\n"
        "}\n";
        
    bool ok = true;
    if (!mProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, vsrc)) {
        qWarning() << "Panorama shader vertex error:" << mProgram->log();
        ok = false;
    }
    QFile fragFile(":/res/shaders/panorama.frag");
    QString fragSource;
    if (fragFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        fragSource = fragFile.readAll();
        fragFile.close();
    } else {
        qWarning() << "Panorama fragment shader load error: could not open resource file";
        ok = false;
    }

    if (ok) {
        QString prefix = QString("#define kAdjustEpsilon %1\n").arg(ImageLib::kAdjustEpsilon);
        fragSource.prepend(prefix);
        if (!mProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, fragSource)) {
            qWarning() << "Panorama shader fragment error:" << mProgram->log();
            ok = false;
        }
    }
    if (!mProgram->link()) {
        qWarning() << "Panorama shader link error:" << mProgram->log();
        ok = false;
    }
    
    mShaderFailed = !ok;
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
    if (mShaderFailed) {
        // Fallback (not really a panorama, just a flat image)
        painter->drawPixmap(0, 0, *mPixmap);
        return;
    }
    
    if (!mTexture) {
        mTexture = std::make_unique<QOpenGLTexture>(mPixmap->toImage());
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
    
    float yawRad = (float)(mYaw * ImageLib::kPi / 180.0);
    float pitchRad = (float)(mPitch * ImageLib::kPi / 180.0);
    float fovRad = (float)(mFov * ImageLib::kPi / 180.0);
    float aspect = (float)widget->width() / (float)widget->height();

    mProgram->setUniformValue("yaw", yawRad);
    mProgram->setUniformValue("pitch", pitchRad);
    mProgram->setUniformValue("fov", fovRad);
    mProgram->setUniformValue("aspect", aspect);

    ColorMatrix cm = ImageLib::getColorAdjustmentMatrix(mExposure, mContrast, mBrightness, mTemperature, mTint, mSaturation, mHue);
    float cmData[9] = {
        cm.m[0][0], cm.m[0][1], cm.m[0][2],
        cm.m[1][0], cm.m[1][1], cm.m[1][2],
        cm.m[2][0], cm.m[2][1], cm.m[2][2]
    };
    QMatrix3x3 colorMatrix(cmData);

    mProgram->setUniformValue("colorMatrix", colorMatrix);
    mProgram->setUniformValue("colorOffset", cm.offset);

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
