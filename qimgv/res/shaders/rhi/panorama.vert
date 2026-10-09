#version 440

// Panorama mode of ImageRenderer: a unit quad covering the whole colour
// buffer once per image tile. screenPos runs from (0, 0) at the top-left to
// (1, 1) at the bottom-right corner of the item.

layout(location = 0) in vec2 corner;

layout(location = 0) out vec2 screenPos;

// Mirrored by PanoramaUniforms in gui/quick/render/rhipassuniforms.h;
// identical in panorama.frag.
layout(std140, binding = 0) uniform PanoramaParams {
    mat4 mvp;
    vec4 colorRow0;
    vec4 colorRow1;
    vec4 colorRow2;
    vec4 core;
    vec4 texRect;
    vec2 targetSize;
    float tanHalfFov;
    float aspect;
    float sinYaw;
    float cosYaw;
    float sinPitch;
    float cosPitch;
    float colorOffset;
    int colorEnabled;
    int wrapsHorizontally;
};

void main()
{
    screenPos = corner;
    gl_Position = mvp * vec4(corner * targetSize, 0.0, 1.0);
}
