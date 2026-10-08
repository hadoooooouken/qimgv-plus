#version 440

// Samples one image tile (premultiplied alpha) and optionally composites it
// over the transparency checkerboard. The output is premultiplied and blended
// with ONE, ONE_MINUS_SRC_ALPHA over the cleared background.

layout(location = 0) in vec2 texCoord;
layout(location = 1) in vec2 devicePos;

layout(location = 0) out vec4 fragColor;

// Mirrored by TileUniforms in gui/quick/render/imagerenderer.cpp.
layout(std140, binding = 0) uniform TileParams {
    mat4 mvp;
    vec4 targetRect;
    vec4 texRect;
    vec4 checkerLight;
    vec4 checkerDark;
    vec2 checkerOrigin;
    float checkerTile;
    float checkerFirstCell;
    int checkerEnabled;
};

layout(binding = 1) uniform sampler2D imageTexture;

void main()
{
    vec4 color = texture(imageTexture, texCoord);
    if (checkerEnabled != 0) {
        // Same pattern as the widget viewer: square tiles of checkerTile
        // device pixels starting at the image's top-left corner, light in the
        // top-left and bottom-right cells.
        vec2 cell = mod(devicePos - checkerOrigin, vec2(checkerTile));
        bool light = (cell.x < checkerFirstCell) == (cell.y < checkerFirstCell);
        vec4 checker = light ? checkerLight : checkerDark;
        color += checker * (1.0 - color.a);
    }
    fragColor = color;
}
