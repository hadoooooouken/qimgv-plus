#pragma once

#include <QGraphicsPixmapItem>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>

class FilterPixmapItem : public QGraphicsPixmapItem, protected QOpenGLFunctions {
public:
    explicit FilterPixmapItem(QGraphicsItem *parent = nullptr);
    ~FilterPixmapItem();

    void setColorAdjustments(float brightness, float contrast, float saturation, float hue, float exposure, float temperature, float tint);
    void setCasSettings(float sharpening, float contrast);

protected:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    float mBrightness = 0.0f; // -1.0f to 1.0f
    float mContrast = 1.0f;   // 0.0f to 3.0f
    float mSaturation = 1.0f; // 0.0f to 2.0f
    float mHue = 0.0f;        // -180.0f to 180.0f (degrees)
    float mExposure = 0.0f;    // -3.0f to 3.0f
    float mTemperature = 0.0f; // -0.5f to 0.5f
    float mTint = 0.0f;        // -0.5f to 0.5f
    float mCasSharpening = 0.0f;
    float mCasContrast = 0.0f;

    bool mInitialized = false;
    QOpenGLShaderProgram *mProgram = nullptr;
    QOpenGLTexture *mTexture = nullptr;
    QPixmap mLastPixmap;

    void initShader();
};
