#include "hdrtonemapper.h"

#include <QColorSpace>
#include <QFloat16>
#include <QRgba64>
#include <QThreadPool>
#include <QRunnable>
#include <QSemaphore>

#include <immintrin.h>

#include <vector>
#include <array>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <cstdint>

namespace {

// PQ EOTF constants (SMPTE ST 2084 / Rec.2100).
constexpr float kPqM1 = 2610.0f / 16384.0f;
constexpr float kPqM2 = (2523.0f / 4096.0f) * 128.0f;
constexpr float kPqC1 = 3424.0f / 4096.0f;
constexpr float kPqC2 = (2413.0f / 4096.0f) * 32.0f;
constexpr float kPqC3 = (2392.0f / 4096.0f) * 32.0f;
constexpr float kPqPeakLuminanceNits = 10000.0f;

// HLG EOTF constants (ARIB STD-B67 / Rec.2100).
constexpr float kHlgA = 0.17883277f;
constexpr float kHlgB = 1.0f - 4.0f * kHlgA;
constexpr float kHlgC = 0.55991073f;
constexpr float kHlgPeakLuminanceNits = 1000.0f;

// scRGB reference white luminance.
constexpr float kScRgbReferenceWhiteNits = 80.0f;

// Rec.709 / sRGB luminance coefficients.
constexpr float kLumaR = 0.2126f;
constexpr float kLumaG = 0.7152f;
constexpr float kLumaB = 0.0722f;

// BT.2020 -> linear sRGB.
constexpr float kBt2020ToSrgb[3][3] = {
    {  1.6604910f, -0.5876411f, -0.0728499f },
    { -0.1245505f,  1.1328999f, -0.0083494f },
    { -0.0181508f, -0.1005789f,  1.1187297f }
};

// Display P3 -> linear sRGB.
constexpr float kP3ToSrgb[3][3] = {
    {  1.2249402f, -0.2249402f,  0.0000000f },
    { -0.0420569f,  1.0420569f,  0.0000000f },
    { -0.0196376f, -0.0786361f,  1.0982737f }
};

// ACES Narkowicz fit.
constexpr float kAcesA = 2.51f;
constexpr float kAcesB = 0.03f;
constexpr float kAcesC = 2.43f;
constexpr float kAcesD = 0.59f;
constexpr float kAcesE = 0.14f;

// Hable (Uncharted 2).
constexpr float kHableA = 0.15f;
constexpr float kHableB = 0.50f;
constexpr float kHableC = 0.10f;
constexpr float kHableD = 0.20f;
constexpr float kHableE = 0.02f;
constexpr float kHableF = 0.30f;
constexpr float kHableW = 11.2f;

// BT.2408 knee point.
constexpr float kBt2408Knee = 0.75f;

constexpr float kEpsilon = 1e-6f;

// Components per pixel in the kimageformats float plugins' buffer layout.
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

// qfloat16 does not expose setBits()/bits() in the public Qt API. Its
// storage is a single 16-bit word, so reinterpret via memcpy.
inline quint16 halfToBits(qfloat16 h) {
    quint16 bits;
    std::memcpy(&bits, &h, sizeof(quint16));
    return bits;
}

inline qfloat16 halfFromBits(quint16 bits) {
    qfloat16 h;
    std::memcpy(&h, &bits, sizeof(quint16));
    return h;
}

// ---------------------------------------------------------------------------
// Absolute transfer decoding, returning nits. Only used at LUT build time.
// ---------------------------------------------------------------------------
inline float decodeTransferAbsolute(InputTransfer transfer, float n) {
    if (n <= 0.0f) return 0.0f;
    switch (transfer) {
    case InputTransfer::PQ: {
        const float v = std::min(n, 1.0f);
        const float vp = std::pow(v, 1.0f / kPqM2);
        const float num = std::max(vp - kPqC1, 0.0f);
        const float den = kPqC2 - kPqC3 * vp;
        if (den <= 0.0f) return kPqPeakLuminanceNits;
        const float y = std::pow(num / den, 1.0f / kPqM1);
        return y * kPqPeakLuminanceNits;
    }
    case InputTransfer::HLG: {
        const float v = std::min(n, 1.0f);
        const float lin = (v <= 0.5f)
            ? (v * v) / 3.0f
            : (std::exp((v - kHlgC) / kHlgA) + kHlgB) / 12.0f;
        return lin * kHlgPeakLuminanceNits;
    }
    case InputTransfer::Linear:
        return n * kScRgbReferenceWhiteNits;
    case InputTransfer::Gamma:
    default:
        return 0.0f;
    }
}

// ---------------------------------------------------------------------------
// Static sRGB and tanh LUTs. Built once.
// ---------------------------------------------------------------------------
struct SrgbLut {
    static constexpr int kN = 4096;
    std::array<float, kN> v;
    SrgbLut() {
        for (int i = 0; i < kN; ++i) {
            const float x = float(i) / float(kN - 1);
            v[i] = (x <= 0.0031308f)
                ? x * 12.92f
                : 1.055f * std::pow(x, 1.0f / 2.4f) - 0.055f;
        }
    }
    inline float at(float x) const {
        if (!(x > 0.0f)) return 0.0f;
        if (x >= 1.0f) return 1.0f;
        int i = int(x * float(kN - 1) + 0.5f);
        if (i < 0) i = 0;
        if (i >= kN) i = kN - 1;
        return v[i];
    }
};

inline const SrgbLut &srgbLut() {
    static const SrgbLut lut;
    return lut;
}

struct TanhLut {
    static constexpr int kN = 2048;
    static constexpr float kMax = 12.0f;
    std::array<float, kN> v;
    TanhLut() {
        for (int i = 0; i < kN; ++i)
            v[i] = std::tanh(kMax * float(i) / float(kN - 1));
    }
    inline float at(float x) const {
        if (x <= 0.0f) return 0.0f;
        if (x >= kMax) return 1.0f;
        const float t = x * float(kN - 1) / kMax;
        int i = int(t);
        if (i < 0) i = 0;
        if (i > kN - 2) i = kN - 2;
        const float f = t - float(i);
        return v[i] + f * (v[i + 1] - v[i]);
    }
};

inline const TanhLut &tanhLut() {
    static const TanhLut lut;
    return lut;
}

// ---------------------------------------------------------------------------
// Per-call transfer LUT descriptor.
// ---------------------------------------------------------------------------
struct TransferLut {
    std::vector<float> values;
};

void buildHalfLut(TransferLut &lut, InputTransfer t, float invWhite) {
    lut.values.resize(65536);
    for (int i = 0; i < 65536; ++i) {
        const qfloat16 h = halfFromBits(quint16(i));
        float f = float(h);
        if (!std::isfinite(f)) f = 0.0f;
        lut.values[i] = decodeTransferAbsolute(t, f) * invWhite;
    }
}

void buildInt16Lut(TransferLut &lut, InputTransfer t, float invWhite) {
    lut.values.resize(65536);
    for (int i = 0; i < 65536; ++i)
        lut.values[i] = decodeTransferAbsolute(t, float(i) / 65535.0f) * invWhite;
}

void buildByteLut(TransferLut &lut, InputTransfer t, float invWhite) {
    lut.values.resize(256);
    for (int i = 0; i < 256; ++i)
        lut.values[i] = decodeTransferAbsolute(t, float(i) / 255.0f) * invWhite;
}

// ---------------------------------------------------------------------------
// AVX2 primitives.
// ---------------------------------------------------------------------------
inline void mul3x3(__m256 &r, __m256 &g, __m256 &b, const float m[3][3]) {
    const __m256 r0 = _mm256_fmadd_ps(_mm256_set1_ps(m[0][0]), r,
                       _mm256_fmadd_ps(_mm256_set1_ps(m[0][1]), g,
                                        _mm256_mul_ps(_mm256_set1_ps(m[0][2]), b)));
    const __m256 g0 = _mm256_fmadd_ps(_mm256_set1_ps(m[1][0]), r,
                       _mm256_fmadd_ps(_mm256_set1_ps(m[1][1]), g,
                                        _mm256_mul_ps(_mm256_set1_ps(m[1][2]), b)));
    const __m256 b0 = _mm256_fmadd_ps(_mm256_set1_ps(m[2][0]), r,
                       _mm256_fmadd_ps(_mm256_set1_ps(m[2][1]), g,
                                        _mm256_mul_ps(_mm256_set1_ps(m[2][2]), b)));
    r = r0; g = g0; b = b0;
}

inline void compressGamut(__m256 &r, __m256 &g, __m256 &b) {
    const __m256 zero = _mm256_setzero_ps();
    const __m256 luma = _mm256_fmadd_ps(_mm256_set1_ps(kLumaR), r,
                        _mm256_fmadd_ps(_mm256_set1_ps(kLumaG), g,
                                         _mm256_mul_ps(_mm256_set1_ps(kLumaB), b)));
    const __m256 minC = _mm256_min_ps(_mm256_min_ps(r, g), b);
    const __m256 s = _mm256_div_ps(luma, _mm256_sub_ps(luma, minC));
    const __m256 nr = _mm256_fmadd_ps(s, _mm256_sub_ps(r, luma), luma);
    const __m256 ng = _mm256_fmadd_ps(s, _mm256_sub_ps(g, luma), luma);
    const __m256 nb = _mm256_fmadd_ps(s, _mm256_sub_ps(b, luma), luma);
    const __m256 mLt = _mm256_cmp_ps(minC, zero, _CMP_LT_OS);
    r = _mm256_blendv_ps(r, nr, mLt);
    g = _mm256_blendv_ps(g, ng, mLt);
    b = _mm256_blendv_ps(b, nb, mLt);
    const __m256 lGt = _mm256_cmp_ps(luma, zero, _CMP_GT_OS);
    r = _mm256_and_ps(r, lGt);
    g = _mm256_and_ps(g, lGt);
    b = _mm256_and_ps(b, lGt);
}

inline __m256 tanhGather(__m256 x, const float *lut, int n, float maxV) {
    const __m256 zero = _mm256_setzero_ps();
    x = _mm256_min_ps(_mm256_max_ps(x, zero), _mm256_set1_ps(maxV));
    const float scale = float(n - 1) / maxV;
    const __m256 t = _mm256_mul_ps(x, _mm256_set1_ps(scale));
    __m256i idx = _mm256_cvttps_epi32(t);
    idx = _mm256_min_epi32(idx, _mm256_set1_epi32(n - 2));
    idx = _mm256_max_epi32(idx, _mm256_setzero_si256());
    const __m256 frac = _mm256_sub_ps(t, _mm256_cvtepi32_ps(idx));
    const __m256 v0 = _mm256_i32gather_ps(lut, idx, 4);
    const __m256 v1 = _mm256_i32gather_ps(lut,
        _mm256_add_epi32(idx, _mm256_set1_epi32(1)), 4);
    return _mm256_fmadd_ps(frac, _mm256_sub_ps(v1, v0), v0);
}

inline void highlightDesat(__m256 &r, __m256 &g, __m256 &b) {
    const __m256 one = _mm256_set1_ps(1.0f);
    const __m256 zero = _mm256_setzero_ps();
    const __m256 maxC = _mm256_max_ps(_mm256_max_ps(r, g), b);
    const __m256 l2 = _mm256_fmadd_ps(_mm256_set1_ps(kLumaR), r,
                      _mm256_fmadd_ps(_mm256_set1_ps(kLumaG), g,
                                       _mm256_mul_ps(_mm256_set1_ps(kLumaB), b)));
    const __m256 m2 = _mm256_and_ps(
        _mm256_cmp_ps(maxC, one, _CMP_GT_OS),
        _mm256_cmp_ps(maxC, l2, _CMP_GT_OS));
    __m256 s = _mm256_div_ps(_mm256_sub_ps(one, l2), _mm256_sub_ps(maxC, l2));
    s = _mm256_min_ps(_mm256_max_ps(s, zero), one);
    const __m256 nr = _mm256_fmadd_ps(s, _mm256_sub_ps(r, l2), l2);
    const __m256 ng = _mm256_fmadd_ps(s, _mm256_sub_ps(g, l2), l2);
    const __m256 nb = _mm256_fmadd_ps(s, _mm256_sub_ps(b, l2), l2);
    r = _mm256_blendv_ps(r, nr, m2);
    g = _mm256_blendv_ps(g, ng, m2);
    b = _mm256_blendv_ps(b, nb, m2);
}

inline void tmBt2408(__m256 &r, __m256 &g, __m256 &b, const float *tanhTable) {
    const __m256 knee = _mm256_set1_ps(kBt2408Knee);
    const __m256 range = _mm256_set1_ps(1.0f - kBt2408Knee);
    const __m256 invRange = _mm256_set1_ps(1.0f / (1.0f - kBt2408Knee));
    const __m256 eps = _mm256_set1_ps(kEpsilon);

    const __m256 luma = _mm256_fmadd_ps(_mm256_set1_ps(kLumaR), r,
                        _mm256_fmadd_ps(_mm256_set1_ps(kLumaG), g,
                                         _mm256_mul_ps(_mm256_set1_ps(kLumaB), b)));
    const __m256 t = _mm256_mul_ps(_mm256_sub_ps(luma, knee), invRange);
    const __m256 th = tanhGather(t, tanhTable, 2048, 12.0f);
    __m256 mapped = _mm256_fmadd_ps(range, th, knee);
    mapped = _mm256_blendv_ps(mapped, luma,
        _mm256_cmp_ps(luma, knee, _CMP_LE_OS));
    const __m256 ratio = _mm256_div_ps(mapped, _mm256_max_ps(luma, eps));
    const __m256 lGt = _mm256_cmp_ps(luma, eps, _CMP_GT_OS);
    r = _mm256_blendv_ps(r, _mm256_mul_ps(r, ratio), lGt);
    g = _mm256_blendv_ps(g, _mm256_mul_ps(g, ratio), lGt);
    b = _mm256_blendv_ps(b, _mm256_mul_ps(b, ratio), lGt);
    highlightDesat(r, g, b);
}

inline void tmReinhardJodie(__m256 &r, __m256 &g, __m256 &b) {
    const __m256 one = _mm256_set1_ps(1.0f);
    const __m256 luma = _mm256_fmadd_ps(_mm256_set1_ps(kLumaR), r,
                        _mm256_fmadd_ps(_mm256_set1_ps(kLumaG), g,
                                         _mm256_mul_ps(_mm256_set1_ps(kLumaB), b)));
    auto ch = [&](__m256 c) {
        const __m256 tc = _mm256_div_ps(c, _mm256_add_ps(one, c));
        const __m256 tl = _mm256_div_ps(c, _mm256_add_ps(one, luma));
        return _mm256_fmadd_ps(tc, _mm256_sub_ps(tc, tl), tl);
    };
    r = ch(r); g = ch(g); b = ch(b);
}

inline void tmAces(__m256 &r, __m256 &g, __m256 &b) {
    const __m256 eps = _mm256_set1_ps(kEpsilon);
    auto op = [](__m256 v) {
        const __m256 num = _mm256_mul_ps(v,
            _mm256_fmadd_ps(_mm256_set1_ps(kAcesA), v, _mm256_set1_ps(kAcesB)));
        const __m256 den = _mm256_fmadd_ps(v,
            _mm256_fmadd_ps(_mm256_set1_ps(kAcesC), v, _mm256_set1_ps(kAcesD)),
            _mm256_set1_ps(kAcesE));
        return _mm256_div_ps(num, den);
    };
    const __m256 luma = _mm256_fmadd_ps(_mm256_set1_ps(kLumaR), r,
                        _mm256_fmadd_ps(_mm256_set1_ps(kLumaG), g,
                                         _mm256_mul_ps(_mm256_set1_ps(kLumaB), b)));
    __m256 mapped = op(luma);
    mapped = _mm256_min_ps(_mm256_max_ps(mapped, _mm256_setzero_ps()),
                           _mm256_set1_ps(1.0f));
    const __m256 ratio = _mm256_div_ps(mapped, _mm256_max_ps(luma, eps));
    const __m256 lGt = _mm256_cmp_ps(luma, eps, _CMP_GT_OS);
    r = _mm256_blendv_ps(r, _mm256_mul_ps(r, ratio), lGt);
    g = _mm256_blendv_ps(g, _mm256_mul_ps(g, ratio), lGt);
    b = _mm256_blendv_ps(b, _mm256_mul_ps(b, ratio), lGt);
    highlightDesat(r, g, b);
}

inline void tmHable(__m256 &r, __m256 &g, __m256 &b) {
    static const float invWhite = 1.0f / [] {
        const float v = kHableW;
        const float f = ((v * (kHableA * v + kHableC * kHableB) + kHableD * kHableE) /
                         (v * (kHableA * v + kHableB) + kHableD * kHableF)) -
                        (kHableE / kHableF);
        return f;
    }();
    const __m256 eps = _mm256_set1_ps(kEpsilon);
    const __m256 invWhiteV = _mm256_set1_ps(invWhite);
    auto f = [](__m256 v) {
        const __m256 num = _mm256_fmadd_ps(v,
            _mm256_fmadd_ps(_mm256_set1_ps(kHableA), v,
                            _mm256_set1_ps(kHableC * kHableB)),
            _mm256_set1_ps(kHableD * kHableE));
        const __m256 den = _mm256_fmadd_ps(v,
            _mm256_fmadd_ps(_mm256_set1_ps(kHableA), v, _mm256_set1_ps(kHableB)),
            _mm256_set1_ps(kHableD * kHableF));
        return _mm256_sub_ps(_mm256_div_ps(num, den),
                             _mm256_set1_ps(kHableE / kHableF));
    };
    const __m256 luma = _mm256_fmadd_ps(_mm256_set1_ps(kLumaR), r,
                        _mm256_fmadd_ps(_mm256_set1_ps(kLumaG), g,
                                         _mm256_mul_ps(_mm256_set1_ps(kLumaB), b)));
    __m256 mapped = _mm256_mul_ps(f(luma), invWhiteV);
    mapped = _mm256_max_ps(mapped, _mm256_setzero_ps());
    const __m256 ratio = _mm256_div_ps(mapped, _mm256_max_ps(luma, eps));
    const __m256 lGt = _mm256_cmp_ps(luma, eps, _CMP_GT_OS);
    r = _mm256_blendv_ps(r, _mm256_mul_ps(r, ratio), lGt);
    g = _mm256_blendv_ps(g, _mm256_mul_ps(g, ratio), lGt);
    b = _mm256_blendv_ps(b, _mm256_mul_ps(b, ratio), lGt);
    highlightDesat(r, g, b);
}

inline void applyToneMap(ToneMapOperator op, __m256 &r, __m256 &g, __m256 &b,
                         const float *tanhTable) {
    switch (op) {
    case ToneMapOperator::Bt2408:        tmBt2408(r, g, b, tanhTable); break;
    case ToneMapOperator::ReinhardJodie: tmReinhardJodie(r, g, b); break;
    case ToneMapOperator::AcesFilmic:    tmAces(r, g, b); break;
    case ToneMapOperator::Hable:         tmHable(r, g, b); break;
    }
}

inline __m256 srgbEncode(__m256 x, const float *table) {
    const __m256 zero = _mm256_setzero_ps();
    const __m256 one = _mm256_set1_ps(1.0f);
    x = _mm256_min_ps(_mm256_max_ps(x, zero), one);
    const __m256 scaled = _mm256_fmadd_ps(x,
        _mm256_set1_ps(float(SrgbLut::kN - 1)), _mm256_set1_ps(0.5f));
    __m256i idx = _mm256_cvttps_epi32(scaled);
    idx = _mm256_min_epi32(idx, _mm256_set1_epi32(SrgbLut::kN - 1));
    idx = _mm256_max_epi32(idx, _mm256_setzero_si256());
    return _mm256_i32gather_ps(table, idx, 4);
}

inline void packAndStore(__m256 r, __m256 g, __m256 b, __m256 a,
                         uint32_t *dst, int x) {
    const __m256 scale = _mm256_set1_ps(255.0f);
    const __m256 half = _mm256_set1_ps(0.5f);
    const __m256i zero = _mm256_setzero_si256();
    const __m256i max255 = _mm256_set1_epi32(255);

    __m256i ri = _mm256_cvttps_epi32(_mm256_fmadd_ps(r, scale, half));
    __m256i gi = _mm256_cvttps_epi32(_mm256_fmadd_ps(g, scale, half));
    __m256i bi = _mm256_cvttps_epi32(_mm256_fmadd_ps(b, scale, half));
    __m256i ai = _mm256_cvttps_epi32(_mm256_fmadd_ps(a, scale, half));

    ri = _mm256_min_epi32(_mm256_max_epi32(ri, zero), max255);
    gi = _mm256_min_epi32(_mm256_max_epi32(gi, zero), max255);
    bi = _mm256_min_epi32(_mm256_max_epi32(bi, zero), max255);
    ai = _mm256_min_epi32(_mm256_max_epi32(ai, zero), max255);

    const __m256i packed = _mm256_or_si256(
        _mm256_or_si256(_mm256_slli_epi32(ai, 24), _mm256_slli_epi32(ri, 16)),
        _mm256_or_si256(_mm256_slli_epi32(gi, 8), bi));

    _mm256_storeu_si256(reinterpret_cast<__m256i *>(dst + x), packed);
}

inline void coreAndStore(__m256 r, __m256 g, __m256 b, __m256 a,
                         InputPrimaries primaries, ToneMapOperator op,
                         const float *srgbTable, const float *tanhTable,
                         uint32_t *dst, int x) {
    if (primaries == InputPrimaries::Bt2020)
        mul3x3(r, g, b, kBt2020ToSrgb);
    else if (primaries == InputPrimaries::DisplayP3)
        mul3x3(r, g, b, kP3ToSrgb);

    compressGamut(r, g, b);
    applyToneMap(op, r, g, b, tanhTable);
    r = srgbEncode(r, srgbTable);
    g = srgbEncode(g, srgbTable);
    b = srgbEncode(b, srgbTable);
    packAndStore(r, g, b, a, dst, x);
}

// FP16 non-premultiplied RGBA.
inline void loadHalfRow8(const quint16 *src, int x,
                         const float *transferTable,
                         __m256 &r, __m256 &g, __m256 &b, __m256 &a) {
    const int *base = reinterpret_cast<const int *>(src + x * 4);
    const __m256i idx = _mm256_setr_epi32(0, 2, 4, 6, 8, 10, 12, 14);

    const __m256i rg = _mm256_i32gather_epi32(base, idx, 4);
    const __m256i ba = _mm256_i32gather_epi32(base + 1, idx, 4);

    const __m256i rb = _mm256_and_si256(rg, _mm256_set1_epi32(0xFFFF));
    const __m256i gb = _mm256_srli_epi32(rg, 16);
    const __m256i bb = _mm256_and_si256(ba, _mm256_set1_epi32(0xFFFF));
    const __m256i ab = _mm256_srli_epi32(ba, 16);

    r = _mm256_i32gather_ps(transferTable, rb, 4);
    g = _mm256_i32gather_ps(transferTable, gb, 4);
    b = _mm256_i32gather_ps(transferTable, bb, 4);

    const __m128i a16 = _mm_packus_epi32(_mm256_castsi256_si128(ab),
                                         _mm256_extracti128_si256(ab, 1));
    a = _mm256_cvtph_ps(a16);
    a = _mm256_min_ps(_mm256_max_ps(a, _mm256_setzero_ps()),
                      _mm256_set1_ps(1.0f));
}

// FP32 non-premultiplied RGBA.
inline void loadFloatRow8(const float *src, int x,
                          const float *transferTable,
                          __m256 &r, __m256 &g, __m256 &b, __m256 &a) {
    const float *p = src + x * 4;
    const __m256i idxR = _mm256_setr_epi32(0, 4, 8, 12, 16, 20, 24, 28);
    const __m256i idxG = _mm256_setr_epi32(1, 5, 9, 13, 17, 21, 25, 29);
    const __m256i idxB = _mm256_setr_epi32(2, 6, 10, 14, 18, 22, 26, 30);
    const __m256i idxA = _mm256_setr_epi32(3, 7, 11, 15, 19, 23, 27, 31);

    const __m256 rf = _mm256_i32gather_ps(p, idxR, 4);
    const __m256 gf = _mm256_i32gather_ps(p, idxG, 4);
    const __m256 bf = _mm256_i32gather_ps(p, idxB, 4);
    const __m256 af = _mm256_i32gather_ps(p, idxA, 4);

    const __m128i rh = _mm256_cvtps_ph(rf, _MM_FROUND_TO_NEAREST_INT);
    const __m128i gh = _mm256_cvtps_ph(gf, _MM_FROUND_TO_NEAREST_INT);
    const __m128i bh = _mm256_cvtps_ph(bf, _MM_FROUND_TO_NEAREST_INT);

    const __m256i ri = _mm256_cvtepu16_epi32(rh);
    const __m256i gi = _mm256_cvtepu16_epi32(gh);
    const __m256i bi = _mm256_cvtepu16_epi32(bh);

    r = _mm256_i32gather_ps(transferTable, ri, 4);
    g = _mm256_i32gather_ps(transferTable, gi, 4);
    b = _mm256_i32gather_ps(transferTable, bi, 4);
    a = _mm256_min_ps(_mm256_max_ps(af, _mm256_setzero_ps()),
                      _mm256_set1_ps(1.0f));
}

// 16-bit unsigned integer RGBA (QRgba64) non-premultiplied.
inline void loadQRgba64Row8(const quint16 *src, int x,
                            const float *transferTable,
                            __m256 &r, __m256 &g, __m256 &b, __m256 &a) {
    const int *base = reinterpret_cast<const int *>(src + x * 4);
    const __m256i idx = _mm256_setr_epi32(0, 2, 4, 6, 8, 10, 12, 14);

    const __m256i rg = _mm256_i32gather_epi32(base, idx, 4);
    const __m256i ba = _mm256_i32gather_epi32(base + 1, idx, 4);

    const __m256i rb = _mm256_and_si256(rg, _mm256_set1_epi32(0xFFFF));
    const __m256i gb = _mm256_srli_epi32(rg, 16);
    const __m256i bb = _mm256_and_si256(ba, _mm256_set1_epi32(0xFFFF));
    const __m256i ab = _mm256_srli_epi32(ba, 16);

    r = _mm256_i32gather_ps(transferTable, rb, 4);
    g = _mm256_i32gather_ps(transferTable, gb, 4);
    b = _mm256_i32gather_ps(transferTable, bb, 4);
    a = _mm256_div_ps(_mm256_cvtepi32_ps(ab), _mm256_set1_ps(65535.0f));
}

// ---------------------------------------------------------------------------
// Scalar reference for the tail and the 8-bit fallback path.
// ---------------------------------------------------------------------------
inline uint8_t floatToByte(float v) {
    int i = int(srgbLut().at(v) * 255.0f + 0.5f);
    if (i < 0) i = 0;
    if (i > 255) i = 255;
    return uint8_t(i);
}

inline float toneMapBt2408Scalar(float x) {
    if (x <= kBt2408Knee) return x;
    const float range = 1.0f - kBt2408Knee;
    return kBt2408Knee + range * tanhLut().at((x - kBt2408Knee) / range);
}

inline void highlightDesatScalar(float &r, float &g, float &b) {
    const float maxC = std::max({ r, g, b });
    if (maxC <= 1.0f) return;
    const float l2 = kLumaR * r + kLumaG * g + kLumaB * b;
    if (maxC <= l2) return;
    float s = (1.0f - l2) / (maxC - l2);
    s = std::clamp(s, 0.0f, 1.0f);
    r = l2 + s * (r - l2);
    g = l2 + s * (g - l2);
    b = l2 + s * (b - l2);
}

inline void applyToneMapScalar(ToneMapOperator op, float &r, float &g, float &b) {
    switch (op) {
    case ToneMapOperator::Bt2408: {
        const float luma = kLumaR * r + kLumaG * g + kLumaB * b;
        if (luma > kEpsilon) {
            const float ratio = toneMapBt2408Scalar(luma) / luma;
            r *= ratio; g *= ratio; b *= ratio;
        }
        highlightDesatScalar(r, g, b);
        break;
    }
    case ToneMapOperator::ReinhardJodie: {
        const float luma = kLumaR * r + kLumaG * g + kLumaB * b;
        if (luma <= 0.0f) break;
        auto ch = [luma](float c) {
            const float tc = c / (1.0f + c);
            const float tl = c / (1.0f + luma);
            return tl + tc * (tc - tl);
        };
        r = ch(r); g = ch(g); b = ch(b);
        break;
    }
    case ToneMapOperator::AcesFilmic: {
        const float luma = kLumaR * r + kLumaG * g + kLumaB * b;
        if (luma > kEpsilon) {
            const float num = luma * (kAcesA * luma + kAcesB);
            const float den = luma * (kAcesC * luma + kAcesD) + kAcesE;
            const float mapped = std::clamp(num / den, 0.0f, 1.0f);
            const float ratio = mapped / luma;
            r *= ratio; g *= ratio; b *= ratio;
        }
        highlightDesatScalar(r, g, b);
        break;
    }
    case ToneMapOperator::Hable: {
        static const float invWhite = [] {
            const float v = kHableW;
            return 1.0f / (((v * (kHableA * v + kHableC * kHableB) +
                             kHableD * kHableE) /
                            (v * (kHableA * v + kHableB) + kHableD * kHableF)) -
                           (kHableE / kHableF));
        }();
        const float luma = kLumaR * r + kLumaG * g + kLumaB * b;
        if (luma > kEpsilon) {
            const float num = luma * (kHableA * luma + kHableC * kHableB) +
                              kHableD * kHableE;
            const float den = luma * (kHableA * luma + kHableB) +
                              kHableD * kHableF;
            const float mapped = std::max((num / den - kHableE / kHableF) *
                                          invWhite, 0.0f);
            const float ratio = mapped / luma;
            r *= ratio; g *= ratio; b *= ratio;
        }
        highlightDesatScalar(r, g, b);
        break;
    }
    }
}

inline void transformPrimariesScalar(float &r, float &g, float &b,
                                     InputPrimaries p) {
    if (p == InputPrimaries::Bt2020) {
        const float rO = kBt2020ToSrgb[0][0] * r + kBt2020ToSrgb[0][1] * g + kBt2020ToSrgb[0][2] * b;
        const float gO = kBt2020ToSrgb[1][0] * r + kBt2020ToSrgb[1][1] * g + kBt2020ToSrgb[1][2] * b;
        const float bO = kBt2020ToSrgb[2][0] * r + kBt2020ToSrgb[2][1] * g + kBt2020ToSrgb[2][2] * b;
        r = rO; g = gO; b = bO;
    } else if (p == InputPrimaries::DisplayP3) {
        const float rO = kP3ToSrgb[0][0] * r + kP3ToSrgb[0][1] * g + kP3ToSrgb[0][2] * b;
        const float gO = kP3ToSrgb[1][0] * r + kP3ToSrgb[1][1] * g + kP3ToSrgb[1][2] * b;
        const float bO = kP3ToSrgb[2][0] * r + kP3ToSrgb[2][1] * g + kP3ToSrgb[2][2] * b;
        r = rO; g = gO; b = bO;
    }
}

inline void compressGamutScalar(float &r, float &g, float &b) {
    const float luma = kLumaR * r + kLumaG * g + kLumaB * b;
    if (luma <= 0.0f) { r = g = b = 0.0f; return; }
    const float minC = std::min({ r, g, b });
    if (minC < 0.0f) {
        const float s = luma / (luma - minC);
        r = luma + s * (r - luma);
        g = luma + s * (g - luma);
        b = luma + s * (b - luma);
    }
}

// ---------------------------------------------------------------------------
// Worker task: one chunk of scanlines. AVX2 for the three HDR paths.
// ---------------------------------------------------------------------------
class ToneMapTask : public QRunnable {
public:
    ToneMapTask(int yStart, int yEnd, const QImage &src, QImage &dst,
                const TransferLut &lut, InputTransfer transfer,
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

        const bool isHalfFloat = (srcFormat == QImage::Format_RGBX16FPx4 ||
                                  srcFormat == QImage::Format_RGBA16FPx4 ||
                                  srcFormat == QImage::Format_RGBA16FPx4_Premultiplied);
        const bool isFullFloat = (srcFormat == QImage::Format_RGBX32FPx4 ||
                                  srcFormat == QImage::Format_RGBA32FPx4 ||
                                  srcFormat == QImage::Format_RGBA32FPx4_Premultiplied);
        const bool is16BitInt = (srcFormat == QImage::Format_RGBA64 ||
                                 srcFormat == QImage::Format_RGBX64 ||
                                 srcFormat == QImage::Format_RGBA64_Premultiplied);
        const bool isPremul = (srcFormat == QImage::Format_RGBA16FPx4_Premultiplied ||
                               srcFormat == QImage::Format_RGBA32FPx4_Premultiplied ||
                               srcFormat == QImage::Format_RGBA64_Premultiplied);

        const float *srgbTable = srgbLut().v.data();
        const float *tanhTable = tanhLut().v.data();
        const float *transferTable = m_lut.values.data();

        for (int y = m_yStart; y < m_yEnd; ++y) {
            uint32_t *dstLine = reinterpret_cast<uint32_t *>(m_dst.scanLine(y));
            const uchar *srcLine = m_src.constScanLine(y);

            if ((isHalfFloat || isFullFloat || is16BitInt) &&
                !isPremul && width >= 8) {
                auto processRow = [&](int x) {
                    __m256 r, g, b, a;
                    if (isHalfFloat) {
                        loadHalfRow8(reinterpret_cast<const quint16 *>(srcLine),
                                     x, transferTable, r, g, b, a);
                    } else if (isFullFloat) {
                        loadFloatRow8(reinterpret_cast<const float *>(srcLine),
                                      x, transferTable, r, g, b, a);
                    } else {
                        loadQRgba64Row8(reinterpret_cast<const quint16 *>(srcLine),
                                        x, transferTable, r, g, b, a);
                    }
                    coreAndStore(r, g, b, a, m_primaries, m_op,
                                 srgbTable, tanhTable, dstLine, x);
                };

                int x = 0;
                for (; x + 8 <= width; x += 8) processRow(x);
                if (x < width) processRow(width - 8);
                continue;
            }

            // Scalar fallback: premultiplied, 8-bit, or very narrow rows.
            for (int x = 0; x < width; ++x) {
                float r, g, b, a;
                if (isHalfFloat) {
                    const qfloat16 *p = reinterpret_cast<const qfloat16 *>(srcLine) +
                                        x * kFloatChannelsPerPixel;
                    r = float(p[0]); g = float(p[1]); b = float(p[2]); a = float(p[3]);
                    if (isPremul) {
                        if (a > kEpsilon) { r /= a; g /= a; b /= a; }
                        else { r = g = b = 0.0f; }
                    }
                    r = transferTable[halfToBits(p[0])];
                    g = transferTable[halfToBits(p[1])];
                    b = transferTable[halfToBits(p[2])];
                } else if (isFullFloat) {
                    const float *p = reinterpret_cast<const float *>(srcLine) +
                                     x * kFloatChannelsPerPixel;
                    r = p[0]; g = p[1]; b = p[2]; a = p[3];
                    if (isPremul) {
                        if (a > kEpsilon) { r /= a; g /= a; b /= a; }
                        else { r = g = b = 0.0f; }
                    }
                    qfloat16 hr(std::clamp(r, 0.0f, 65504.0f));
                    qfloat16 hg(std::clamp(g, 0.0f, 65504.0f));
                    qfloat16 hb(std::clamp(b, 0.0f, 65504.0f));
                    r = transferTable[halfToBits(hr)];
                    g = transferTable[halfToBits(hg)];
                    b = transferTable[halfToBits(hb)];
                } else if (is16BitInt) {
                    const QRgba64 *p = reinterpret_cast<const QRgba64 *>(srcLine) + x;
                    const float af = p->alpha() / 65535.0f;
                    if (isPremul) {
                        if (af > kEpsilon) {
                            const quint16 rr = quint16(std::clamp(p->red()   / af, 0.0f, 65535.0f));
                            const quint16 gg = quint16(std::clamp(p->green() / af, 0.0f, 65535.0f));
                            const quint16 bb = quint16(std::clamp(p->blue()  / af, 0.0f, 65535.0f));
                            r = transferTable[rr];
                            g = transferTable[gg];
                            b = transferTable[bb];
                        } else {
                            r = g = b = 0.0f;
                        }
                    } else {
                        r = transferTable[p->red()];
                        g = transferTable[p->green()];
                        b = transferTable[p->blue()];
                    }
                    a = af;
                } else {
                    const QRgb px = m_src.pixel(x, y);
                    r = transferTable[qRed(px)];
                    g = transferTable[qGreen(px)];
                    b = transferTable[qBlue(px)];
                    a = qAlpha(px) / 255.0f;
                }

                transformPrimariesScalar(r, g, b, m_primaries);
                compressGamutScalar(r, g, b);
                applyToneMapScalar(m_op, r, g, b);

                const uint8_t alphaByte = static_cast<uint8_t>(
                    std::clamp(a, 0.0f, 1.0f) * 255.0f + 0.5f);
                dstLine[x] = qRgba(floatToByte(r), floatToByte(g),
                                   floatToByte(b), alphaByte);
            }
        }
        m_semaphore.release();
    }

private:
    int m_yStart;
    int m_yEnd;
    const QImage &m_src;
    QImage &m_dst;
    const TransferLut &m_lut;
    InputTransfer m_transfer;
    InputPrimaries m_primaries;
    ToneMapOperator m_op;
    float m_targetWhiteNits;
    QSemaphore &m_semaphore;
};

} // namespace

// ---------------------------------------------------------------------------
// Public API.
// ---------------------------------------------------------------------------
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
    if (image.isNull()) return false;

    if (image.text(QStringLiteral("HDR_IsHDR")) == QStringLiteral("true") ||
        !image.text(QStringLiteral("HDR_Profile")).isEmpty()) {
        return true;
    }
    if (isLinearFloatFormat(image.format())) return true;

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
    if (image.isNull()) return QString();

    const QString tagged = image.text(QStringLiteral("HDR_Profile"));
    if (!tagged.isEmpty()) return tagged;

    const QColorSpace cs = image.colorSpace();
    if (!cs.isValid()) return QString();

    const QColorSpace::TransferFunction tf = cs.transferFunction();
    const QColorSpace::Primaries prim = cs.primaries();

    if (tf == QColorSpace::TransferFunction::St2084) {
        if (prim == QColorSpace::Primaries::Bt2020)
            return QStringLiteral("Rec.2100 PQ (HDR10)");
        if (prim == QColorSpace::Primaries::DciP3D65)
            return QStringLiteral("Display P3 PQ");
        return QStringLiteral("SMPTE ST 2084 (PQ)");
    }
    if (tf == QColorSpace::TransferFunction::Hlg) {
        if (prim == QColorSpace::Primaries::Bt2020)
            return QStringLiteral("BT.2100 HLG");
        if (prim == QColorSpace::Primaries::DciP3D65)
            return QStringLiteral("Display P3 HLG");
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

QImage HdrToneMapper::applyToneMapping(const QImage &srcImage,
                                       const HdrToneMapParams &params) {
    if (srcImage.isNull()) return srcImage;

    InputTransfer transfer = InputTransfer::PQ;
    const QString transferText = srcImage.text(QStringLiteral("HDR_Transfer"));
    const QColorSpace cs = srcImage.colorSpace();

    if (transferText.compare(QStringLiteral("HLG"), Qt::CaseInsensitive) == 0 ||
        (cs.isValid() && cs.transferFunction() == QColorSpace::TransferFunction::Hlg)) {
        transfer = InputTransfer::HLG;
    } else if (transferText.compare(QStringLiteral("Linear"), Qt::CaseInsensitive) == 0 ||
               (cs.isValid() && cs.transferFunction() == QColorSpace::TransferFunction::Linear) ||
               (!cs.isValid() && isLinearFloatFormat(srcImage.format()))) {
        transfer = InputTransfer::Linear;
    }

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

    const float targetWhite = (params.targetWhiteNits > 0.0f)
        ? params.targetWhiteNits : 203.0f;
    const float invWhite = 1.0f / targetWhite;

    TransferLut transferLut;
    const QImage::Format srcFmt = srcImage.format();
    if (isLinearFloatFormat(srcFmt)) {
        buildHalfLut(transferLut, transfer, invWhite);
    } else if (srcFmt == QImage::Format_RGBA64 ||
               srcFmt == QImage::Format_RGBX64 ||
               srcFmt == QImage::Format_RGBA64_Premultiplied) {
        buildInt16Lut(transferLut, transfer, invWhite);
    } else {
        buildByteLut(transferLut, transfer, invWhite);
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

    const int threadCount = std::clamp(
        QThreadPool::globalInstance()->maxThreadCount(), 1, 64);
    const int linesPerChunk = std::max(8, (height + threadCount - 1) / threadCount);
    const int chunkCount = (height + linesPerChunk - 1) / linesPerChunk;

    QSemaphore semaphore(0);
    for (int i = 0; i < chunkCount; ++i) {
        const int yStart = i * linesPerChunk;
        const int yEnd = std::min(yStart + linesPerChunk, height);
        auto *task = new ToneMapTask(yStart, yEnd, srcImage, dstImage,
                                     transferLut, transfer, primaries,
                                     params.op, targetWhite, semaphore);
        QThreadPool::globalInstance()->start(task);
    }
    semaphore.acquire(chunkCount);

    dstImage.setColorSpace(QColorSpace(QColorSpace::SRgb));

    for (const QString &key : srcImage.textKeys()) {
        if (!key.startsWith(QStringLiteral("HDR_"))) {
            dstImage.setText(key, srcImage.text(key));
        }
    }

    return dstImage;
}
