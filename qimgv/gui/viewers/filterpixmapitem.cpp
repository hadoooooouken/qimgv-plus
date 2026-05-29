#include "filterpixmapitem.h"
#include <QPainter>
#include <QOpenGLWidget>
#include <QMatrix4x4>
#include <QOpenGLContext>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

FilterPixmapItem::FilterPixmapItem(QGraphicsItem *parent)
    : QGraphicsPixmapItem(parent)
{
}

FilterPixmapItem::~FilterPixmapItem() {
    if (QOpenGLContext::currentContext()) {
        if (mProgram) delete mProgram;
        if (mTexture) delete mTexture;
    }
}

void FilterPixmapItem::setColorAdjustments(float brightness, float contrast, float saturation, float hue, float exposure, float temperature, float tint) {
    mBrightness = brightness;
    mContrast = contrast;
    mSaturation = saturation;
    mHue = hue;
    mExposure = exposure;
    mTemperature = temperature;
    mTint = tint;
    update();
}

void FilterPixmapItem::initShader() {
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

    const char *fsrc =
        "varying highp vec2 texCoord;\n"
        "uniform sampler2D tex;\n"
        "uniform highp float brightness;\n"
        "uniform highp float contrast;\n"
        "uniform highp float saturation;\n"
        "uniform highp float hue;\n"
        "uniform highp float exposure;\n"
        "uniform highp float temperature;\n"
        "uniform highp float tint;\n"
        "\n"
        "vec3 hueRotate(vec3 color, float angle) {\n"
        "    vec3 k = vec3(0.57735, 0.57735, 0.57735);\n"
        "    float cosAngle = cos(angle);\n"
        "    return color * cosAngle + cross(k, color) * sin(angle) + k * dot(k, color) * (1.0 - cosAngle);\n"
        "}\n"
        "\n"
        "void main() {\n"
        "    highp vec4 color = texture2D(tex, texCoord);\n"
        "    highp vec3 rgb = color.rgb;\n"
        "    if (abs(temperature) > 0.001 || abs(tint) > 0.001) {\n"
        "        rgb.r *= (1.0 + temperature + tint * 0.5);\n"
        "        rgb.g *= (1.0 - tint);\n"
        "        rgb.b *= (1.0 - temperature + tint * 0.5);\n"
        "    }\n"
        "    if (abs(exposure) > 0.001) {\n"
        "        rgb *= pow(2.0, exposure);\n"
        "    }\n"
        "    if (abs(hue) > 0.001) {\n"
        "        rgb = hueRotate(rgb, hue);\n"
        "    }\n"
        "    if (abs(saturation - 1.0) > 0.001) {\n"
        "        highp float gray = dot(rgb, vec3(0.2126, 0.7152, 0.0722));\n"
        "        rgb = mix(vec3(gray), rgb, saturation);\n"
        "    }\n"
        "    rgb += vec3(brightness);\n"
        "    rgb = (rgb - vec3(0.5)) * contrast + vec3(0.5);\n"
        "    gl_FragColor = vec4(clamp(rgb, 0.0, 1.0), color.a);\n"
        "}\n";

    mProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, fsrc);

    if (!mProgram->link()) {
        qDebug() << "FilterPixmapItem shader link error:" << mProgram->log();
    }

    mInitialized = true;
}

void FilterPixmapItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    // 1. Fallback to default QGraphicsPixmapItem paint if there are no adjustments
    if (qAbs(mBrightness) < 0.001f && qAbs(mContrast - 1.0f) < 0.001f && qAbs(mSaturation - 1.0f) < 0.001f && qAbs(mHue) < 0.001f &&
        qAbs(mExposure) < 0.001f && qAbs(mTemperature) < 0.001f && qAbs(mTint) < 0.001f) {
        QGraphicsPixmapItem::paint(painter, option, widget);
        return;
    }

    QPixmap currentPixmap = pixmap();
    if (currentPixmap.isNull()) return;

    QOpenGLWidget *glWidget = qobject_cast<QOpenGLWidget*>(widget);
    if (!glWidget) {
        // Fallback if not rendering on an OpenGL viewport
        QGraphicsPixmapItem::paint(painter, option, widget);
        return;
    }

    initShader();

    if (!mTexture || mLastPixmap.cacheKey() != currentPixmap.cacheKey()) {
        if (mTexture) {
            delete mTexture;
            mTexture = nullptr;
        }
        mTexture = new QOpenGLTexture(currentPixmap.toImage());
        mTexture->setWrapMode(QOpenGLTexture::ClampToEdge);
        mLastPixmap = currentPixmap;
    }

    // Match filtering to transformationMode
    QOpenGLTexture::Filter filter = (transformationMode() == Qt::SmoothTransformation)
                                    ? QOpenGLTexture::Linear
                                    : QOpenGLTexture::Nearest;
    mTexture->setMagnificationFilter(filter);
    mTexture->setMinificationFilter(filter);

    painter->beginNativePainting();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    mProgram->bind();
    mTexture->bind();

    QMatrix4x4 modelview(painter->combinedTransform());
    QMatrix4x4 projection;
    projection.ortho(0, glWidget->width(), glWidget->height(), 0, -1.0, 1.0);
    QMatrix4x4 matrix = projection * modelview;

    mProgram->setUniformValue("matrix", matrix);
    mProgram->setUniformValue("tex", 0);
    mProgram->setUniformValue("brightness", mBrightness);
    mProgram->setUniformValue("contrast", mContrast);
    mProgram->setUniformValue("saturation", mSaturation);
    mProgram->setUniformValue("exposure", mExposure);
    mProgram->setUniformValue("temperature", mTemperature);
    mProgram->setUniformValue("tint", mTint);
    
    // Convert hue degrees to radians
    float hueRad = (float)(mHue * M_PI / 180.0);
    mProgram->setUniformValue("hue", hueRad);

    float x1 = offset().x();
    float y1 = offset().y();
    float x2 = x1 + currentPixmap.width();
    float y2 = y1 + currentPixmap.height();

    GLfloat vertices[] = {
        x1, y1,
        x2, y1,
        x1, y2,
        x2, y2
    };
    GLfloat texCoords[] = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 1.0f
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
