varying highp vec2 texCoord;
uniform sampler2D tex;
uniform highp mat3 colorMatrix;
uniform highp float colorOffset;

uniform highp vec2 pixelSize;
uniform highp float casContrast;
uniform highp float casSharpening;
uniform int sharpenMode;
uniform int isDownscaling;

// Rec.709 luma weights, used to collapse CAS/SmartSharpen's per-channel
// contrast/amplitude estimation down to a single scalar. The original
// per-channel (vec3) math derives an independent amplitude/weight for R, G
// and B, so a saturated edge (e.g. red-orange) gets a different sharpening
// amount per channel, which shifts hue. Deriving one scalar from luma and
// applying it identically to all three channels preserves hue, since hue
// depends only on the ratios/differences between channels, not their
// absolute values.
const vec3 LUMA = vec3(0.2126, 0.7152, 0.0722);

// Floor used wherever a luma value is about to be divided by or fed into
// inversesqrt(), to avoid a divide-by-zero / Inf on a fully black window.
const highp float kMinLuma = 1e-5;

// Sharpening is only applied above this alpha (see main()). Below it, the
// CAS/SmartSharpen taps would mix premultiplied RGB from neighboring texels
// carrying their own, different alpha -- meaningless on a soft edge and
// actively harmful on fully transparent texels, where premultiplied RGB is
// (0,0,0) and contributes only noise.
const highp float kOpaqueAlphaThreshold = 0.9999;

vec3 applyCAS(vec2 uv) {
    vec2 offX = vec2(pixelSize.x, 0.0);
    vec2 offY = vec2(0.0, pixelSize.y);

    // When downscaling, trilinear (mip) filtering has already suppressed high
    // frequencies at the auto-selected mip level. A fixed 1-texel 3x3 window
    // sampled at that level mostly picks up mip-blur/quantization noise rather
    // than real detail, so CAS ends up sharpening artifacts instead of the
    // image. To fix this we widen the sampling window (same 1.2 / 2.8 texel
    // radii used by SmartSharpen's downscale path) and pull samples from a
    // sharper mip level via an explicit LOD bias, while keeping CAS's own
    // adaptive min/max/amp formula over the resulting 9 taps.
    if (isDownscaling == 1) {
        float bias = -0.7;

        vec3 e = texture2D(tex, uv, bias).rgb;

        vec3 b = texture2D(tex, uv - 1.2 * offY, bias).rgb;
        vec3 d = texture2D(tex, uv - 1.2 * offX, bias).rgb;
        vec3 f = texture2D(tex, uv + 1.2 * offX, bias).rgb;
        vec3 h = texture2D(tex, uv + 1.2 * offY, bias).rgb;

        vec3 a = texture2D(tex, uv - 2.8 * offX - 2.8 * offY, bias).rgb;
        vec3 c = texture2D(tex, uv + 2.8 * offX - 2.8 * offY, bias).rgb;
        vec3 g = texture2D(tex, uv - 2.8 * offX + 2.8 * offY, bias).rgb;
        vec3 i = texture2D(tex, uv + 2.8 * offX + 2.8 * offY, bias).rgb;

        vec3 mnRGB = min(min(min(d, e), min(f, b)), h);
        vec3 mnRGB2 = min(mnRGB, min(min(a, c), min(g, i)));
        mnRGB += mnRGB2;

        vec3 mxRGB = max(max(max(d, e), max(f, b)), h);
        vec3 mxRGB2 = max(mxRGB, max(max(a, c), max(g, i)));
        mxRGB += mxRGB2;

        // Amplitude and sharpening weight are derived from luma alone (see
        // the LUMA comment above) instead of per-channel, so all three
        // channels get the same weight and hue is preserved on saturated
        // edges.
        float mnL = dot(mnRGB, LUMA);
        float mxL = dot(mxRGB, LUMA);
        float rcpM = 1.0 / max(mxL, kMinLuma);
        float amp = clamp(min(mnL, 2.0 - mxL) * rcpM, 0.0, 1.0);
        amp = inversesqrt(max(amp, kMinLuma));

        float peak = -3.0 * casContrast + 8.0;
        float w = -1.0 / (amp * peak);
        float rcpWeight = 1.0 / (4.0 * w + 1.0);

        vec3 window = (b + d) + (f + h);
        vec3 outColor = clamp((window * w + e) * rcpWeight, 0.0, 1.0);

        return mix(e, outColor, casSharpening);
    }

    vec3 e = texture2D(tex, uv).rgb;
    vec3 b = texture2D(tex, uv - offY).rgb;
    vec3 d = texture2D(tex, uv - offX).rgb;
    vec3 f = texture2D(tex, uv + offX).rgb;
    vec3 h = texture2D(tex, uv + offY).rgb;

    vec3 a = texture2D(tex, uv - offX - offY).rgb;
    vec3 c = texture2D(tex, uv + offX - offY).rgb;
    vec3 g = texture2D(tex, uv - offX + offY).rgb;
    vec3 i = texture2D(tex, uv + offX + offY).rgb;

    vec3 mnRGB = min(min(min(d, e), min(f, b)), h);
    vec3 mnRGB2 = min(mnRGB, min(min(a, c), min(g, i)));
    mnRGB += mnRGB2;

    vec3 mxRGB = max(max(max(d, e), max(f, b)), h);
    vec3 mxRGB2 = max(mxRGB, max(max(a, c), max(g, i)));
    mxRGB += mxRGB2;

    float mnL = dot(mnRGB, LUMA);
    float mxL = dot(mxRGB, LUMA);
    float rcpM = 1.0 / max(mxL, kMinLuma);
    float amp = clamp(min(mnL, 2.0 - mxL) * rcpM, 0.0, 1.0);
    amp = inversesqrt(max(amp, kMinLuma));

    float peak = -3.0 * casContrast + 8.0;
    float w = -1.0 / (amp * peak);
    float rcpWeight = 1.0 / (4.0 * w + 1.0);

    vec3 window = (b + d) + (f + h);
    vec3 outColor = clamp((window * w + e) * rcpWeight, 0.0, 1.0);

    return mix(e, outColor, casSharpening);
}

vec3 applySmartSharpenGPU(vec2 uv) {
    if (isDownscaling == 1) {
        vec2 offX = vec2(pixelSize.x, 0.0);
        vec2 offY = vec2(0.0, pixelSize.y);
        float bias = -0.7;
        vec3 center = texture2D(tex, uv, bias).rgb;
        vec3 t1 = texture2D(tex, uv - 1.2 * offY, bias).rgb;
        vec3 b1 = texture2D(tex, uv + 1.2 * offY, bias).rgb;
        vec3 l1 = texture2D(tex, uv - 1.2 * offX, bias).rgb;
        vec3 r1 = texture2D(tex, uv + 1.2 * offX, bias).rgb;
        vec3 t2 = texture2D(tex, uv - 2.8 * offY, bias).rgb;
        vec3 b2 = texture2D(tex, uv + 2.8 * offY, bias).rgb;
        vec3 l2 = texture2D(tex, uv - 2.8 * offX, bias).rgb;
        vec3 r2 = texture2D(tex, uv + 2.8 * offX, bias).rgb;

        // Blur and the resulting delta are computed on luma only; the
        // scalar delta is then added identically to all three channels,
        // which is what keeps this hue-preserving (see the LUMA comment
        // above).
        float lc  = dot(center, LUMA);
        float lT1 = dot(t1, LUMA);
        float lB1 = dot(b1, LUMA);
        float lL1 = dot(l1, LUMA);
        float lR1 = dot(r1, LUMA);
        float lT2 = dot(t2, LUMA);
        float lB2 = dot(b2, LUMA);
        float lL2 = dot(l2, LUMA);
        float lR2 = dot(r2, LUMA);

        float blurredL = lc * 0.17
                       + (lT1 + lB1 + lL1 + lR1) * 0.14
                       + (lT2 + lB2 + lL2 + lR2) * 0.0675;
        float deltaL = 0.18 * (lc - blurredL);

        vec3 sharpened = center + deltaL;
        return clamp(sharpened, 0.0, 1.0);
    } else {
        vec2 offX = vec2(pixelSize.x, 0.0);
        vec2 offY = vec2(0.0, pixelSize.y);
        vec3 c = texture2D(tex, uv).rgb;
        vec3 t = texture2D(tex, uv - offY).rgb;
        vec3 b = texture2D(tex, uv + offY).rgb;
        vec3 l = texture2D(tex, uv - offX).rgb;
        vec3 r = texture2D(tex, uv + offX).rgb;

        float lc = dot(c, LUMA);
        float lt = dot(t, LUMA);
        float lb = dot(b, LUMA);
        float ll = dot(l, LUMA);
        float lr = dot(r, LUMA);
        float lap = 4.0 * lc - lt - lb - ll - lr;
        vec3 sharpened = c + lap * 0.0625;

        return clamp(sharpened, 0.0, 1.0);
    }
}

void main() {
    // The bound texture holds premultiplied alpha (RGB already scaled by A).
    // Sampling/filtering (including the CAS and smart-sharpen taps above,
    // which read several neighboring texels) works correctly in this space:
    // a fully transparent texel is exactly (0,0,0,0), so it can no longer
    // bleed an arbitrary baked-in color into an opaque neighbor during
    // bilinear/mipmap filtering or sharpening.
    highp vec4 color = texture2D(tex, texCoord);
    highp float a = color.a;
    highp vec3 rgb = color.rgb;

    // Sharpening is gated to fully opaque pixels (see kOpaqueAlphaThreshold
    // above main()): on a soft edge the CAS/SmartSharpen taps would mix
    // premultiplied RGB from neighbors carrying their own, different alpha.
    if (a >= kOpaqueAlphaThreshold) {
        if (sharpenMode == 3 && casSharpening > kAdjustEpsilon) {
            rgb = applyCAS(texCoord);
        } else if (sharpenMode == 4) {
            rgb = applySmartSharpenGPU(texCoord);
        }
    }

    // colorMatrix/colorOffset (exposure, contrast, brightness, etc.) are
    // defined in terms of straight color, so un-premultiply before applying
    // them, then re-premultiply the result for premultiplied-alpha blending.
    highp vec3 straightRgb = (a > 0.0001) ? rgb / a : rgb;
    straightRgb = clamp(colorMatrix * straightRgb + vec3(colorOffset), 0.0, 1.0);
    highp vec3 outRgb = straightRgb * a;

    gl_FragColor = vec4(outRgb, a);
}
