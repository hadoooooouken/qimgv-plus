#include "filterpixmapitem.h"
#include "settings.h"
#include "utils/imagelib.h"
#include <QPainter>
#include <QOpenGLWidget>
#include <QMatrix4x4>
#include <QMatrix3x3>
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

void FilterPixmapItem::setColorAdjustments(float exposure, float contrast, float brightness, float temperature, float tint, float saturation, float hue) {
    mExposure = exposure;
    mContrast = contrast;
    mBrightness = brightness;
    mTemperature = temperature;
    mTint = tint;
    mSaturation = saturation;
    mHue = hue;
    update();
}

void FilterPixmapItem::setCasSettings(float sharpening, float contrast) {
    mCasSharpening = sharpening;
    mCasContrast = contrast;
    update();
}

void FilterPixmapItem::setScalingFilter(ScalingFilter filter) {
    mScalingFilter = filter;
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
        "uniform highp mat3 colorMatrix;\n"
        "uniform highp float colorOffset;\n"
        "\n"
        "uniform highp vec2 pixelSize;\n"
        "uniform highp float casContrast;\n"
        "uniform highp float casSharpening;\n"
        "uniform int sharpenMode;\n"
        "uniform int isDownscaling;\n"
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
        "vec3 applySmartSharpenGPU(vec2 uv) {\n"
        "    if (isDownscaling == 1) {\n"
        "        vec2 offX = vec2(pixelSize.x, 0.0);\n"
        "        vec2 offY = vec2(0.0, pixelSize.y);\n"
        "        float bias = -0.7;\n"
        "        vec3 center = texture2D(tex, uv, bias).rgb;\n"
        "        vec3 t1 = texture2D(tex, uv - 1.2 * offY, bias).rgb;\n"
        "        vec3 b1 = texture2D(tex, uv + 1.2 * offY, bias).rgb;\n"
        "        vec3 l1 = texture2D(tex, uv - 1.2 * offX, bias).rgb;\n"
        "        vec3 r1 = texture2D(tex, uv + 1.2 * offX, bias).rgb;\n"
        "        vec3 t2 = texture2D(tex, uv - 2.8 * offY, bias).rgb;\n"
        "        vec3 b2 = texture2D(tex, uv + 2.8 * offY, bias).rgb;\n"
        "        vec3 l2 = texture2D(tex, uv - 2.8 * offX, bias).rgb;\n"
        "        vec3 r2 = texture2D(tex, uv + 2.8 * offX, bias).rgb;\n"
        "        vec3 blurred = center * 0.17 + (t1 + b1 + l1 + r1) * 0.14 + (t2 + b2 + l2 + r2) * 0.0675;\n"
        "        vec3 sharpened = center + 0.18 * (center - blurred);\n"
        "        return clamp(sharpened, 0.0, 1.0);\n"
        "    } else {\n"
        "        vec2 offX = vec2(pixelSize.x, 0.0);\n"
        "        vec2 offY = vec2(0.0, pixelSize.y);\n"
        "        vec3 c = texture2D(tex, uv).rgb;\n"
        "        vec3 t = texture2D(tex, uv - offY).rgb;\n"
        "        vec3 b = texture2D(tex, uv + offY).rgb;\n"
        "        vec3 l = texture2D(tex, uv - offX).rgb;\n"
        "        vec3 r = texture2D(tex, uv + offX).rgb;\n"
        "        vec3 sharpened = c + (4.0 * c - t - b - l - r) * 0.0625;\n"
        "        return clamp(sharpened, 0.0, 1.0);\n"
        "    }\n"
        "}\n"
        "\n"
        "void main() {\n"
        "    highp vec4 color = texture2D(tex, texCoord);\n"
        "    highp vec3 rgb = color.rgb;\n"
        "    if (sharpenMode == 3 && casSharpening > 0.001) {\n"
        "        rgb = applyCAS(texCoord);\n"
        "    } else if (sharpenMode == 4) {\n"
        "        rgb = applySmartSharpenGPU(texCoord);\n"
        "    }\n"
        "    rgb = clamp(colorMatrix * rgb + vec3(colorOffset), 0.0, 1.0);\n"
        "    gl_FragColor = vec4(rgb, color.a);\n"
        "}\n";

    mProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, fsrc);

    if (!mProgram->link()) {
        qDebug() << "FilterPixmapItem shader link error:" << mProgram->log();
    }

    mInitialized = true;
}

void FilterPixmapItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    QTransform transform = painter->combinedTransform();
    double transScaleX = std::sqrt(transform.m11() * transform.m11() + transform.m12() * transform.m12());
    double transScaleY = std::sqrt(transform.m21() * transform.m21() + transform.m22() * transform.m22());
    if (transScaleX < 0.001) transScaleX = 1.0;
    if (transScaleY < 0.001) transScaleY = 1.0;
    bool isOneToOne = (qAbs(transScaleX - 1.0) < 0.001 && qAbs(transScaleY - 1.0) < 0.001);
    float activeCasSharpening = (isOneToOne && !settings->applyFilterAt100()) ? 0.0f : mCasSharpening;
    bool activeSmartGpu = (mScalingFilter == QI_FILTER_SMART_GPU);
    if (isOneToOne && !settings->applyFilterAt100()) {
        activeSmartGpu = false;
    }

    // 1. Fallback to default QGraphicsPixmapItem paint if there are no adjustments
    if (qAbs(mBrightness) < 0.001f && qAbs(mContrast - 1.0f) < 0.001f && qAbs(mSaturation - 1.0f) < 0.001f && qAbs(mHue) < 0.001f &&
        qAbs(mExposure) < 0.001f && qAbs(mTemperature) < 0.001f && qAbs(mTint) < 0.001f &&
        activeCasSharpening < 0.001f && !activeSmartGpu) {
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

    // Use trilinear filtering (MipMapLinear) for downscaling or active GPU Smart Sharpen
    QOpenGLTexture::Filter minFilter = filter;
    if (filter == QOpenGLTexture::Linear && (transScaleX < 0.999 || transScaleY < 0.999 || activeSmartGpu)) {
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
    ColorMatrix cm = ImageLib::getColorAdjustmentMatrix(mExposure, mContrast, mBrightness, mTemperature, mTint, mSaturation, mHue);
    float cmData[9] = {
        cm.m[0][0], cm.m[0][1], cm.m[0][2],
        cm.m[1][0], cm.m[1][1], cm.m[1][2],
        cm.m[2][0], cm.m[2][1], cm.m[2][2]
    };
    QMatrix3x3 colorMatrix(cmData);

    mProgram->setUniformValue("colorMatrix", colorMatrix);
    mProgram->setUniformValue("colorOffset", cm.offset);
    mProgram->setUniformValue("pixelSize", QVector2D(1.0f / (currentPixmap.width() * transScaleX), 1.0f / (currentPixmap.height() * transScaleY)));
    mProgram->setUniformValue("casContrast", mCasContrast);
    mProgram->setUniformValue("casSharpening", activeCasSharpening);
    mProgram->setUniformValue("sharpenMode", activeSmartGpu ? (int)QI_FILTER_SMART_GPU : (int)mScalingFilter);
    mProgram->setUniformValue("isDownscaling", (transScaleX < 0.999) ? 1 : 0);

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
