#pragma once

#include <QImage>
#include <QString>
#include <cstdint>

enum class ToneMapOperator {
    Bt2408 = 0,
    ReinhardJodie = 1,
    AcesFilmic = 2,
    Hable = 3
};

struct HdrToneMapParams {
    bool enabled = true;
    ToneMapOperator op = ToneMapOperator::Bt2408;
    float targetWhiteNits = 203.0f;
};

class HdrToneMapper {
public:
    // Checks whether the given image is an HDR image (via color space, floating-point
    // pixel format, or HDR text attributes).
    static bool isHdr(const QImage &image);

    // Returns a human-readable HDR profile description (e.g. "Rec.2100 PQ (HDR10)", "BT.2100 HLG"),
    // or an empty QString if the image is SDR.
    static QString detectHdrProfile(const QImage &image);

    // Tone-maps an HDR image into an 8-bit SDR image in standard sRGB color space.
    // If the input image is not HDR or tone-mapping fails, returns a fallback copy converted to ARGB32.
    static QImage applyToneMapping(const QImage &srcImage, const HdrToneMapParams &params);
};
