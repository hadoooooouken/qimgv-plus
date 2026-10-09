#version 440

// ImageRenderer's source conversion pass: a unit quad covering level 0 of the
// tile's displayed texture. dstPos is the destination position in texels;
// mvp maps texel row 0 to the texture row that is sampled at v = 0 on every
// QRhi backend.

layout(location = 0) in vec2 corner;

layout(location = 0) out vec2 dstPos;

// Mirrored by ConvertUniforms in gui/quick/render/rhipassuniforms.h;
// identical in convert.frag.
layout(std140, binding = 0) uniform ConvertParams {
    mat4 mvp;
    vec4 primariesRow0;
    vec4 primariesRow1;
    vec4 primariesRow2;
    vec4 colorRow0;
    vec4 colorRow1;
    vec4 colorRow2;
    vec4 sourceCurve0;
    vec4 sourceCurve1;
    vec4 targetCurve0;
    vec4 targetCurve1;
    vec2 dstSize;
    float whiteScale;
    float lutScale;
    float lutOffset;
    int sourceMode;
    int hdrTransfer;
    int toneMapOperator;
    int colorMode;
};

void main()
{
    dstPos = corner * dstSize;
    gl_Position = mvp * vec4(dstPos, 0.0, 1.0);
}
