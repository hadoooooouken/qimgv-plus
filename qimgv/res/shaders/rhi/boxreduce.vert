#version 440

// One pass of ImageRenderer's exact-ratio downsample: a unit quad covering
// the whole destination texture. dstPos is the destination position in
// texels; mvp maps texel row 0 to the texture row that is sampled at v = 0 on
// every QRhi backend.

layout(location = 0) in vec2 corner;

layout(location = 0) out vec2 dstPos;

// Mirrored by ReduceUniforms in gui/quick/render/rhipassuniforms.h;
// identical in boxreduce.frag.
layout(std140, binding = 0) uniform ReduceParams {
    mat4 mvp;
    vec2 srcTexelSize;
    vec2 dstSize;
    vec2 ratio;
};

void main()
{
    dstPos = corner * dstSize;
    gl_Position = mvp * vec4(dstPos, 0.0, 1.0);
}
