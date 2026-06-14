#pragma once

#include <QGraphicsPixmapItem>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <memory>
#include "settings.h"

class FilterPixmapItem : public QGraphicsPixmapItem, protected QOpenGLFunctions {
public:
    explicit FilterPixmapItem(QGraphicsItem *parent = nullptr);
    ~FilterPixmapItem();

    void setColorAdjustments(float exposure, float contrast, float brightness, float temperature, float tint, float saturation, float hue);
    void setCasSettings(float sharpening, float contrast);
    void setScalingFilter(ScalingFilter filter);
    void setApplyFilterAt100(bool enabled);

protected:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    float mExposure = 0.0f;    // -3.0f to 3.0f
    float mContrast = 1.0f;   // 0.0f to 3.0f
    float mBrightness = 0.0f; // -1.0f to 1.0f
    float mTemperature = 0.0f; // -0.5f to 0.5f
    float mTint = 0.0f;        // -0.5f to 0.5f
    float mSaturation = 1.0f; // 0.0f to 2.0f
    float mHue = 0.0f;        // -180.0f to 180.0f (degrees)
    float mCasSharpening = 0.0f;
    float mCasContrast = 0.0f;
    ScalingFilter mScalingFilter = QI_FILTER_BILINEAR;
    bool mApplyFilterAt100 = false;

    bool mInitialized = false;
    bool mShaderFailed = false;
    std::unique_ptr<QOpenGLShaderProgram> mProgram;
    std::unique_ptr<QOpenGLTexture> mTexture;
    QPixmap mLastPixmap;

    void initShader();
};
