#version 440

// One axis of the Magic Kernel Sharp 2021 resampling, the GPU port of
// ImageLib::scaled_MKS2021() in utils/imagelib.cpp. ImageRenderer runs a
// horizontal pass into an intermediate texture and a vertical pass from it.
// Alpha is premultiplied, as in the CPU version.
//
// The taps of every destination index are built on the CPU exactly like
// buildMksAxisTaps() (Mks2021Axis::weights()) and uploaded as a weight table:
// evaluating the kernel per tap in the shader made the pass ALU-bound and
// many times slower than its memory traffic.

layout(location = 0) in vec2 dstPos;

layout(location = 0) out vec4 fragColor;

// Mirrored by ResampleUniforms in gui/quick/render/imagerenderer.cpp;
// identical in resample.vert.
layout(std140, binding = 0) uniform ResampleParams {
    mat4 mvp;
    vec2 dstSize;
    // kAxisHorizontal or kAxisVertical.
    int axis;
    // Taps are clamped to source texels [clampMin, clampMax] along the axis:
    // the image edge, or the edge of a tile's texture.
    int clampMin;
    int clampMax;
    // Source texel = destination index + crossOffset across the axis.
    int crossOffset;
    // Non-zero for the pass that produces the displayed texture.
    int finalPass;
    // Rows of weight data per destination index (the most taps of any).
    int tapCount;
};

layout(binding = 1) uniform sampler2D srcTexture;
// Column d holds destination index d: row 0 the first source texel of its
// taps (relative to srcTexture), rows 1..tapCount the normalized weights,
// zero past the index's own taps.
layout(binding = 2) uniform sampler2D weightTable;

const int kAxisHorizontal = 0;
const int kFirstTapRow = 0;
const int kFirstWeightRow = 1;

void main()
{
    ivec2 index = ivec2(floor(dstPos));
    bool horizontal = axis == kAxisHorizontal;
    int along = horizontal ? index.x : index.y;
    int across = (horizontal ? index.y : index.x) + crossOffset;

    int first = int(texelFetch(weightTable, ivec2(along, kFirstTapRow), 0).r);
    vec4 accum = vec4(0.0);
    for (int t = 0; t < tapCount; ++t) {
        float w = texelFetch(weightTable, ivec2(along, kFirstWeightRow + t), 0).r;
        int tap = clamp(first + t, clampMin, clampMax);
        ivec2 texel = horizontal ? ivec2(tap, across) : ivec2(across, tap);
        accum += texelFetch(srcTexture, texel, 0) * w;
    }
    // The CPU version clamps every pass to the 8-bit range.
    vec4 color = clamp(accum, 0.0, 1.0);
    // The sharp kernel overshoots on edges of translucent pixels; keep the
    // displayed colour a valid premultiplied value.
    if (finalPass != 0)
        color.rgb = min(color.rgb, vec3(color.a));
    fragColor = color;
}
