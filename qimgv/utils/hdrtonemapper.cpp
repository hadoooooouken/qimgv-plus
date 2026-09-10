#include "hdrtonemapper.h"

#include <QColorSpace>
#include <QFloat16>
#include <QRgba64>
#include <QThreadPool>
#include <QRunnable>
#include <QSemaphore>
#include <vector>
#include <cmath>
#include <algorithm>
#include <array>

namespace {

// PQ EOTF Constants (SMPTE ST 2084 / Rec.2100)
constexpr float kPqM1 = 2610.0f / 16384.0f;           // 0.1593017578125f
constexpr float kPqM2 = (2523.0f / 4096.0f) * 128.0f; // 78.84375f
constexpr float kPqC1 = 3424.0f / 4096.0f;            // 0.8359375f
constexpr float kPqC2 = (2413.0f / 4096.0f) * 32.0f;  // 18.8515625f
constexpr float kPqC3 = (2392.0f / 4096.0f) * 32.0f;  // 18.6875f
constexpr float kPqPeakLuminanceNits = 10000.0f;

// HLG EOTF Constants (ARIB STD-B67 / Rec.2100)
constexpr float kHlgA = 0.17883277f;
constexpr float kHlgB = 1.0f - 4.0f * kHlgA; // 0.28466892f
constexpr float kHlgC = 0.55991073f;
constexpr float kHlgPeakLuminanceNits = 1000.0f;

// scRGB (IEC 61966-2-2) reference white: a linear sample value of 1.0
// corresponds to this many nits of SDR reference white.
constexpr float kScRgbReferenceWhiteNits = 80.0f;

// Rec.709 / sRGB Luminance Coefficients
constexpr float kLumaR = 0.2126f;
constexpr float kLumaG = 0.7152f;
constexpr float kLumaB = 0.0722f;

// BT.2020 to Linear sRGB Matrix
//   [R_srgb]   [ 1.6604910 -0.5876411 -0.0728499 ] [R_2020]
//   [G_srgb] = [-0.1245505  1.1328999 -0.0083494 ] [G_2020]
//   [B_srgb]   [-0.0181508 -0.1005789  1.1187297 ] [B_2020]
constexpr float kBt2020ToSrgb[3][3] = {
    {  1.6604910f, -0.5876411f, -0.0728499f },
    { -0.1245505f,  1.1328999f, -0.0083494f },
    { -0.0181508f, -0.1005789f,  1.1187297f }
};

// Display P3 to Linear sRGB Matrix
//   [R_srgb]   [ 1.2249402 -0.2249402  0.0000000 ] [R_p3]
//   [G_srgb] = [-0.0420569  1.0420569  0.0000000 ] [G_p3]
//   [B_srgb]   [-0.0196376 -0.0786361  1.0982737 ] [B_p3]
constexpr float kP3ToSrgb[3][3] = {
    {  1.2249402f, -0.2249402f,  0.0000000f },
    { -0.0420569f,  1.0420569f,  0.0000000f },
    { -0.0196376f, -0.0786361f,  1.0982737f }
};

// ACES Narkowicz fit parameters
constexpr float kAcesA = 2.51f;
constexpr float kAcesB = 0.03f;
constexpr float kAcesC = 2.43f;
constexpr float kAcesD = 0.59f;
constexpr float kAcesE = 0.14f;

// Hable (Uncharted 2) parameters
constexpr float kHableA = 0.15f;
constexpr float kHableB = 0.50f;
constexpr float kHableC = 0.10f;
constexpr float kHableD = 0.20f;
constexpr float kHableE = 0.02f;
constexpr float kHableF = 0.30f;
constexpr float kHableW = 11.2f;

// BT.2408 Knee point (75% of SDR reference white)
constexpr float kBt2408Knee = 0.75f;

// Small epsilon to guard against division by zero
constexpr float kEpsilon = 1e-6f;

// Size of 16-bit EOTF lookup table
constexpr int kLutSize16 = 65536;

// Components per pixel in the kimageformats float plugins' buffer layout
// (R, G, B, A), used when reading JXR/EXR/HDR/PFM float scanlines directly.
constexpr int kFloatChannelsPerPixel = 4;

enum class InputTransfer {
    PQ,
    HLG,
    Linear,
    Gamma
};

enum class InputPrimaries {
    Bt2020,
    DisplayP3,
    Srgb
};

inline float decodePq(float n, float targetWhiteNits) {
    if (n <= 0.0f) return 0.0f;
    float v = std::min(n, 1.0f);
    float vp = std::pow(v, 1.0f / kPqM2);
    float num = std::max(vp - kPqC1, 0.0f);
    float den = kPqC2 - kPqC3 * vp;
    if (den <= 0.0f) return kPqPeakLuminanceNits / targetWhiteNits;
    float y = std::pow(num / den, 1.0f / kPqM1);
    return (y * kPqPeakLuminanceNits) / targetWhiteNits;
}

inline float decodeHlg(float n, float targetWhiteNits) {
    if (n <= 0.0f) return 0.0f;
    float v = std::min(n, 1.0f);
    float linearNorm = 0.0f;
    if (v <= 0.5f) {
        linearNorm = (v * v) / 3.0f;
    } else {
        linearNorm = (std::exp((v - kHlgC) / kHlgA) + kHlgB) / 12.0f;
    }
    return (linearNorm * kHlgPeakLuminanceNits) / targetWhiteNits;
}

inline float decodeLinear(float n, float targetWhiteNits) {
    // scRGB-style linear samples (JXR/EXR/HDR/PFM). Unlike decodePq()/
    // decodeHlg(), values above 1.0 are not clamped: they represent real
    // highlight information above SDR white, which the tone-map operator
    // is responsible for compressing back into range.
    if (n <= 0.0f) return 0.0f;
    return (n * kScRgbReferenceWhiteNits) / targetWhiteNits;
}

inline float decodeTransfer(InputTransfer transfer, float n, float targetWhiteNits) {
    switch (transfer) {
    case InputTransfer::PQ:
        return decodePq(n, targetWhiteNits);
    case InputTransfer::HLG:
        return decodeHlg(n, targetWhiteNits);
    case InputTransfer::Linear:
        return decodeLinear(n, targetWhiteNits);
    case InputTransfer::Gamma:
        // Not yet resolved by applyToneMapping(); keep the pre-existing
        // default (PQ) so behavior is unchanged if this becomes reachable.
        break;
    }
    return decodePq(n, targetWhiteNits);
}

inline float linearToSrgb(float val) {
    val = std::clamp(val, 0.0f, 1.0f);
    if (val <= 0.0031308f) {
        return val * 12.92f;
    }
    return 1.055f * std::pow(val, 1.0f / 2.4f) - 0.055f;
}

inline uint8_t floatToByte(float val) {
    int ival = static_cast<int>(linearToSrgb(val) * 255.0f + 0.5f);
    return static_cast<uint8_t>(std::clamp(ival, 0, 255));
}

inline void transformPrimaries(float &r, float &g, float &b, InputPrimaries primaries) {
    if (primaries == InputPrimaries::Bt2020) {
        float rOut = kBt2020ToSrgb[0][0] * r + kBt2020ToSrgb[0][1] * g + kBt2020ToSrgb[0][2] * b;
        float gOut = kBt2020ToSrgb[1][0] * r + kBt2020ToSrgb[1][1] * g + kBt2020ToSrgb[1][2] * b;
        float bOut = kBt2020ToSrgb[2][0] * r + kBt2020ToSrgb[2][1] * g + kBt2020ToSrgb[2][2] * b;
        r = rOut;
        g = gOut;
        b = bOut;
    } else if (primaries == InputPrimaries::DisplayP3) {
        float rOut = kP3ToSrgb[0][0] * r + kP3ToSrgb[0][1] * g + kP3ToSrgb[0][2] * b;
        float gOut = kP3ToSrgb[1][0] * r + kP3ToSrgb[1][1] * g + kP3ToSrgb[1][2] * b;
        float bOut = kP3ToSrgb[2][0] * r + kP3ToSrgb[2][1] * g + kP3ToSrgb[2][2] * b;
        r = rOut;
        g = gOut;
        b = bOut;
    }
}

inline void compressGamut(float &r, float &g, float &b) {
    float luma = kLumaR * r + kLumaG * g + kLumaB * b;
    if (luma <= 0.0f) {
        r = 0.0f;
        g = 0.0f;
        b = 0.0f;
        return;
    }
    float minC = std::min({ r, g, b });
    if (minC < 0.0f) {
        float s = luma / (luma - minC);
        r = luma + s * (r - luma);
        g = luma + s * (g - luma);
        b = luma + s * (b - luma);
    }
}

inline float toneMapBt2408(float x) {
    if (x <= kBt2408Knee) {
        return x;
    }
    float diff = x - kBt2408Knee;
    float range = 1.0f - kBt2408Knee;
    return kBt2408Knee + range * std::tanh(diff / range);
}

inline void applyOperatorBt2408(float &r, float &g, float &b) {
    float luma = kLumaR * r + kLumaG * g + kLumaB * b;
    if (luma > kEpsilon) {
        float mappedLuma = toneMapBt2408(luma);
        float ratio = mappedLuma / luma;
        r *= ratio;
        g *= ratio;
        b *= ratio;
    }
    float maxC = std::max({ r, g, b });
    if (maxC > 1.0f) {
        float luma2 = kLumaR * r + kLumaG * g + kLumaB * b;
        if (maxC > luma2) {
            float s = (1.0f - luma2) / (maxC - luma2);
            s = std::clamp(s, 0.0f, 1.0f);
            r = luma2 + s * (r - luma2);
            g = luma2 + s * (g - luma2);
            b = luma2 + s * (b - luma2);
        }
    }
}

inline void applyOperatorReinhardJodie(float &r, float &g, float &b) {
    float luma = kLumaR * r + kLumaG * g + kLumaB * b;
    if (luma <= 0.0f) return;

    auto jodieChannel = [luma](float c) {
        float tc = c / (1.0f + c);
        float tl = c / (1.0f + luma);
        return tl + tc * (tc - tl);
    };

    r = jodieChannel(r);
    g = jodieChannel(g);
    b = jodieChannel(b);
}

inline float acesNarkowicz(float v) {
    return (v * (kAcesA * v + kAcesB)) / (v * (kAcesC * v + kAcesD) + kAcesE);
}

inline void applyOperatorAcesFilmic(float &r, float &g, float &b) {
    float luma = kLumaR * r + kLumaG * g + kLumaB * b;
    if (luma > kEpsilon) {
        float mappedLuma = std::clamp(acesNarkowicz(luma), 0.0f, 1.0f);
        float ratio = mappedLuma / luma;
        r *= ratio;
        g *= ratio;
        b *= ratio;
    }
    float maxC = std::max({ r, g, b });
    if (maxC > 1.0f) {
        float luma2 = kLumaR * r + kLumaG * g + kLumaB * b;
        if (maxC > luma2) {
            float s = (1.0f - luma2) / (maxC - luma2);
            s = std::clamp(s, 0.0f, 1.0f);
            r = luma2 + s * (r - luma2);
            g = luma2 + s * (g - luma2);
            b = luma2 + s * (b - luma2);
        }
    }
}

inline float hableFunc(float v) {
    return ((v * (kHableA * v + kHableC * kHableB) + kHableD * kHableE) /
            (v * (kHableA * v + kHableB) + kHableD * kHableF)) - (kHableE / kHableF);
}

inline void applyOperatorHable(float &r, float &g, float &b) {
    static const float invWhite = 1.0f / hableFunc(kHableW);
    float luma = kLumaR * r + kLumaG * g + kLumaB * b;
    if (luma > kEpsilon) {
        float mappedLuma = std::max(hableFunc(luma) * invWhite, 0.0f);
        float ratio = mappedLuma / luma;
        r *= ratio;
        g *= ratio;
        b *= ratio;
    }
    float maxC = std::max({ r, g, b });
    if (maxC > 1.0f) {
        float luma2 = kLumaR * r + kLumaG * g + kLumaB * b;
        if (maxC > luma2) {
            float s = (1.0f - luma2) / (maxC - luma2);
            s = std::clamp(s, 0.0f, 1.0f);
            r = luma2 + s * (r - luma2);
            g = luma2 + s * (g - luma2);
            b = luma2 + s * (b - luma2);
        }
    }
}

inline void applyToneMapOperator(ToneMapOperator op, float &r, float &g, float &b) {
    switch (op) {
    case ToneMapOperator::Bt2408:
        applyOperatorBt2408(r, g, b);
        break;
    case ToneMapOperator::ReinhardJodie:
        applyOperatorReinhardJodie(r, g, b);
        break;
    case ToneMapOperator::AcesFilmic:
        applyOperatorAcesFilmic(r, g, b);
        break;
    case ToneMapOperator::Hable:
        applyOperatorHable(r, g, b);
        break;
    }
}

class ToneMapTask : public QRunnable {
public:
    ToneMapTask(int yStart, int yEnd, const QImage &src, QImage &dst,
                const std::vector<float> &lut, InputTransfer transfer,
                InputPrimaries primaries, ToneMapOperator op,
                float targetWhiteNits, QSemaphore &semaphore)
        : m_yStart(yStart), m_yEnd(yEnd), m_src(src), m_dst(dst),
          m_lut(lut), m_transfer(transfer), m_primaries(primaries),
          m_op(op), m_targetWhiteNits(targetWhiteNits), m_semaphore(semaphore)
    {
        setAutoDelete(true);
    }

    void run() override {
        const int width = m_src.width();
        const QImage::Format srcFormat = m_src.format();
        const bool is16Bit = (srcFormat == QImage::Format_RGBA64 ||
                              srcFormat == QImage::Format_RGBX64 ||
                              srcFormat == QImage::Format_RGBA64_Premultiplied);
        const bool isHalfFloat = (srcFormat == QImage::Format_RGBX16FPx4 ||
                                  srcFormat == QImage::Format_RGBA16FPx4 ||
                                  srcFormat == QImage::Format_RGBA16FPx4_Premultiplied);
        const bool isFullFloat = (srcFormat == QImage::Format_RGBX32FPx4 ||
                                  srcFormat == QImage::Format_RGBA32FPx4 ||
                                  srcFormat == QImage::Format_RGBA32FPx4_Premultiplied);
        const bool isPremultipliedFloat = (srcFormat == QImage::Format_RGBA16FPx4_Premultiplied ||
                                           srcFormat == QImage::Format_RGBA32FPx4_Premultiplied);

        for (int y = m_yStart; y < m_yEnd; ++y) {
            uint32_t *dstLine = reinterpret_cast<uint32_t *>(m_dst.scanLine(y));

            if (isHalfFloat) {
                // Linear (or otherwise transfer-tagged) half-float scanlines, read
                // directly to avoid the precision loss and highlight clipping that
                // QImage::pixel()/convertToFormat() would introduce by first
                // collapsing the buffer through 8-bit ARGB32. Channel order mirrors
                // the kimageformats float plugins (JXR/EXR/HDR/PFM): R, G, B, A.
                const qfloat16 *srcLine = reinterpret_cast<const qfloat16 *>(m_src.constScanLine(y));
                for (int x = 0; x < width; ++x) {
                    const int base = x * kFloatChannelsPerPixel;
                    float r = static_cast<float>(srcLine[base + 0]);
                    float g = static_cast<float>(srcLine[base + 1]);
                    float b = static_cast<float>(srcLine[base + 2]);
                    float a = static_cast<float>(srcLine[base + 3]);

                    if (isPremultipliedFloat) {
                        if (a > kEpsilon) {
                            r /= a;
                            g /= a;
                            b /= a;
                        } else {
                            r = g = b = 0.0f;
                        }
                    }

                    r = decodeTransfer(m_transfer, r, m_targetWhiteNits);
                    g = decodeTransfer(m_transfer, g, m_targetWhiteNits);
                    b = decodeTransfer(m_transfer, b, m_targetWhiteNits);

                    transformPrimaries(r, g, b, m_primaries);
                    compressGamut(r, g, b);
                    applyToneMapOperator(m_op, r, g, b);

                    const uint8_t alphaByte = static_cast<uint8_t>(std::clamp(a, 0.0f, 1.0f) * 255.0f + 0.5f);
                    dstLine[x] = qRgba(floatToByte(r), floatToByte(g), floatToByte(b), alphaByte);
                }
            } else if (isFullFloat) {
                const float *srcLine = reinterpret_cast<const float *>(m_src.constScanLine(y));
                for (int x = 0; x < width; ++x) {
                    const int base = x * kFloatChannelsPerPixel;
                    float r = srcLine[base + 0];
                    float g = srcLine[base + 1];
                    float b = srcLine[base + 2];
                    float a = srcLine[base + 3];

                    if (isPremultipliedFloat) {
                        if (a > kEpsilon) {
                            r /= a;
                            g /= a;
                            b /= a;
                        } else {
                            r = g = b = 0.0f;
                        }
                    }

                    r = decodeTransfer(m_transfer, r, m_targetWhiteNits);
                    g = decodeTransfer(m_transfer, g, m_targetWhiteNits);
                    b = decodeTransfer(m_transfer, b, m_targetWhiteNits);

                    transformPrimaries(r, g, b, m_primaries);
                    compressGamut(r, g, b);
                    applyToneMapOperator(m_op, r, g, b);

                    const uint8_t alphaByte = static_cast<uint8_t>(std::clamp(a, 0.0f, 1.0f) * 255.0f + 0.5f);
                    dstLine[x] = qRgba(floatToByte(r), floatToByte(g), floatToByte(b), alphaByte);
                }
            } else if (is16Bit) {
                const QRgba64 *srcLine = reinterpret_cast<const QRgba64 *>(m_src.constScanLine(y));
                for (int x = 0; x < width; ++x) {
                    const QRgba64 px = srcLine[x];
                    float r = m_lut[px.red()];
                    float g = m_lut[px.green()];
                    float b = m_lut[px.blue()];
                    const uint8_t a = static_cast<uint8_t>(px.alpha() >> 8);

                    transformPrimaries(r, g, b, m_primaries);
                    compressGamut(r, g, b);

                    switch (m_op) {
                    case ToneMapOperator::Bt2408:
                        applyOperatorBt2408(r, g, b);
                        break;
                    case ToneMapOperator::ReinhardJodie:
                        applyOperatorReinhardJodie(r, g, b);
                        break;
                    case ToneMapOperator::AcesFilmic:
                        applyOperatorAcesFilmic(r, g, b);
                        break;
                    case ToneMapOperator::Hable:
                        applyOperatorHable(r, g, b);
                        break;
                    }

                    dstLine[x] = qRgba(floatToByte(r), floatToByte(g), floatToByte(b), a);
                }
            } else {
                // 32-bit fallback / general path
                for (int x = 0; x < width; ++x) {
                    const QRgb px = m_src.pixel(x, y);
                    float rNorm = qRed(px) / 255.0f;
                    float gNorm = qGreen(px) / 255.0f;
                    float bNorm = qBlue(px) / 255.0f;

                    float r = decodeTransfer(m_transfer, rNorm, m_targetWhiteNits);
                    float g = decodeTransfer(m_transfer, gNorm, m_targetWhiteNits);
                    float b = decodeTransfer(m_transfer, bNorm, m_targetWhiteNits);

                    transformPrimaries(r, g, b, m_primaries);
                    compressGamut(r, g, b);

                    switch (m_op) {
                    case ToneMapOperator::Bt2408:
                        applyOperatorBt2408(r, g, b);
                        break;
                    case ToneMapOperator::ReinhardJodie:
                        applyOperatorReinhardJodie(r, g, b);
                        break;
                    case ToneMapOperator::AcesFilmic:
                        applyOperatorAcesFilmic(r, g, b);
                        break;
                    case ToneMapOperator::Hable:
                        applyOperatorHable(r, g, b);
                        break;
                    }

                    dstLine[x] = qRgba(floatToByte(r), floatToByte(g), floatToByte(b), qAlpha(px));
                }
            }
        }
        m_semaphore.release();
    }

private:
    int m_yStart;
    int m_yEnd;
    const QImage &m_src;
    QImage &m_dst;
    const std::vector<float> &m_lut;
    InputTransfer m_transfer;
    InputPrimaries m_primaries;
    ToneMapOperator m_op;
    float m_targetWhiteNits;
    QSemaphore &m_semaphore;
};

} // namespace

// Returns true for QImage floating-point pixel formats (16-bit half float or
// 32-bit float, with or without alpha). Plugins that decode HDR data as
// linear scRGB-style samples (JXR, OpenEXR, Radiance HDR, PFM) use these
// formats regardless of whether they also set HDR_* text tags.
bool HdrToneMapper::isLinearFloatFormat(QImage::Format format) {
    switch (format) {
    case QImage::Format_RGBX16FPx4:
    case QImage::Format_RGBA16FPx4:
    case QImage::Format_RGBA16FPx4_Premultiplied:
    case QImage::Format_RGBX32FPx4:
    case QImage::Format_RGBA32FPx4:
    case QImage::Format_RGBA32FPx4_Premultiplied:
        return true;
    default:
        return false;
    }
}

bool HdrToneMapper::isHdr(const QImage &image) {
    if (image.isNull()) {
        return false;
    }

    // Check explicit metadata attributes
    if (image.text(QStringLiteral("HDR_IsHDR")) == QStringLiteral("true") ||
        !image.text(QStringLiteral("HDR_Profile")).isEmpty()) {
        return true;
    }

    // Floating-point pixel storage is HDR-range data regardless of which
    // plugin produced it (JXR, OpenEXR, Radiance HDR, PFM all decode into
    // these formats without necessarily setting HDR_* text tags).
    if (isLinearFloatFormat(image.format())) {
        return true;
    }

    // Check QColorSpace
    const QColorSpace cs = image.colorSpace();
    if (cs.isValid()) {
        const QColorSpace::TransferFunction tf = cs.transferFunction();
        if (tf == QColorSpace::TransferFunction::St2084 ||
            tf == QColorSpace::TransferFunction::Hlg) {
            return true;
        }

        const QString desc = cs.description();
        if (desc.contains(QStringLiteral("HDR"), Qt::CaseInsensitive) ||
            desc.contains(QStringLiteral("PQ"), Qt::CaseInsensitive) ||
            desc.contains(QStringLiteral("HLG"), Qt::CaseInsensitive) ||
            desc.contains(QStringLiteral("2100"), Qt::CaseInsensitive)) {
            return true;
        }
    }

    return false;
}

QString HdrToneMapper::detectHdrProfile(const QImage &image) {
    if (image.isNull()) {
        return QString();
    }

    // Explicit tag takes first precedence
    const QString taggedProfile = image.text(QStringLiteral("HDR_Profile"));
    if (!taggedProfile.isEmpty()) {
        return taggedProfile;
    }

    const QColorSpace cs = image.colorSpace();
    if (!cs.isValid()) {
        return QString();
    }

    const QColorSpace::TransferFunction tf = cs.transferFunction();
    const QColorSpace::Primaries prim = cs.primaries();

    if (tf == QColorSpace::TransferFunction::St2084) {
        if (prim == QColorSpace::Primaries::Bt2020) {
            return QStringLiteral("Rec.2100 PQ (HDR10)");
        } else if (prim == QColorSpace::Primaries::DciP3D65) {
            return QStringLiteral("Display P3 PQ");
        }
        return QStringLiteral("SMPTE ST 2084 (PQ)");
    } else if (tf == QColorSpace::TransferFunction::Hlg) {
        if (prim == QColorSpace::Primaries::Bt2020) {
            return QStringLiteral("BT.2100 HLG");
        } else if (prim == QColorSpace::Primaries::DciP3D65) {
            return QStringLiteral("Display P3 HLG");
        }
        return QStringLiteral("ARIB STD-B67 (HLG)");
    }

    const QString desc = cs.description();
    if (desc.contains(QStringLiteral("HDR"), Qt::CaseInsensitive) ||
        desc.contains(QStringLiteral("PQ"), Qt::CaseInsensitive) ||
        desc.contains(QStringLiteral("HLG"), Qt::CaseInsensitive) ||
        desc.contains(QStringLiteral("2100"), Qt::CaseInsensitive)) {
        return desc;
    }

    return QString();
}

QImage HdrToneMapper::applyToneMapping(const QImage &srcImage, const HdrToneMapParams &params) {
    if (srcImage.isNull()) {
        return srcImage;
    }

    // Determine Transfer Function
    InputTransfer transfer = InputTransfer::PQ;
    const QString transferText = srcImage.text(QStringLiteral("HDR_Transfer"));
    const QColorSpace cs = srcImage.colorSpace();

    if (transferText.compare(QStringLiteral("HLG"), Qt::CaseInsensitive) == 0 ||
        (cs.isValid() && cs.transferFunction() == QColorSpace::TransferFunction::Hlg)) {
        transfer = InputTransfer::HLG;
    } else if (transferText.compare(QStringLiteral("Linear"), Qt::CaseInsensitive) == 0 ||
               (cs.isValid() && cs.transferFunction() == QColorSpace::TransferFunction::Linear) ||
               (!cs.isValid() && isLinearFloatFormat(srcImage.format()))) {
        // Linear scRGB-style float buffers (JXR, OpenEXR, Radiance HDR, PFM).
        // Fall back to treating untagged floating-point sources as linear
        // rather than silently defaulting to PQ, which would misdecode raw
        // linear samples.
        transfer = InputTransfer::Linear;
    }

    // Determine Primaries
    InputPrimaries primaries = InputPrimaries::Bt2020;
    const QString primariesText = srcImage.text(QStringLiteral("HDR_Primaries"));
    if (primariesText.contains(QStringLiteral("P3"), Qt::CaseInsensitive) ||
        (cs.isValid() && cs.primaries() == QColorSpace::Primaries::DciP3D65)) {
        primaries = InputPrimaries::DisplayP3;
    } else if (primariesText.contains(QStringLiteral("709"), Qt::CaseInsensitive) ||
               primariesText.contains(QStringLiteral("sRGB"), Qt::CaseInsensitive) ||
               (cs.isValid() && cs.primaries() == QColorSpace::Primaries::SRgb)) {
        primaries = InputPrimaries::Srgb;
    }

    const float targetWhite = (params.targetWhiteNits > 0.0f) ? params.targetWhiteNits : 203.0f;

    // Precompute 16-bit EOTF LUT for fast decode
    std::vector<float> eotfLut(kLutSize16);
    for (int i = 0; i < kLutSize16; ++i) {
        const float norm = static_cast<float>(i) / (kLutSize16 - 1);
        eotfLut[i] = decodeTransfer(transfer, norm, targetWhite);
    }

    const int width = srcImage.width();
    const int height = srcImage.height();
    const QImage::Format dstFormat = srcImage.hasAlphaChannel()
        ? QImage::Format_ARGB32
        : QImage::Format_RGB32;

    QImage dstImage(width, height, dstFormat);
    if (dstImage.isNull()) {
        return srcImage.convertToFormat(dstFormat);
    }

    // Multi-threaded chunked scanline execution
    const int threadCount = std::clamp(QThreadPool::globalInstance()->maxThreadCount(), 1, 64);
    const int linesPerChunk = std::max(8, (height + threadCount - 1) / threadCount);
    const int chunkCount = (height + linesPerChunk - 1) / linesPerChunk;

    QSemaphore semaphore(0);
    for (int i = 0; i < chunkCount; ++i) {
        int yStart = i * linesPerChunk;
        int yEnd = std::min(yStart + linesPerChunk, height);
        auto *task = new ToneMapTask(yStart, yEnd, srcImage, dstImage, eotfLut,
                                     transfer, primaries, params.op, targetWhite, semaphore);
        QThreadPool::globalInstance()->start(task);
    }

    semaphore.acquire(chunkCount);

    // Set standard sRGB color space
    dstImage.setColorSpace(QColorSpace(QColorSpace::SRgb));

    // Preserve non-HDR metadata text keys (Exif, XMP, etc.), skipping HDR_* tags
    for (const QString &key : srcImage.textKeys()) {
        if (!key.startsWith(QStringLiteral("HDR_"))) {
            dstImage.setText(key, srcImage.text(key));
        }
    }

    return dstImage;
}
