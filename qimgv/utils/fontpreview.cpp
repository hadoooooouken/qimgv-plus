#include "fontpreview.h"

#include <QColorSpace>
#include <QGlyphRun>
#include <QPainter>
#include <QRawFont>
#include <QStringList>
#include <algorithm>
#include <cmath>

namespace {

constexpr int kPreviewWidth = 512;
constexpr int kPreviewHeight = 288;
constexpr qreal kProbePixelSize = 100.0;
constexpr qreal kMinPixelSize = 24.0;
constexpr qreal kMaxPixelSize = 220.0;
constexpr qreal kHorizontalMargin = 40.0;
constexpr qreal kVerticalMargin = 32.0;

bool supportsText(const QRawFont &font, const QString &text) {
  for (const QChar ch : text) {
    if (!font.supportsCharacter(ch))
      return false;
  }
  return true;
}

QString choosePreviewText(const QRawFont &font) {
  static const QStringList candidates = {
      QStringLiteral("\u0410\u0431\u0432"), // Абв
      QStringLiteral("Abc"),
      QStringLiteral("123")
  };

  for (const QString &candidate : candidates) {
    if (supportsText(font, candidate))
      return candidate;
  }

  // Symbol fonts may not contain either Latin or Cyrillic. In that case,
  // pick up to three printable ASCII glyphs that really exist in the file.
  QString fallback;
  for (uint codePoint = 33; codePoint <= 126 && fallback.size() < 3;
       ++codePoint) {
    if (font.supportsCharacter(codePoint))
      fallback.append(QChar(static_cast<ushort>(codePoint)));
  }
  return fallback;
}

QGlyphRun makeGlyphRun(const QRawFont &font, const QString &text) {
  const QList<quint32> glyphs = font.glyphIndexesForString(text);
  const QList<QPointF> advances =
      font.advancesForGlyphIndexes(glyphs, QRawFont::KernedAdvances);

  QList<QPointF> positions;
  positions.reserve(glyphs.size());

  QPointF penPosition;
  for (qsizetype i = 0; i < glyphs.size(); ++i) {
    positions.append(penPosition);
    if (i < advances.size())
      penPosition += advances.at(i);
  }

  QGlyphRun run;
  run.setRawFont(font);
  run.setGlyphIndexes(glyphs);
  run.setPositions(positions);
  return run;
}

} // namespace

QImage FontPreview::render(const QString &path) {
  QRawFont font(path, kProbePixelSize);
  if (!font.isValid())
    return {};

  const QString text = choosePreviewText(font);
  if (text.isEmpty())
    return {};

  QGlyphRun run = makeGlyphRun(font, text);
  QRectF bounds = run.boundingRect();
  if (!bounds.isValid() || bounds.isEmpty())
    return {};

  const qreal availableWidth =
      static_cast<qreal>(kPreviewWidth) - 2.0 * kHorizontalMargin;
  const qreal availableHeight =
      static_cast<qreal>(kPreviewHeight) - 2.0 * kVerticalMargin;
  const qreal scale =
      std::min(availableWidth / bounds.width(),
               availableHeight / bounds.height());
  if (!std::isfinite(scale) || scale <= 0.0)
    return {};

  font.setPixelSize(std::clamp(kProbePixelSize * scale,
                               kMinPixelSize, kMaxPixelSize));
  run = makeGlyphRun(font, text);
  bounds = run.boundingRect();
  if (!bounds.isValid() || bounds.isEmpty())
    return {};

  QImage preview(kPreviewWidth, kPreviewHeight, QImage::Format_RGB32);
  preview.fill(Qt::white);
  preview.setColorSpace(QColorSpace(QColorSpace::SRgb));

  QPainter painter(&preview);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setRenderHint(QPainter::TextAntialiasing, true);
  painter.setPen(Qt::black);

  const QPointF origin(
      (static_cast<qreal>(kPreviewWidth) - bounds.width()) / 2.0 -
          bounds.left(),
      (static_cast<qreal>(kPreviewHeight) - bounds.height()) / 2.0 -
          bounds.top());
  painter.drawGlyphRun(origin, run);

  return preview;
}
