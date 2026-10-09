#version 440

// One pass of ImageRenderer's Magic Kernel Sharp 2021 resampling: a unit
// quad covering the whole destination texture. dstPos is the destination
// position in texels; mvp maps texel row 0 to the first row of the texture
// on every QRhi backend (as in boxreduce.vert).

layout(location = 0) in vec2 corner;

layout(location = 0) out vec2 dstPos;

// Mirrored by ResampleUniforms in gui/quick/render/rhipassuniforms.h;
// identical in resample.frag.
layout(std140, binding = 0) uniform ResampleParams {
    mat4 mvp;
    vec2 dstSize;
    int axis;
    int clampMin;
    int clampMax;
    int crossOffset;
    int finalPass;
    int tapCount;
};

void main()
{
    dstPos = corner * dstSize;
    gl_Position = mvp * vec4(dstPos, 0.0, 1.0);
}
