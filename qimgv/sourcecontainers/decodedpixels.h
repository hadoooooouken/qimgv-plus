#pragma once

#include <QImage>
#include <QMutex>
#include <QSize>

#include <functional>
#include <memory>

#include "utils/displaypipeline.h"

// Decoded pixels of a static image and the SDR copy the CPU works with
// (editing, saving, copying, scaling, upscaling). An HDR image is converted
// to SDR by the conversion it is given: at once for DisplayPipeline::Cpu, and
// on the first sdr() call for DisplayPipeline::Gpu, whose viewer displays the
// decoded HDR pixels itself (decoded()). An SDR image is its own SDR copy.
//
// Thread safe: images are shared between the GUI thread and the loader,
// scaler and upscaler pools.
class DecodedPixels {
public:
    // HDR -> SDR; returns a null image when the conversion fails.
    using SdrConversion = std::function<QImage(const QImage &hdr)>;

    DecodedPixels() = default;
    DecodedPixels(const DecodedPixels &) = delete;
    DecodedPixels &operator=(const DecodedPixels &) = delete;

    // Takes decoded (null clears). hdr: decoded needs toSdr.
    void assign(std::shared_ptr<const QImage> decoded, bool hdr,
                DisplayPipeline pipeline, SdrConversion toSdr);

    // The SDR pixels; converts an HDR image on first use. nullptr without
    // pixels or when the conversion failed (reported once).
    [[nodiscard]] std::shared_ptr<const QImage> sdr();
    // The pixels as decoded when the HDR source is kept (Gpu), otherwise the
    // SDR pixels. Never converts.
    [[nodiscard]] std::shared_ptr<const QImage> decoded() const;
    // True while the HDR source is kept.
    [[nodiscard]] bool hasHdrSource() const;
    [[nodiscard]] QSize size() const;

    // Replaces the pixels with SDR ones (committed edits); a kept HDR source
    // is dropped.
    void replace(std::shared_ptr<const QImage> sdrPixels);

private:
    // Requires mMutex.
    void convertLocked();

    mutable QMutex mMutex;
    std::shared_ptr<const QImage> mHdr;
    std::shared_ptr<const QImage> mSdr;
    SdrConversion mToSdr;
    bool mConversionFailed = false;
};
