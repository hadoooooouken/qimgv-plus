#version 440

// ImageRenderer's source conversion pass: turns one source texel (straight
// alpha) into one displayed texel (premultiplied alpha, encoded for the
// display colour space). Rendered once per tile when the image or the
// conversion changes; mip levels are built from its result.
//
// HDR images are decoded and tone mapped as HdrToneMapper
// (utils/hdrtonemapper.cpp) does on the CPU, with its constants: transfer
// decode to nits, division by the white level, primaries to linear sRGB,
// gamut compression, the tone mapping operator and its highlight
// desaturation. The colour transform to the display then follows as either a
// parametric conversion (transfer curve, matrix, inverse transfer curve) or a
// 3D lookup table (see gui/quick/render/colortransformplan.h).

layout(location = 0) in vec2 dstPos;

layout(location = 0) out vec4 fragColor;

// Mirrored by ConvertUniforms in gui/quick/render/rhipassuniforms.h;
// identical in convert.vert.
layout(std140, binding = 0) uniform ConvertParams {
    mat4 mvp;
    // Rows (xyz) of the HDR primaries -> linear sRGB matrix.
    vec4 primariesRow0;
    vec4 primariesRow1;
    vec4 primariesRow2;
    // Rows (xyz) of the linear source -> linear target matrix.
    vec4 colorRow0;
    vec4 colorRow1;
    vec4 colorRow2;
    // Transfer curves as (a, b, c, d) and (e, f, g, unused): encoded x maps
    // to x < d ? c * x + f : pow(a * x + b, g) + e.
    vec4 sourceCurve0;
    vec4 sourceCurve1;
    vec4 targetCurve0;
    vec4 targetCurve1;
    vec2 dstSize;
    // 1 / white level in nits.
    float whiteScale;
    // Lookup table coordinate = encoded * lutScale + lutOffset.
    float lutScale;
    float lutOffset;
    // kSourceEncoded / kSourceToneMapped.
    int sourceMode;
    // Values of HdrTransfer (utils/hdrsource.h).
    int hdrTransfer;
    // Values of RenderEnums::ToneMapOperator.
    int toneMapOperator;
    // kColorNone / kColorParametric / kColorLut.
    int colorMode;
};

layout(binding = 1) uniform sampler2D sourceTexture;
layout(binding = 2) uniform sampler3D colorLut;

// The source holds encoded values in [0, 1] (SDR images, and HDR images with
// tone mapping off, which are clamped and shown as sRGB like the CPU's
// fallback conversion).
const int kSourceEncoded = 0;
// The source holds HDR samples that are decoded and tone mapped to linear
// sRGB.
const int kSourceToneMapped = 1;

const int kColorNone = 0;
const int kColorParametric = 1;
const int kColorLut = 2;

const int kTransferPq = 0;
const int kTransferHlg = 1;
const int kTransferLinear = 2;

const int kOperatorBt2408 = 0;
const int kOperatorReinhardJodie = 1;
const int kOperatorAcesFilmic = 2;
const int kOperatorHable = 3;

// PQ EOTF constants (SMPTE ST 2084 / Rec.2100).
const float kPqM1 = 2610.0 / 16384.0;
const float kPqM2 = (2523.0 / 4096.0) * 128.0;
const float kPqC1 = 3424.0 / 4096.0;
const float kPqC2 = (2413.0 / 4096.0) * 32.0;
const float kPqC3 = (2392.0 / 4096.0) * 32.0;
const float kPqPeakLuminanceNits = 10000.0;

// HLG EOTF constants (ARIB STD-B67 / Rec.2100).
const float kHlgA = 0.17883277;
const float kHlgB = 1.0 - 4.0 * kHlgA;
const float kHlgC = 0.55991073;
const float kHlgPeakLuminanceNits = 1000.0;
const float kHlgBreak = 0.5;
const float kHlgLowDivisor = 3.0;
const float kHlgHighDivisor = 12.0;

// scRGB reference white luminance.
const float kScRgbReferenceWhiteNits = 80.0;

// Rec.709 / sRGB luminance coefficients.
const vec3 kLuma = vec3(0.2126, 0.7152, 0.0722);

// ACES Narkowicz fit.
const float kAcesA = 2.51;
const float kAcesB = 0.03;
const float kAcesC = 2.43;
const float kAcesD = 0.59;
const float kAcesE = 0.14;

// Hable (Uncharted 2).
const float kHableA = 0.15;
const float kHableB = 0.50;
const float kHableC = 0.10;
const float kHableD = 0.20;
const float kHableE = 0.02;
const float kHableF = 0.30;
const float kHableW = 11.2;

// BT.2408 knee point.
const float kBt2408Knee = 0.75;
// tanh() argument limit, as the CPU's tanh table: tanh(12) is 1 in float,
// and some backends' tanh() overflows to NaN for large arguments.
const float kTanhMax = 12.0;

const float kEpsilon = 1e-6;

// sRGB encoding (IEC 61966-2-1).
const float kSrgbLinearBreak = 0.0031308;
const float kSrgbLinearSlope = 12.92;
const float kSrgbScale = 1.055;
const float kSrgbOffset = 0.055;
const float kSrgbInverseGamma = 1.0 / 2.4;

float finiteOrZero(float value)
{
    return (isnan(value) || isinf(value)) ? 0.0 : value;
}

float decodeToNits(float encoded)
{
    if (encoded <= 0.0)
        return 0.0;
    if (hdrTransfer == kTransferPq) {
        float v = min(encoded, 1.0);
        float vp = pow(v, 1.0 / kPqM2);
        float num = max(vp - kPqC1, 0.0);
        float den = kPqC2 - kPqC3 * vp;
        if (den <= 0.0)
            return kPqPeakLuminanceNits;
        return pow(num / den, 1.0 / kPqM1) * kPqPeakLuminanceNits;
    }
    if (hdrTransfer == kTransferHlg) {
        float v = min(encoded, 1.0);
        float lin = (v <= kHlgBreak)
            ? (v * v) / kHlgLowDivisor
            : (exp((v - kHlgC) / kHlgA) + kHlgB) / kHlgHighDivisor;
        return lin * kHlgPeakLuminanceNits;
    }
    return encoded * kScRgbReferenceWhiteNits;
}

vec3 compressGamut(vec3 c)
{
    float luma = dot(c, kLuma);
    if (luma <= 0.0)
        return vec3(0.0);
    float minC = min(min(c.r, c.g), c.b);
    if (minC < 0.0) {
        float s = luma / (luma - minC);
        c = vec3(luma) + s * (c - vec3(luma));
    }
    return c;
}

vec3 highlightDesaturate(vec3 c)
{
    float maxC = max(max(c.r, c.g), c.b);
    if (maxC <= 1.0)
        return c;
    float l2 = dot(c, kLuma);
    if (maxC <= l2)
        return c;
    float s = clamp((1.0 - l2) / (maxC - l2), 0.0, 1.0);
    return vec3(l2) + s * (c - vec3(l2));
}

// Scales the colour so that its luma becomes mapped(luma).
vec3 scaleLuma(vec3 c, float luma, float mapped)
{
    if (luma > kEpsilon)
        c *= mapped / luma;
    return highlightDesaturate(c);
}

float hableCurve(float v)
{
    return (v * (kHableA * v + kHableC * kHableB) + kHableD * kHableE) /
               (v * (kHableA * v + kHableB) + kHableD * kHableF) -
           kHableE / kHableF;
}

vec3 toneMap(vec3 c)
{
    float luma = dot(c, kLuma);
    if (toneMapOperator == kOperatorReinhardJodie) {
        if (luma <= 0.0)
            return c;
        vec3 tc = c / (vec3(1.0) + c);
        vec3 tl = c / (1.0 + luma);
        return tl + tc * (tc - tl);
    }
    if (toneMapOperator == kOperatorAcesFilmic) {
        float num = luma * (kAcesA * luma + kAcesB);
        float den = luma * (kAcesC * luma + kAcesD) + kAcesE;
        return scaleLuma(c, luma, clamp(num / den, 0.0, 1.0));
    }
    if (toneMapOperator == kOperatorHable) {
        float mapped = max(hableCurve(luma) / hableCurve(kHableW), 0.0);
        return scaleLuma(c, luma, mapped);
    }
    // kOperatorBt2408: identity below the knee, tanh roll-off above it.
    float range = 1.0 - kBt2408Knee;
    float mapped = luma <= kBt2408Knee
        ? luma
        : kBt2408Knee + range * tanh(min((luma - kBt2408Knee) / range, kTanhMax));
    return scaleLuma(c, luma, mapped);
}

vec3 srgbEncode(vec3 linear)
{
    vec3 x = clamp(linear, 0.0, 1.0);
    vec3 low = x * kSrgbLinearSlope;
    vec3 high = kSrgbScale * pow(x, vec3(kSrgbInverseGamma)) - kSrgbOffset;
    return mix(high, low, lessThanEqual(x, vec3(kSrgbLinearBreak)));
}

float curveToLinear(float x, vec4 p0, vec4 p1)
{
    if (x < p0.w)
        return p0.z * x + p1.y;
    return pow(max(p0.x * x + p0.y, 0.0), p1.z) + p1.x;
}

float curveFromLinear(float y, vec4 p0, vec4 p1)
{
    if (p0.z > 0.0 && y < p0.z * p0.w + p1.y)
        return (y - p1.y) / p0.z;
    return (pow(max(y - p1.x, 0.0), 1.0 / p1.z) - p0.y) / p0.x;
}

vec3 toTarget(vec3 linearSource)
{
    vec3 linearTarget = clamp(vec3(dot(colorRow0.xyz, linearSource),
                                   dot(colorRow1.xyz, linearSource),
                                   dot(colorRow2.xyz, linearSource)),
                              0.0, 1.0);
    return vec3(curveFromLinear(linearTarget.r, targetCurve0, targetCurve1),
                curveFromLinear(linearTarget.g, targetCurve0, targetCurve1),
                curveFromLinear(linearTarget.b, targetCurve0, targetCurve1));
}

vec3 lookUp(vec3 encoded)
{
    return textureLod(colorLut, encoded * lutScale + vec3(lutOffset), 0.0).rgb;
}

void main()
{
    vec4 source = texelFetch(sourceTexture, ivec2(dstPos), 0);
    vec3 raw = vec3(finiteOrZero(source.r), finiteOrZero(source.g),
                    finiteOrZero(source.b));
    float alpha = clamp(finiteOrZero(source.a), 0.0, 1.0);

    vec3 rgb;
    if (sourceMode == kSourceToneMapped) {
        vec3 relative = vec3(decodeToNits(raw.r), decodeToNits(raw.g),
                             decodeToNits(raw.b)) * whiteScale;
        vec3 linear = vec3(dot(primariesRow0.xyz, relative),
                           dot(primariesRow1.xyz, relative),
                           dot(primariesRow2.xyz, relative));
        linear = clamp(toneMap(compressGamut(linear)), 0.0, 1.0);
        if (colorMode == kColorParametric)
            rgb = toTarget(linear);
        else if (colorMode == kColorLut)
            rgb = lookUp(srgbEncode(linear));
        else
            rgb = srgbEncode(linear);
    } else {
        vec3 encoded = clamp(raw, 0.0, 1.0);
        if (colorMode == kColorParametric) {
            rgb = toTarget(vec3(
                curveToLinear(encoded.r, sourceCurve0, sourceCurve1),
                curveToLinear(encoded.g, sourceCurve0, sourceCurve1),
                curveToLinear(encoded.b, sourceCurve0, sourceCurve1)));
        } else if (colorMode == kColorLut) {
            rgb = lookUp(encoded);
        } else {
            rgb = encoded;
        }
    }
    fragColor = vec4(clamp(rgb, 0.0, 1.0) * alpha, alpha);
}
