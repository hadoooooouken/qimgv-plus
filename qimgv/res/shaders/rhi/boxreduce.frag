#version 440

// Exact-area box filter, port of the widget viewer's res/shaders/boxreduce.frag.
// For this destination texel, find the source interval it covers (its
// footprint) and weight every source texel by how much of it overlaps that
// footprint. A plain 2x reduction equals one mip level; any ratio in
// [1/3, 1] is supported, so a chain of passes reaches an arbitrary target
// size instead of snapping to the nearest mip level. Alpha is premultiplied, so
// the plain average is correct.

layout(location = 0) in vec2 dstPos;

layout(location = 0) out vec4 fragColor;

// Mirrored by ReduceUniforms in gui/quick/render/imagerenderer.cpp;
// identical in boxreduce.vert.
layout(std140, binding = 0) uniform ReduceParams {
    mat4 mvp;
    vec2 srcTexelSize;
    vec2 dstSize;
    // Destination size / source size per axis, in [1/3, 1].
    vec2 ratio;
};

layout(binding = 1) uniform sampler2D srcTexture;

// ImageRenderer issues ratios in [0.5, 1] for the exact downsample (sizes
// halved rounding up) and floor(n / 2) / n for mip levels, at least 1/3 (a
// side of 3 reduced to 1). Four taps per axis cover every such footprint: up
// to 2.5 texels at any alignment, and the 3-texel footprint of 3 -> 1, which
// starts on a texel edge.
const int kTapsPerAxis = 4;
const float kPixelCentre = 0.5;
// Guards the division; every footprint overlaps at least one texel.
const float kMinWeightSum = 1e-6;

float axisOverlap(float srcIndex, float center, float halfFootprint)
{
    float lo = max(srcIndex - kPixelCentre, center - halfFootprint);
    float hi = min(srcIndex + kPixelCentre, center + halfFootprint);
    return max(hi - lo, 0.0);
}

void main()
{
    // dstPos is at the destination texel centre (index + 0.5).
    vec2 center = dstPos / ratio - vec2(kPixelCentre);
    vec2 halfFootprint = vec2(kPixelCentre) / ratio;
    vec2 k0 = floor(center - halfFootprint);

    vec4 accum = vec4(0.0);
    float wsum = 0.0;
    for (int iy = 0; iy < kTapsPerAxis; ++iy) {
        float ky = k0.y + float(iy);
        float wy = axisOverlap(ky, center.y, halfFootprint.y);
        for (int ix = 0; ix < kTapsPerAxis; ++ix) {
            float kx = k0.x + float(ix);
            float w = axisOverlap(kx, center.x, halfFootprint.x) * wy;
            vec2 uv = (vec2(kx, ky) + vec2(kPixelCentre)) * srcTexelSize;
            // Level 0 only: the first pass reads the mipmapped tile texture.
            accum += textureLod(srcTexture, uv, 0.0) * w;
            wsum += w;
        }
    }
    fragColor = accum / max(wsum, kMinWeightSum);
}
