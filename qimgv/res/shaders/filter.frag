varying highp vec2 texCoord;
uniform sampler2D tex;
uniform highp mat3 colorMatrix;
uniform highp float colorOffset;

uniform highp vec2 pixelSize;
uniform highp float casContrast;
uniform highp float casSharpening;
uniform int sharpenMode;
uniform int isDownscaling;

vec3 applyCAS(vec2 uv) {
    vec2 offX = vec2(pixelSize.x, 0.0);
    vec2 offY = vec2(0.0, pixelSize.y);

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

    vec3 rcpMRGB = 1.0 / mxRGB;
    vec3 ampRGB = clamp(min(mnRGB, 2.0 - mxRGB) * rcpMRGB, 0.0, 1.0);
    ampRGB = inversesqrt(ampRGB);

    float peak = -3.0 * casContrast + 8.0;
    vec3 wRGB = -1.0 / (ampRGB * peak);
    vec3 rcpWeightRGB = 1.0 / (4.0 * wRGB + 1.0);

    vec3 window = (b + d) + (f + h);
    vec3 outColor = clamp((window * wRGB + e) * rcpWeightRGB, 0.0, 1.0);

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
        vec3 blurred = center * 0.17 + (t1 + b1 + l1 + r1) * 0.14 + (t2 + b2 + l2 + r2) * 0.0675;
        vec3 sharpened = center + 0.18 * (center - blurred);
        return clamp(sharpened, 0.0, 1.0);
    } else {
        vec2 offX = vec2(pixelSize.x, 0.0);
        vec2 offY = vec2(0.0, pixelSize.y);
        vec3 c = texture2D(tex, uv).rgb;
        vec3 t = texture2D(tex, uv - offY).rgb;
        vec3 b = texture2D(tex, uv + offY).rgb;
        vec3 l = texture2D(tex, uv - offX).rgb;
        vec3 r = texture2D(tex, uv + offX).rgb;
        vec3 sharpened = c + (4.0 * c - t - b - l - r) * 0.0625;
        return clamp(sharpened, 0.0, 1.0);
    }
}

void main() {
    highp vec4 color = texture2D(tex, texCoord);
    highp vec3 rgb = color.rgb;
    if (sharpenMode == 3 && casSharpening > 0.001) {
        rgb = applyCAS(texCoord);
    } else if (sharpenMode == 4) {
        rgb = applySmartSharpenGPU(texCoord);
    }
    rgb = clamp(colorMatrix * rgb + vec3(colorOffset), 0.0, 1.0);
    gl_FragColor = vec4(rgb, color.a);
}
