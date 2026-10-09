#version 440

// Thumbnail with rounded corners and the hover highlight of the widget
// ThumbnailWidget: the corners are cut by a rounded-rectangle distance field
// with an edge of edgeWidth (one device pixel, in item units); highlight adds
// that fraction of the thumbnail to itself (QPainter's Plus composition of
// the widget), saturating at the alpha of the premultiplied colour.

layout(location = 0) in vec2 texCoord;

layout(location = 0) out vec4 fragColor;

// Mirrored by ThumbnailUniforms in gui/quick/render/thumbnailmaterial.cpp;
// identical in thumbnail.vert.
layout(std140, binding = 0) uniform ThumbnailParams {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 rectSize;
    float cornerRadius;
    float highlight;
    float edgeWidth;
};

layout(binding = 1) uniform sampler2D source;

// Signed distance from p to a rectangle of halfSize centred at the origin
// whose corners are rounded with radius.
float roundedRectDistance(vec2 p, vec2 halfSize, float radius)
{
    vec2 q = abs(p) - halfSize + vec2(radius);
    return min(max(q.x, q.y), 0.0) + length(max(q, vec2(0.0))) - radius;
}

void main()
{
    vec4 color = texture(source, texCoord);
    vec3 lit = min(color.rgb * (1.0 + highlight), vec3(color.a));
    vec2 halfSize = rectSize * 0.5;
    float distance = roundedRectDistance(texCoord * rectSize - halfSize,
                                         halfSize, cornerRadius);
    float coverage = clamp(0.5 - distance / edgeWidth, 0.0, 1.0);
    fragColor = vec4(lit, color.a) * (coverage * qt_Opacity);
}
