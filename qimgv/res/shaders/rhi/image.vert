#version 440

// Textured quad of one image tile (ImageRenderer). The quad is a unit square
// stretched over targetRect, in device pixels of the item's colour buffer.

layout(location = 0) in vec2 corner;

layout(location = 0) out vec2 texCoord;
layout(location = 1) out vec2 devicePos;

// Mirrored by TileUniforms in gui/quick/render/rhipassuniforms.h; identical
// in image.frag.
layout(std140, binding = 0) uniform TileParams {
    mat4 mvp;
    vec4 targetRect;
    vec4 texRect;
    vec4 checkerLight;
    vec4 checkerDark;
    vec4 colorRow0;
    vec4 colorRow1;
    vec4 colorRow2;
    vec2 checkerOrigin;
    vec2 texelStep;
    float checkerTile;
    float checkerFirstCell;
    float colorOffset;
    float casSharpening;
    float casContrast;
    int checkerEnabled;
    int colorEnabled;
    int sharpenMode;
    int downscaleTaps;
};

void main()
{
    devicePos = targetRect.xy + corner * targetRect.zw;
    texCoord = texRect.xy + corner * texRect.zw;
    gl_Position = mvp * vec4(devicePos, 0.0, 1.0);
}
