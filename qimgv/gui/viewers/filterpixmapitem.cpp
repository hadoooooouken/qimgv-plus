#include "filterpixmapitem.h"
#include "utils/imagelib.h"
#include <QPainter>
#include <QOpenGLWidget>
#include <QMatrix4x4>
#include <QMatrix3x3>
#include <QOpenGLContext>
#include <QFile>
#include <QTextStream>
#include <cmath>

FilterPixmapItem::FilterPixmapItem(QGraphicsItem *parent)
    : QGraphicsItem(parent)
{
}

FilterPixmapItem::~FilterPixmapItem() {
    // QOpenGLTexture and QOpenGLShaderProgram need a current context to release GPU resources.
    // If no context is current, we release them to avoid calling their destructors (which would make glDelete* calls),
    // preventing potential driver hangs/crashes at the cost of a CPU memory leak.
    if (!QOpenGLContext::currentContext()) {
        mTexture.release();
        mProgram.release();
    }
}

void FilterPixmapItem::setImage(const QImage &image) {
    prepareGeometryChange();
    mImage = image;
    update();
}

void FilterPixmapItem::setOffset(const QPointF &offset) {
    if (mOffset == offset) return;
    prepareGeometryChange();
    mOffset = offset;
    update();
}

void FilterPixmapItem::setOffset(qreal x, qreal y) {
    setOffset(QPointF(x, y));
}

void FilterPixmapItem::setTransformationMode(Qt::TransformationMode mode) {
    if (mTransformationMode == mode) return;
    mTransformationMode = mode;
    update();
}

QRectF FilterPixmapItem::boundingRect() const {
    if (mImage.isNull()) return QRectF();
    qreal dpr = mImage.devicePixelRatio();
    if (dpr <= 0.0) dpr = 1.0;
    return QRectF(mOffset, QSizeF(mImage.width() / dpr, mImage.height() / dpr));
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

void FilterPixmapItem::setApplyFilterAt100(bool enabled) {
    mApplyFilterAt100 = enabled;
}

void FilterPixmapItem::initShader() {
    if (mInitialized) return;
    initializeOpenGLFunctions();

    mProgram = std::make_unique<QOpenGLShaderProgram>();
    bool ok = true;
    if (!mProgram->addShaderFromSourceFile(QOpenGLShader::Vertex, ":/res/shaders/filter.vert")) {
        qWarning() << "FilterPixmapItem vertex shader error:" << mProgram->log();
        ok = false;
    }

    QFile fragFile(":/res/shaders/filter.frag");
    QString fragSource;
    if (fragFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        fragSource = fragFile.readAll();
        fragFile.close();
    } else {
        qWarning() << "FilterPixmapItem fragment shader load error: could not open resource file";
        ok = false;
    }

    if (ok) {
        QString prefix = QString("#define kAdjustEpsilon %1\n").arg(ImageLib::kAdjustEpsilon);
        fragSource.prepend(prefix);
        if (!mProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, fragSource)) {
            qWarning() << "FilterPixmapItem fragment shader error:" << mProgram->log();
            ok = false;
        }
    }

    if (!mProgram->link()) {
        qWarning() << "FilterPixmapItem shader link error:" << mProgram->log();
        ok = false;
    }

    mShaderFailed = !ok;
    mInitialized = true;
}

void FilterPixmapItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    QTransform transform = painter->combinedTransform();
    double transScaleX = std::sqrt(transform.m11() * transform.m11() + transform.m12() * transform.m12());
    double transScaleY = std::sqrt(transform.m21() * transform.m21() + transform.m22() * transform.m22());
    if (transScaleX < 0.001) transScaleX = 1.0;
    if (transScaleY < 0.001) transScaleY = 1.0;
    bool isOneToOne = (qAbs(transScaleX - 1.0) < 0.001 && qAbs(transScaleY - 1.0) < 0.001);
    float activeCasSharpening = (isOneToOne && !mApplyFilterAt100) ? 0.0f : mCasSharpening;
    bool activeSmartGpu = (mScalingFilter == QI_FILTER_SMART_GPU);
    if (isOneToOne && !mApplyFilterAt100) {
        activeSmartGpu = false;
    }

    auto fallbackPaint = [this](QPainter *p) {
        bool oldSmooth = p->renderHints() & QPainter::SmoothPixmapTransform;
        p->setRenderHint(QPainter::SmoothPixmapTransform, mTransformationMode == Qt::SmoothTransformation);
        p->drawImage(mOffset, mImage);
        p->setRenderHint(QPainter::SmoothPixmapTransform, oldSmooth);
    };

    // 1. Fallback to default paint if there are no adjustments
    if (qAbs(mBrightness) < ImageLib::kAdjustEpsilon && qAbs(mContrast - 1.0f) < ImageLib::kAdjustEpsilon && qAbs(mSaturation - 1.0f) < ImageLib::kAdjustEpsilon && qAbs(mHue) < ImageLib::kAdjustEpsilon &&
        qAbs(mExposure) < ImageLib::kAdjustEpsilon && qAbs(mTemperature) < ImageLib::kAdjustEpsilon && qAbs(mTint) < ImageLib::kAdjustEpsilon &&
        activeCasSharpening < ImageLib::kAdjustEpsilon && !activeSmartGpu) {
        fallbackPaint(painter);
        return;
    }

    if (mImage.isNull()) return;

    QOpenGLWidget *glWidget = qobject_cast<QOpenGLWidget*>(widget);
    if (!glWidget) {
        // Fallback if not rendering on an OpenGL viewport
        fallbackPaint(painter);
        return;
    }

    initShader();
    if (mShaderFailed) {
        fallbackPaint(painter);
        return;
    }

    GLint maxTexSize = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTexSize);
    if (maxTexSize > 0 && (mImage.width() > maxTexSize || mImage.height() > maxTexSize)) {
        fallbackPaint(painter);
        return;
    }

    bool needMips = (transScaleX < kDownscaleThreshold || transScaleY < kDownscaleThreshold || activeSmartGpu);
    bool canReuse = mTexture &&
                    mTexture->width() == mImage.width() &&
                    mTexture->height() == mImage.height() &&
                    (!needMips || mTexture->mipLevels() > 1);

    if (canReuse) {
        if (mLastImage.cacheKey() != mImage.cacheKey()) {
            mTexture->setData(mImage, needMips ? QOpenGLTexture::GenerateMipMaps : QOpenGLTexture::DontGenerateMipMaps);
            mLastImage = mImage;
        }
    } else {
        mTexture.reset();
        mTexture = std::make_unique<QOpenGLTexture>(mImage, needMips ? QOpenGLTexture::GenerateMipMaps : QOpenGLTexture::DontGenerateMipMaps);
        mTexture->setWrapMode(QOpenGLTexture::ClampToEdge);
        mLastImage = mImage;
    }

    // Match filtering to transformationMode
    QOpenGLTexture::Filter filter = (transformationMode() == Qt::SmoothTransformation)
                                    ? QOpenGLTexture::Linear
                                    : QOpenGLTexture::Nearest;
    mTexture->setMagnificationFilter(filter);

    // Use trilinear filtering (MipMapLinear) for downscaling or active GPU Smart Sharpen
    QOpenGLTexture::Filter minFilter = filter;
    if (filter == QOpenGLTexture::Linear && (transScaleX < kDownscaleThreshold || transScaleY < kDownscaleThreshold || activeSmartGpu)) {
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

    qreal dpr = mImage.devicePixelRatio();
    if (dpr <= 0.0) dpr = 1.0;
    qreal logicalWidth = mImage.width() / dpr;
    qreal logicalHeight = mImage.height() / dpr;

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
    mProgram->setUniformValue("pixelSize", QVector2D(1.0f / (logicalWidth * transScaleX), 1.0f / (logicalHeight * transScaleY)));
    mProgram->setUniformValue("casContrast", mCasContrast);
    mProgram->setUniformValue("casSharpening", activeCasSharpening);
    mProgram->setUniformValue("sharpenMode", activeSmartGpu ? (int)QI_FILTER_SMART_GPU : (int)mScalingFilter);
    mProgram->setUniformValue("isDownscaling", (transScaleX < kDownscaleThreshold) ? 1 : 0);

    float x1 = offset().x();
    float y1 = offset().y();
    float x2 = x1 + logicalWidth;
    float y2 = y1 + logicalHeight;

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
