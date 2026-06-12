varying highp vec2 texCoord;
uniform sampler2D tex;
uniform highp float yaw;
uniform highp float pitch;
uniform highp float fov;
uniform highp float aspect;

uniform highp mat3 colorMatrix;
uniform highp float colorOffset;

#define PI 3.14159265358979323846

void main() {
    // 1. Transform texCoord from 0..1 to -1..1
    highp vec2 sc = texCoord * 2.0 - 1.0;

    // 2. Calculate ray direction in camera space
    highp float tanHalfFov = tan(fov * 0.5);
    highp vec3 ray = normalize(vec3(sc.x * aspect * tanHalfFov, 
                                   -sc.y * tanHalfFov, 
                                   1.0));

    // 3. Rotate ray by pitch (X-axis)
    highp float cp = cos(pitch);
    highp float sp = sin(pitch);
    highp vec3 r = ray;
    ray.y = r.y * cp - r.z * sp;
    ray.z = r.y * sp + r.z * cp;

    // 4. Rotate ray by yaw (Y-axis)
    highp float cy = cos(yaw);
    highp float sy = sin(yaw);
    r = ray;
    ray.x = r.x * cy + r.z * sy;
    ray.z = -r.x * sy + r.z * cy;

    // 5. Convert 3D ray to spherical coordinates (equirectangular)
    highp float lon = atan(ray.x, ray.z);
    highp float lat = asin(clamp(ray.y, -1.0, 1.0));

    // 6. Map to [0, 1] texture coordinates
    highp vec2 uv = vec2(0.5 + lon / (2.0 * PI), 
                         0.5 - lat / PI);

    highp vec4 color = texture2D(tex, uv);
    highp vec3 rgb = color.rgb;
    rgb = clamp(colorMatrix * rgb + vec3(colorOffset), 0.0, 1.0);
    
    gl_FragColor = vec4(rgb, color.a);
}
