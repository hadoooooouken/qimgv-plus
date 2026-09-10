#pragma once

#include <QImage>
#include <QOpenGLTexture>
#include <memory>

// Bridges a QImage into a QOpenGLTexture, choosing between Qt's standard
// 8-bit upload path (used for every ordinary SDR image, via the unmodified
// QOpenGLTexture(QImage, ...) convenience API, which internally converts to
// Format_RGBA8888 / RGBA8_UNorm regardless of the input format) and an
// explicit RGBA16F upload path for FP16 QImages
// (Format_RGBA16FPx4 / Format_RGBX16FPx4 / Format_RGBA16FPx4_Premultiplied),
// which that convenience API would otherwise silently collapse to 8 bits.
//
// This exists as a small, standalone helper - rather than inline branching
// in FilterPixmapItem - specifically so this GL-format-selection logic
// isn't duplicated a second time in PanoramaGraphicsItem, which
// independently creates its own QOpenGLTexture from a QImage today.
// PanoramaGraphicsItem is not changed by this helper's introduction; it
// remains on its existing unconditional 8-bit path unless/until a separate
// change adopts it here.
namespace GlImageTexture {

// True for the FP16 QImage formats this module gives an explicit RGBA16F
// upload path. Every other format (including ordinary 8-bit formats, and
// higher-bit-depth integer formats such as Format_RGBA64) is left on the
// unmodified 8-bit convenience path - see the namespace-level comment above
// for why only this format family is special-cased.
bool isFp16Format(QImage::Format format);

// Returns false whenever `existing` cannot be safely reused for `image` via
// update() and must instead be recreated from scratch via create(). This
// covers not only a change in image dimensions (as FilterPixmapItem already
// checked before this helper existed) but also a change in GPU precision
// tier - e.g. a same-size SDR image following a same-size FP16 HDR image in
// a browsing session, where the underlying GPU storage format itself must
// change, not just its contents.
bool isCompatible(const QOpenGLTexture *existing, const QImage &image, bool needMips);

// Creates a new texture from `image`, choosing the RGBA8_UNorm convenience
// path or the explicit RGBA16F path per isFp16Format(image.format()).
// `image` must not be null. When `generateMips` is true, a full mip chain
// is generated for either path (explicitly, via
// QOpenGLTexture::generateMipMaps(), for the RGBA16F path, since the manual
// allocateStorage()/setData() calls it uses do not generate mips as a side
// effect the way the convenience API's MipMapGeneration argument does).
std::unique_ptr<QOpenGLTexture> create(const QImage &image, bool generateMips);

// Updates an existing texture in place with new image data, without
// reallocating GPU storage. The caller must have already confirmed
// isCompatible(existing, image, generateMips) before calling this;
// behavior is undefined otherwise.
void update(QOpenGLTexture *existing, const QImage &image, bool generateMips);

} // namespace GlImageTexture