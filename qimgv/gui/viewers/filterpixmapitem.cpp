#include "filterpixmapitem.h"
#include "settings.h"
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

void FilterPixmapItem::setCasSettings(float sharpening, float contrast) {
    mCasSharpening = sharpening;
    mCasContrast = contrast;
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
        "uniform highp vec2 pixelSize;\n"
        "uniform highp float casContrast;\n"
        "uniform highp float casSharpening;\n"
        "\n"
        "vec3 hueRotate(vec3 color, float angle) {\n"
        "    vec3 k = vec3(0.57735, 0.57735, 0.57735);\n"
        "    float cosAngle = cos(angle);\n"
        "    return color * cosAngle + cross(k, color) * sin(angle) + k * dot(k, color) * (1.0 - cosAngle);\n"
        "}\n"
        "\n"
        "vec3 applyCAS(vec2 uv) {\n"
        "    vec2 offX = vec2(pixelSize.x, 0.0);\n"
        "    vec2 offY = vec2(0.0, pixelSize.y);\n"
        "\n"
        "    vec3 e = texture2D(tex, uv).rgb;\n"
        "    vec3 b = texture2D(tex, uv - offY).rgb;\n"
        "    vec3 d = texture2D(tex, uv - offX).rgb;\n"
        "    vec3 f = texture2D(tex, uv + offX).rgb;\n"
        "    vec3 h = texture2D(tex, uv + offY).rgb;\n"
        "\n"
        "    vec3 a = texture2D(tex, uv - offX - offY).rgb;\n"
        "    vec3 c = texture2D(tex, uv + offX - offY).rgb;\n"
        "    vec3 g = texture2D(tex, uv - offX + offY).rgb;\n"
        "    vec3 i = texture2D(tex, uv + offX + offY).rgb;\n"
        "\n"
        "    vec3 mnRGB = min(min(min(d, e), min(f, b)), h);\n"
        "    vec3 mnRGB2 = min(mnRGB, min(min(a, c), min(g, i)));\n"
        "    mnRGB += mnRGB2;\n"
        "\n"
        "    vec3 mxRGB = max(max(max(d, e), max(f, b)), h);\n"
        "    vec3 mxRGB2 = max(mxRGB, max(max(a, c), max(g, i)));\n"
        "    mxRGB += mxRGB2;\n"
        "\n"
        "    vec3 rcpMRGB = 1.0 / mxRGB;\n"
        "    vec3 ampRGB = clamp(min(mnRGB, 2.0 - mxRGB) * rcpMRGB, 0.0, 1.0);\n"
        "    ampRGB = inversesqrt(ampRGB);\n"
        "\n"
        "    float peak = -3.0 * casContrast + 8.0;\n"
        "    vec3 wRGB = -1.0 / (ampRGB * peak);\n"
        "    vec3 rcpWeightRGB = 1.0 / (4.0 * wRGB + 1.0);\n"
        "\n"
        "    vec3 window = (b + d) + (f + h);\n"
        "    vec3 outColor = clamp((window * wRGB + e) * rcpWeightRGB, 0.0, 1.0);\n"
        "\n"
        "    return mix(e, outColor, casSharpening);\n"
        "}\n"
        "\n"
        "void main() {\n"
        "    highp vec4 color = texture2D(tex, texCoord);\n"
        "    highp vec3 rgb = (casSharpening > 0.001) ? applyCAS(texCoord) : color.rgb;\n"
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
    QTransform transform = painter->combinedTransform();
    double scaleX = transform.m11();
    double scaleY = transform.m22();
    bool isOneToOne = (qAbs(scaleX - 1.0) < 0.001 && qAbs(scaleY - 1.0) < 0.001);
    float activeCasSharpening = (isOneToOne && !settings->applyFilterAt100()) ? 0.0f : mCasSharpening;

    // 1. Fallback to default QGraphicsPixmapItem paint if there are no adjustments
    if (qAbs(mBrightness) < 0.001f && qAbs(mContrast - 1.0f) < 0.001f && qAbs(mSaturation - 1.0f) < 0.001f && qAbs(mHue) < 0.001f &&
        qAbs(mExposure) < 0.001f && qAbs(mTemperature) < 0.001f && qAbs(mTint) < 0.001f && activeCasSharpening < 0.001f) {
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
        // Generate mipmaps for smooth downscaling
        mTexture = new QOpenGLTexture(currentPixmap.toImage(), QOpenGLTexture::GenerateMipMaps);
        mTexture->setWrapMode(QOpenGLTexture::ClampToEdge);
        mLastPixmap = currentPixmap;
    }

    // Match filtering to transformationMode
    QOpenGLTexture::Filter filter = (transformationMode() == Qt::SmoothTransformation)
                                    ? QOpenGLTexture::Linear
                                    : QOpenGLTexture::Nearest;
    mTexture->setMagnificationFilter(filter);

    // Use trilinear filtering (MipMapLinear) for downscaling
    QOpenGLTexture::Filter minFilter = filter;
    if (filter == QOpenGLTexture::Linear && (scaleX < 0.999 || scaleY < 0.999)) {
        minFilter = QOpenGLTexture::LinearMipMapLinear;
    }
    mTexture->setMinificationFilter(minFilter);

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
    mProgram->setUniformValue("pixelSize", QVector2D(1.0f / currentPixmap.width(), 1.0f / currentPixmap.height()));
    mProgram->setUniformValue("casContrast", mCasContrast);
    mProgram->setUniformValue("casSharpening", activeCasSharpening);
    
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
