#pragma once

#include <QColorSpace>
#include <QImage>
#include <QString>

// How an HDR image is encoded, as decided from its colour space, pixel format
// and the HDR_* text keys set by the image plugins (see 3rdparty/kimageformats
// jxl.cpp). Shared by the CPU tone mapper (HdrToneMapper) and the GPU
// renderer (qimgv.render), so that both treat the same images as HDR and
// decode them the same way.

// Transfer function of the stored samples.
enum class HdrTransfer {
  // SMPTE ST 2084 perceptual quantizer, absolute up to 10000 nits.
  PQ,
  // ARIB STD-B67 hybrid log-gamma, scaled to a 1000 nit display.
  HLG,
  // Linear light with 1.0 = scRGB reference white (80 nits).
  Linear,
};

// Primaries of the stored samples.
enum class HdrPrimaries {
  Bt2020,
  DisplayP3,
  Srgb,
};

struct HdrSourceEncoding {
  HdrTransfer transfer = HdrTransfer::PQ;
  HdrPrimaries primaries = HdrPrimaries::Bt2020;

  friend bool operator==(const HdrSourceEncoding &,
                         const HdrSourceEncoding &) = default;
};

// True for the pixel formats that store raw linear-light samples in floating
// point, i.e. the formats the JXR/EXR/HDR/PFM plugins decode into.
[[nodiscard]] inline bool isLinearFloatHdrFormat(QImage::Format format) {
  switch (format) {
  case QImage::Format_RGBX16FPx4:
  case QImage::Format_RGBA16FPx4:
  case QImage::Format_RGBA16FPx4_Premultiplied:
  case QImage::Format_RGBX32FPx4:
  case QImage::Format_RGBA32FPx4:
  case QImage::Format_RGBA32FPx4_Premultiplied:
    return true;
  default:
    return false;
  }
}

// True when image is an HDR image: tagged by an HDR_* text key, stored in a
// linear float format, or in a PQ / HLG / HDR-described colour space.
[[nodiscard]] inline bool isHdrImage(const QImage &image) {
  if (image.isNull())
    return false;

  if (image.text(QStringLiteral("HDR_IsHDR")) == QStringLiteral("true") ||
      !image.text(QStringLiteral("HDR_Profile")).isEmpty()) {
    return true;
  }
  if (isLinearFloatHdrFormat(image.format()))
    return true;

  const QColorSpace cs = image.colorSpace();
  if (cs.isValid()) {
    const QColorSpace::TransferFunction tf = cs.transferFunction();
    if (tf == QColorSpace::TransferFunction::St2084 ||
        tf == QColorSpace::TransferFunction::Hlg) {
      return true;
    }
    const QString desc = cs.description();
    if (desc.contains(QStringLiteral("HDR"), Qt::CaseInsensitive) ||
        desc.contains(QStringLiteral("PQ"), Qt::CaseInsensitive) ||
        desc.contains(QStringLiteral("HLG"), Qt::CaseInsensitive) ||
        desc.contains(QStringLiteral("2100"), Qt::CaseInsensitive)) {
      return true;
    }
  }
  return false;
}

// Encoding of an HDR image (isHdrImage()). PQ with BT.2020 primaries unless
// the HDR_Transfer / HDR_Primaries text keys or the colour space say
// otherwise; untagged linear float formats are linear light.
[[nodiscard]] inline HdrSourceEncoding
detectHdrSourceEncoding(const QImage &image) {
  HdrSourceEncoding encoding;
  const QString transferText = image.text(QStringLiteral("HDR_Transfer"));
  const QColorSpace cs = image.colorSpace();

  if (transferText.compare(QStringLiteral("HLG"), Qt::CaseInsensitive) == 0 ||
      (cs.isValid() &&
       cs.transferFunction() == QColorSpace::TransferFunction::Hlg)) {
    encoding.transfer = HdrTransfer::HLG;
  } else if (transferText.compare(QStringLiteral("Linear"),
                                  Qt::CaseInsensitive) == 0 ||
             (cs.isValid() &&
              cs.transferFunction() == QColorSpace::TransferFunction::Linear) ||
             (!cs.isValid() && isLinearFloatHdrFormat(image.format()))) {
    encoding.transfer = HdrTransfer::Linear;
  }

  const QString primariesText = image.text(QStringLiteral("HDR_Primaries"));
  if (primariesText.contains(QStringLiteral("P3"), Qt::CaseInsensitive) ||
      (cs.isValid() && cs.primaries() == QColorSpace::Primaries::DciP3D65)) {
    encoding.primaries = HdrPrimaries::DisplayP3;
  } else if (primariesText.contains(QStringLiteral("709"),
                                    Qt::CaseInsensitive) ||
             primariesText.contains(QStringLiteral("sRGB"),
                                    Qt::CaseInsensitive) ||
             (cs.isValid() &&
              cs.primaries() == QColorSpace::Primaries::SRgb)) {
    encoding.primaries = HdrPrimaries::Srgb;
  }
  return encoding;
}
