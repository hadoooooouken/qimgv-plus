varying highp vec2 texCoord;
uniform sampler2D srcTex;
uniform highp vec2 srcTexelSize;
uniform highp vec2 dstSize;
uniform highp vec2 ratio;

// Each call site (FilterPixmapItem::buildPreciseDownsample) only ever issues
// a pass with ratio in [0.5, 1.0] per axis, so the source-space footprint of
// one destination texel is at most 2.0 source texels wide. A 4-tap run per
// axis covers that footprint at any phase/alignment with margin to spare.
const int kBoxReduceTapRadius = 4;

highp float axisOverlap(highp float srcIndex, highp float center, highp float halfFootprint) {
    highp float lo = max(srcIndex - 0.5, center - halfFootprint);
    highp float hi = min(srcIndex + 0.5, center + halfFootprint);
    return max(hi - lo, 0.0);
}

void main() {
    // Exact-area box filter: for this destination texel, find the source-
    // space interval it covers (its "footprint") and weight each candidate
    // source texel by how much of its own interval overlaps that footprint.
    // This is the same operation glGenerateMipmap performs for a plain 2x
    // reduction; here it is generalized to any ratio in (0,1] so a chain of
    // these passes composes losslessly down to an arbitrary, non-power-of-
    // two target size instead of snapping to the nearest mip level.
    highp vec2 dstIndex = texCoord * dstSize - vec2(0.5);
    highp vec2 center = (dstIndex + vec2(0.5)) / ratio - vec2(0.5);
    highp vec2 halfFootprint = vec2(0.5) / ratio;
    highp vec2 k0 = floor(center - halfFootprint);

    highp vec4 accum = vec4(0.0);
    highp float wsum = 0.0;
    for (int iy = 0; iy < kBoxReduceTapRadius; iy++) {
        highp float ky = k0.y + float(iy);
        highp float wy = axisOverlap(ky, center.y, halfFootprint.y);
        if (wy <= 0.0) continue;
        for (int ix = 0; ix < kBoxReduceTapRadius; ix++) {
            highp float kx = k0.x + float(ix);
            highp float wx = axisOverlap(kx, center.x, halfFootprint.x);
            highp float w = wx * wy;
            if (w <= 0.0) continue;
            highp vec2 uv = (vec2(kx, ky) + vec2(0.5)) * srcTexelSize;
            accum += texture2D(srcTex, uv) * w;
            wsum += w;
        }
    }
    gl_FragColor = (wsum > 0.0) ? (accum / wsum) : texture2D(srcTex, texCoord);
}
