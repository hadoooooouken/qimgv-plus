#version 440

// One thumbnail of the Qt Quick thumbnail views (ThumbnailItem), drawn as a
// textured quad over the painted rectangle in the item's coordinates.

layout(location = 0) in vec4 qt_VertexPosition;
layout(location = 1) in vec2 qt_VertexTexCoord;

layout(location = 0) out vec2 texCoord;

// Mirrored by ThumbnailUniforms in gui/quick/render/thumbnailmaterial.cpp;
// identical in thumbnail.frag.
layout(std140, binding = 0) uniform ThumbnailParams {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 rectSize;
    float cornerRadius;
    float highlight;
    float edgeWidth;
};

void main()
{
    texCoord = qt_VertexTexCoord;
    gl_Position = qt_Matrix * qt_VertexPosition;
}
