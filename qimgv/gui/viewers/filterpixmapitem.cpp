#include "filterpixmapitem.h"
#include "utils/imagelib.h"
#include <QPainter>
#include <QOpenGLWidget>
#include <QMatrix4x4>
#include <QMatrix3x3>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QFile>
#include <QTextStream>
#include <cmath>
#include <QGraphicsScene>
#include <QGraphicsView>

FilterPixmapItem::FilterPixmapItem(QGraphicsItem *parent)
    : QGraphicsItem(parent)
{
}

FilterPixmapItem::~FilterPixmapItem() {
    releaseGlResources(true);

    if (mProgram || mReduceProgram) {
        // QOpenGLShaderProgram needs a current context to release GPU resources.
        // If no context is current, we try to make it current. If that still fails,
        // we release ownership to avoid calling its destructor (preventing potential
        // driver hangs/crashes at the cost of a CPU memory leak).
        if (!QOpenGLContext::currentContext()) {
            if (auto *glWidget = findGlWidget()) {
                glWidget->makeCurrent();
            }
        }
        if (QOpenGLContext::currentContext()) {
            mProgram.reset();
            mReduceProgram.reset();
        } else {
            mProgram.release();
            mReduceProgram.release();
        }
    }
}

void FilterPixmapItem::setImage(const QImage &image) {
    prepareGeometryChange();
    mImage = image;
    mImagePremultiplied = mImage.hasAlphaChannel()
                               ? mImage.convertToFormat(QImage::Format_ARGB32_Premultiplied)
                               : QImage();
    if (mImage.isNull()) {
        releaseGlResources(false);
    }
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

void FilterPixmapItem::setSettled(bool settled) {
    if (mSettled == settled) return;
    mSettled = settled;
    update();
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

void FilterPixmapItem::ensureReduceProgram() {
    if (mReduceProgram || mReduceProgramFailed) return;

    mReduceProgram = std::make_unique<QOpenGLShaderProgram>();
    bool ok = true;
    if (!mReduceProgram->addShaderFromSourceFile(QOpenGLShader::Vertex, ":/res/shaders/boxreduce.vert")) {
        qWarning() << "FilterPixmapItem box-reduce vertex shader error:" << mReduceProgram->log();
        ok = false;
    }
    if (ok && !mReduceProgram->addShaderFromSourceFile(QOpenGLShader::Fragment, ":/res/shaders/boxreduce.frag")) {
        qWarning() << "FilterPixmapItem box-reduce fragment shader error:" << mReduceProgram->log();
        ok = false;
    }
    if (ok && !mReduceProgram->link()) {
        qWarning() << "FilterPixmapItem box-reduce shader link error:" << mReduceProgram->log();
        ok = false;
    }

    if (!ok) {
        mReduceProgram.reset();
        mReduceProgramFailed = true;
    }
}

// Builds a texture whose resolution already matches the current on-screen
// footprint (targetW x targetH), via a chain of exact-area GPU box-reduction
// passes read directly from the full-resolution mTexture (level 0).
//
// Why not just rely on mTexture's hardware trilinear mip chain (as paint()
// otherwise does)? Trilinear only blends two discrete, power-of-two-spaced
// mip levels. That is an accurate reconstruction exactly at those spacings
// and a progressively worse approximation in between, which shows up as
// phase-dependent stair-stepping on thin, high-contrast diagonal detail at
// non-power-of-two zoom ratios -- the same artifact regardless of whether
// CAS, Smart GPU, or no sharpening at all is layered on top, since they all
// read from that same mip chain. Each pass here instead halves resolution
// (or, on the final pass per axis, resamples the remaining fractional
// ratio) with a true exact-area box average, which composes losslessly
// across passes and matches the on-screen footprint exactly rather than
// snapping to the nearest power-of-two level.
//
// This is deliberately gated to only run once the view is settled (see
// setSettled()): each call costs a handful of full-screen-quad draws, so
// running it every frame during an interactive zoom/pan would add a new,
// unnecessary per-frame GPU cost that does not exist today. While
// interacting, paint() keeps using the ordinary mip/trilinear path below,
// exactly as before this feature existed.
void FilterPixmapItem::buildPreciseDownsample(int targetW, int targetH) {
    ensureReduceProgram();
    if (!mReduceProgram || !mTexture) {
        mPreciseDownsampleFbo.reset();
        return;
    }

    const int srcW = mTexture->width();
    const int srcH = mTexture->height();
    if (srcW <= 0 || srcH <= 0 || targetW <= 0 || targetH <= 0) {
        mPreciseDownsampleFbo.reset();
        return;
    }

    static constexpr GLfloat kFullscreenQuadVertices[] = {
        -1.0f, -1.0f,
         1.0f, -1.0f,
        -1.0f,  1.0f,
         1.0f,  1.0f
    };
    static constexpr GLfloat kFullscreenQuadTexCoords[] = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 1.0f
    };

    GLint previousFbo = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFbo);
    GLint previousViewport[4] = {0, 0, 0, 0};
    glGetIntegerv(GL_VIEWPORT, previousViewport);
    const GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
    glDisable(GL_BLEND);

    // The reduction loop below also touches the active texture unit (it
    // always samples from GL_TEXTURE0) and, on its very first iteration,
    // reads directly from mTexture -- which means it overwrites mTexture's
    // *own* min/mag/wrap texture-object parameters (to GL_NEAREST /
    // GL_CLAMP_TO_EDGE) via raw glTexParameteri calls, not through
    // QOpenGLTexture, so Qt's cached idea of those parameters would
    // otherwise silently drift from the real GL state. Save what's bound on
    // unit 0 now and mTexture's real parameters before they get clobbered,
    // so both can be put back exactly once the loop is done, rather than
    // just trusting a later high-level setMagnificationFilter()/
    // setMinificationFilter() call (paint() still does that too, as a
    // defensive backup, but it must not be the only safeguard: it only runs
    // when the precise texture is rebuilt, while a stale/failed rebuild can
    // still leave mTexture's real GL parameters corrupted for the ordinary
    // mTexture->bind() path used the rest of the time).
    GLint previousActiveTexture = GL_TEXTURE0;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &previousActiveTexture);
    glActiveTexture(GL_TEXTURE0);
    GLint previousBoundTexture = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousBoundTexture);

    GLint savedMinFilter = GL_LINEAR;
    GLint savedMagFilter = GL_LINEAR;
    GLint savedWrapS = GL_CLAMP_TO_EDGE;
    GLint savedWrapT = GL_CLAMP_TO_EDGE;
    glBindTexture(GL_TEXTURE_2D, mTexture->textureId());
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &savedMinFilter);
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &savedMagFilter);
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &savedWrapS);
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, &savedWrapT);

    QOpenGLFramebufferObjectFormat fboFormat;
    fboFormat.setInternalTextureFormat(GL_RGBA8);

    std::unique_ptr<QOpenGLFramebufferObject> currentFbo;
    GLuint currentSourceTexId = mTexture->textureId();
    int curW = srcW;
    int curH = srcH;
    bool ok = true;

    mReduceProgram->bind();
    mReduceProgram->setUniformValue("srcTex", 0);

    // Each iteration reduces whichever axis hasn't reached its target yet by
    // at most half; an axis that already reached its target is left alone
    // (ratio 1.0, a no-op pass for that axis) so non-uniform X/Y scale still
    // converges cleanly. The last touch of an axis lands on a ratio in
    // (0.5, 1.0], which is exactly the range res/shaders/boxreduce.frag is
    // built to resample correctly in one pass.
    while (ok && (curW != targetW || curH != targetH)) {
        const int nextW = (curW == targetW) ? targetW : qMax(targetW, curW / 2);
        const int nextH = (curH == targetH) ? targetH : qMax(targetH, curH / 2);

        auto nextFbo = std::make_unique<QOpenGLFramebufferObject>(nextW, nextH, fboFormat);
        if (!nextFbo->isValid()) {
            ok = false;
            break;
        }
        nextFbo->bind();
        glViewport(0, 0, nextW, nextH);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, currentSourceTexId);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        mReduceProgram->setUniformValue("srcTexelSize", QVector2D(1.0f / curW, 1.0f / curH));
        mReduceProgram->setUniformValue("dstSize", QVector2D(float(nextW), float(nextH)));
        mReduceProgram->setUniformValue("ratio", QVector2D(float(nextW) / float(curW), float(nextH) / float(curH)));

        mReduceProgram->enableAttributeArray("vertex");
        mReduceProgram->setAttributeArray("vertex", GL_FLOAT, kFullscreenQuadVertices, 2);
        mReduceProgram->enableAttributeArray("texCoordAttr");
        mReduceProgram->setAttributeArray("texCoordAttr", GL_FLOAT, kFullscreenQuadTexCoords, 2);

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        mReduceProgram->disableAttributeArray("vertex");
        mReduceProgram->disableAttributeArray("texCoordAttr");

        currentFbo = std::move(nextFbo);
        currentSourceTexId = currentFbo->texture();
        curW = nextW;
        curH = nextH;
    }

    mReduceProgram->release();

    // Restore mTexture's real texture-object parameters (see the comment
    // above the saves) before restoring the texture binding itself, then
    // restore the binding/unit, framebuffer, viewport and blend state so
    // paint() picks back up exactly where it left off, regardless of
    // whether the loop above ran to completion or bailed out early.
    glBindTexture(GL_TEXTURE_2D, mTexture->textureId());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, savedMinFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, savedMagFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, savedWrapS);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, savedWrapT);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousBoundTexture));
    glActiveTexture(static_cast<GLenum>(previousActiveTexture));

    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previousFbo));
    glViewport(previousViewport[0], previousViewport[1], previousViewport[2], previousViewport[3]);
    if (blendWasEnabled) {
        glEnable(GL_BLEND);
    }

    mPreciseDownsampleFbo = ok ? std::move(currentFbo) : nullptr;
}

void FilterPixmapItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    // Native OpenGL operates in physical framebuffer pixels. Using the logical
    // combined transform here offsets and scales the quad incorrectly on HiDPI
    // screens, while also making 1:1 zoom look like device-scale magnification.
    const QTransform deviceTransform = painter->deviceTransform();
    const double deviceScaleX =
        std::hypot(deviceTransform.m11(), deviceTransform.m12());
    const double deviceScaleY =
        std::hypot(deviceTransform.m21(), deviceTransform.m22());
    const qreal imageDpr =
        qMax(mImage.devicePixelRatio(), kMinimumDevicePixelRatio);
    double sourceToDeviceScaleX = deviceScaleX / imageDpr;
    double sourceToDeviceScaleY = deviceScaleY / imageDpr;
    if (sourceToDeviceScaleX < kMinimumTransformScale)
        sourceToDeviceScaleX = 1.0;
    if (sourceToDeviceScaleY < kMinimumTransformScale)
        sourceToDeviceScaleY = 1.0;
    const bool isOneToOne =
        qAbs(sourceToDeviceScaleX - 1.0) < kOneToOneScaleTolerance &&
        qAbs(sourceToDeviceScaleY - 1.0) < kOneToOneScaleTolerance;
    float activeCasSharpening = isOneToOne ? 0.0f : mCasSharpening;
    bool activeSmartGpu = (mScalingFilter == QI_FILTER_SMART_GPU);
    if (isOneToOne) {
        activeSmartGpu = false;
    }

    auto fallbackPaint = [this](QPainter *p) {
        bool oldSmooth = p->renderHints() & QPainter::SmoothPixmapTransform;
        bool smooth = mTransformationMode == Qt::SmoothTransformation;
        p->setRenderHint(QPainter::SmoothPixmapTransform, smooth);
        // QPainter's smooth-pixmap transform (same raster code as
        // QImage::scaled(..., Qt::SmoothTransformation)) interpolates
        // Format_ARGB32 as straight alpha: RGB baked into fully-transparent
        // source pixels bleeds a dark/light fringe into opaque neighbors.
        // Format_ARGB32_Premultiplied is interpolated correctly, so use the
        // cached premultiplied copy whenever smoothing is actually active.
        if (smooth && !mImagePremultiplied.isNull()) {
            p->drawImage(mOffset, mImagePremultiplied);
        } else {
            p->drawImage(mOffset, mImage);
        }
        p->setRenderHint(QPainter::SmoothPixmapTransform, oldSmooth);
    };

    // 1. Fallback to default paint if there are no adjustments
    if (qAbs(mBrightness) < ImageLib::kAdjustEpsilon && qAbs(mContrast - 1.0f) < ImageLib::kAdjustEpsilon && qAbs(mSaturation - 1.0f) < ImageLib::kAdjustEpsilon && qAbs(mHue) < ImageLib::kAdjustEpsilon &&
        qAbs(mExposure) < ImageLib::kAdjustEpsilon && qAbs(mTemperature) < ImageLib::kAdjustEpsilon && qAbs(mTint) < ImageLib::kAdjustEpsilon &&
        activeCasSharpening < ImageLib::kAdjustEpsilon && !activeSmartGpu) {
        // releaseGlResources(false); Avoid releasing GL texture on every no-effects paint call to prevent recreate thrashing near 1:1 zoom
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

    bool needMips = (sourceToDeviceScaleX < kDownscaleThreshold ||
                     sourceToDeviceScaleY < kDownscaleThreshold ||
                     activeSmartGpu);
    bool canReuse = mTexture &&
                    mTexture->width() == mImage.width() &&
                    mTexture->height() == mImage.height() &&
                    (!needMips || mTexture->mipLevels() > 1);

    // mImagePremultiplied (kept in sync in setImage()) avoids feeding GL_LINEAR
    // / mipmap filtering straight-alpha data, which would let RGB baked into
    // fully-transparent texels bleed a fringe into opaque neighbors.
    const QImage &texData = mImagePremultiplied.isNull() ? mImage : mImagePremultiplied;

    if (canReuse) {
        if (mLastImage.cacheKey() != mImage.cacheKey()) {
            mTexture->setData(texData, needMips ? QOpenGLTexture::GenerateMipMaps : QOpenGLTexture::DontGenerateMipMaps);
            mLastImage = mImage;
        }
    } else {
        mTexture.reset();
        mTexture = std::make_unique<QOpenGLTexture>(texData, needMips ? QOpenGLTexture::GenerateMipMaps : QOpenGLTexture::DontGenerateMipMaps);
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
    if (filter == QOpenGLTexture::Linear &&
        (sourceToDeviceScaleX < kDownscaleThreshold ||
         sourceToDeviceScaleY < kDownscaleThreshold || activeSmartGpu)) {
        minFilter = QOpenGLTexture::LinearMipMapLinear;
    }
    mTexture->setMinificationFilter(minFilter);

    // isDownscalingNow checks both axes (unlike the old isDownscaling logic
    // further below, which only checked X -- fixed here as part of the same
    // change, since an anisotropically-scaled image downscaling only on Y
    // was silently skipping CAS/SmartSharpen's downscale branch).
    const bool isDownscalingNow = sourceToDeviceScaleX < kDownscaleThreshold ||
                                   sourceToDeviceScaleY < kDownscaleThreshold;

    // preciseTargetW/H are pure arithmetic (no GL involved), so they can be
    // computed before beginNativePainting() -- only the GL work that uses
    // them has to wait.
    const int preciseTargetW = qMax(1, qRound(mImage.width() * sourceToDeviceScaleX));
    const int preciseTargetH = qMax(1, qRound(mImage.height() * sourceToDeviceScaleY));

    painter->beginNativePainting();

    // Once settled (see setSettled()), replace the live box-mip/trilinear
    // minification with a one-shot, exact-ratio GPU box downsample built
    // directly from the full-resolution source (see buildPreciseDownsample()
    // for why). Skipped entirely while interacting, so pan/zoom performance
    // is unaffected; it only (re)builds once, right after the view stops
    // moving, and is reused across frames until the target size or the
    // source image changes.
    //
    // This decision -- and the buildPreciseDownsample() call it can trigger
    // -- must happen after beginNativePainting(), not before: it runs raw
    // OpenGL (FBO/viewport/texture/shader binds and draws) that changes GL
    // state, and QPainter's OpenGL paint engine only saves/restores its own
    // state around the bracketed native-painting region. Doing this GL work
    // before beginNativePainting() left the paint engine's state corrupted
    // for whatever it painted next, which is what made the image
    // intermittently fail to render/flicker blank.
    bool usePreciseTexture = false;
    if (mSettled && isDownscalingNow) {
        const bool preciseTextureStale =
            !mPreciseDownsampleFbo ||
            mPreciseDownsampleFbo->width() != preciseTargetW ||
            mPreciseDownsampleFbo->height() != preciseTargetH ||
            mPreciseDownsampleSourceCacheKey != mImage.cacheKey();
        if (preciseTextureStale) {
            buildPreciseDownsample(preciseTargetW, preciseTargetH);
            mPreciseDownsampleSourceCacheKey = mImage.cacheKey();
            // buildPreciseDownsample() now saves/restores mTexture's raw GL
            // sampler parameters itself; this stays as a defensive backup
            // so Qt's cached filter state can't quietly drift from the
            // live GL state, and to reassert the right filter in case it
            // fails partway and paint() below falls back to sampling
            // mTexture itself this frame.
            mTexture->setMagnificationFilter(filter);
            mTexture->setMinificationFilter(minFilter);
        }
        usePreciseTexture = mPreciseDownsampleFbo &&
                            mPreciseDownsampleFbo->width() == preciseTargetW &&
                            mPreciseDownsampleFbo->height() == preciseTargetH;
    }

    glEnable(GL_BLEND);
    // Premultiplied-alpha blend: the shader now outputs premultiplied color
    // (see filter.frag), so the source factor is GL_ONE, not GL_SRC_ALPHA.
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    mProgram->bind();
    if (usePreciseTexture) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, mPreciseDownsampleFbo->texture());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    } else {
        mTexture->bind();
    }

    QMatrix4x4 modelview(deviceTransform);
    QMatrix4x4 projection;
    const qreal viewportDpr = glWidget->devicePixelRatioF();
    const QSizeF framebufferSize =
        QSizeF(glWidget->size()) * viewportDpr;
    projection.ortho(0, framebufferSize.width(), framebufferSize.height(), 0,
                     -1.0, 1.0);
    QMatrix4x4 matrix = projection * modelview;

    const qreal logicalWidth = mImage.width() / imageDpr;
    const qreal logicalHeight = mImage.height() / imageDpr;

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
    mProgram->setUniformValue(
        "pixelSize",
        usePreciseTexture
            ? QVector2D(1.0f / preciseTargetW, 1.0f / preciseTargetH)
            : QVector2D(1.0f / (mImage.width() * sourceToDeviceScaleX),
                        1.0f / (mImage.height() * sourceToDeviceScaleY)));
    mProgram->setUniformValue("casContrast", mCasContrast);
    mProgram->setUniformValue("casSharpening", activeCasSharpening);
    mProgram->setUniformValue("sharpenMode", activeSmartGpu ? (int)QI_FILTER_SMART_GPU : (int)mScalingFilter);
    // The precise texture already matches the on-screen footprint 1:1, so
    // CAS/SmartSharpen should use their plain 1-texel-tap branch (isDownscaling
    // == 0) exactly as they would at native/upscaled zoom, rather than their
    // mip-bias downscale branch, which is only needed when tex is mTexture's
    // full-resolution mip chain.
    mProgram->setUniformValue(
        "isDownscaling",
        (!usePreciseTexture && isDownscalingNow) ? 1 : 0);

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

    if (usePreciseTexture) {
        glBindTexture(GL_TEXTURE_2D, 0);
    } else {
        mTexture->release();
    }
    mProgram->release();

    painter->endNativePainting();
}

QVariant FilterPixmapItem::itemChange(GraphicsItemChange change, const QVariant &value) {
    if (change == ItemVisibleHasChanged) {
        if (!value.toBool()) {
            releaseGlResources(false);
        }
    }
    return QGraphicsItem::itemChange(change, value);
}

void FilterPixmapItem::releaseGlResources(bool forceRelease) {
    if (!mTexture && !mPreciseDownsampleFbo) return;

    // QOpenGLTexture/QOpenGLFramebufferObject need a current context to
    // release GPU resources. We try to make it current first. If that fails
    // and forceRelease is true, we release ownership to avoid driver crashes
    // at the cost of a memory leak.
    if (!QOpenGLContext::currentContext()) {
        if (auto *glWidget = findGlWidget()) {
            glWidget->makeCurrent();
        }
    }

    if (QOpenGLContext::currentContext()) {
        mTexture.reset();
        mPreciseDownsampleFbo.reset();
    } else if (forceRelease) {
        mTexture.release();
        mPreciseDownsampleFbo.release();
    }
    mLastImage = QImage();
    mPreciseDownsampleSourceCacheKey = -1;
}

QOpenGLWidget* FilterPixmapItem::findGlWidget() const {
    if (auto *s = scene()) {
        for (auto *view : s->views()) {
            if (auto *glWidget = qobject_cast<QOpenGLWidget*>(view->viewport())) {
                return glWidget;
            }
        }
    }
    return nullptr;
}
