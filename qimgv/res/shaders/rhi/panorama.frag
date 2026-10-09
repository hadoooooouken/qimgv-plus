#version 440

// Equirectangular panorama seen from its centre: port of the widget viewer's
// res/shaders/panorama.frag (PanoramaGraphicsItem), same ray construction,
// rotation order and texture mapping.
//
// Differences from the widget shader:
// - Images larger than the texture size limit are drawn per tile: every tile
//   covers the whole colour buffer and keeps the rays that hit its core.
// - The texture is sampled with explicit gradients whose longitude part is
//   corrected for the wrap at the panorama's back seam, so trilinear
//   sampling uses the mip level of the footprint instead of the smallest one
//   along that seam.
// - Texels are premultiplied; the colour matrix applies to straight colour
//   like in image.frag, and the result is blended over the background.

layout(location = 0) in vec2 screenPos;

layout(location = 0) out vec4 fragColor;

// Mirrored by PanoramaUniforms in gui/quick/render/rhipassuniforms.h;
// identical in panorama.vert.
layout(std140, binding = 0) uniform PanoramaParams {
    mat4 mvp;
    // Rows of the colour adjustment matrix (xyz) applied to straight colour.
    vec4 colorRow0;
    vec4 colorRow1;
    vec4 colorRow2;
    // Tile core in normalized image coordinates: left, top, right, bottom
    // (half-open; edges of the image lie outside [0, 1]).
    vec4 core;
    // Tile texture in normalized image coordinates: x, y, width, height.
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
    // The texture spans the whole image width and its sampler repeats: no
    // horizontal core test.
    int wrapsHorizontally;
};

layout(binding = 1) uniform sampler2D imageTexture;

const float kPi = 3.14159265358979323846;
// Below this alpha the pixel is not un-premultiplied for the colour matrix.
const float kMinUnpremultiplyAlpha = 0.0001;
// Longitude differences of neighbouring pixels larger than half the image
// width are the wrap at the seam, not a footprint.
const float kHalfTurn = 0.5;

void main()
{
    // Screen position in [-1, 1], y down (the widget's texCoord * 2 - 1).
    vec2 sc = screenPos * 2.0 - 1.0;

    // Ray direction in camera space.
    vec3 ray = normalize(vec3(sc.x * aspect * tanHalfFov,
                              -sc.y * tanHalfFov,
                              1.0));

    // Pitch around the X axis.
    vec3 r = ray;
    ray.y = r.y * cosPitch - r.z * sinPitch;
    ray.z = r.y * sinPitch + r.z * cosPitch;

    // Yaw around the Y axis.
    r = ray;
    ray.x = r.x * cosYaw + r.z * sinYaw;
    ray.z = -r.x * sinYaw + r.z * cosYaw;

    // Spherical (equirectangular) coordinates in [0, 1].
    float lon = atan(ray.x, ray.z);
    float lat = asin(clamp(ray.y, -1.0, 1.0));
    vec2 uv = vec2(0.5 + lon / (2.0 * kPi), 0.5 - lat / kPi);

    // Footprint, computed before any fragment is discarded.
    vec2 dx = dFdx(uv);
    vec2 dy = dFdy(uv);
    if (abs(dx.x) > kHalfTurn)
        dx.x -= sign(dx.x);
    if (abs(dy.x) > kHalfTurn)
        dy.x -= sign(dy.x);

    bool outsideCore = uv.y < core.y || uv.y >= core.w;
    if (wrapsHorizontally == 0)
        outsideCore = outsideCore || uv.x < core.x || uv.x >= core.z;
    if (outsideCore)
        discard;

    vec2 tc = (uv - texRect.xy) / texRect.zw;
    vec4 color = textureGrad(imageTexture, tc, dx / texRect.zw,
                             dy / texRect.zw);
    if (colorEnabled != 0) {
        float a = color.a;
        vec3 straight = (a > kMinUnpremultiplyAlpha) ? color.rgb / a
                                                     : color.rgb;
        straight = clamp(vec3(dot(colorRow0.xyz, straight),
                              dot(colorRow1.xyz, straight),
                              dot(colorRow2.xyz, straight)) + vec3(colorOffset),
                         0.0, 1.0);
        color.rgb = straight * a;
    }
    fragColor = color;
}
