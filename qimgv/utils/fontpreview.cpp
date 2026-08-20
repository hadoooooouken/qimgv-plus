#include "fontpreview.h"

#include <QChar>
#include <QColorSpace>
#include <QFontDatabase>
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
constexpr qsizetype kPreviewCharacterCount = 3;

struct UnicodeRange {
  char32_t first;
  char32_t last;
};

struct PreviewSelection {
  QString text;
  qsizetype characterCount = 0;

  bool isComplete() const {
    return characterCount == kPreviewCharacterCount;
  }
};

constexpr UnicodeRange kPrintableAsciiRange{U'!', U'~'};
constexpr UnicodeRange kBmpPrivateUseRange{0xE000, 0xF8FF};
constexpr UnicodeRange kSupplementaryPrivateUseAreaARange{0xF0000, 0xFFFFD};
constexpr UnicodeRange kSupplementaryPrivateUseAreaBRange{0x100000, 0x10FFFD};
constexpr UnicodeRange kSupplementarySymbolRange{0x1F000, 0x1FAFF};
constexpr UnicodeRange kBmpBeforeSurrogatesRange{0x00A1, 0xD7FF};
constexpr UnicodeRange kBmpAfterPrivateUseRange{0xF900, 0xFFFD};
constexpr UnicodeRange kEarlySupplementaryRange{0x10000, 0x1EFFF};
constexpr UnicodeRange kRemainingSupplementaryRange{0x1FB00, 0xEFFFF};

constexpr UnicodeRange kFallbackUnicodeRanges[] = {
    kPrintableAsciiRange,
    kBmpPrivateUseRange,
    kSupplementaryPrivateUseAreaARange,
    kSupplementaryPrivateUseAreaBRange,
    kSupplementarySymbolRange,
    kBmpBeforeSurrogatesRange,
    kBmpAfterPrivateUseRange,
    kEarlySupplementaryRange,
    kRemainingSupplementaryRange,
};

bool isStandalonePreviewCharacter(char32_t codePoint) {
  switch (QChar::category(codePoint)) {
  case QChar::Mark_NonSpacing:
  case QChar::Mark_SpacingCombining:
  case QChar::Mark_Enclosing:
  case QChar::Separator_Space:
  case QChar::Separator_Line:
  case QChar::Separator_Paragraph:
  case QChar::Other_Control:
  case QChar::Other_Format:
  case QChar::Other_Surrogate:
    return false;
  default:
    return true;
  }
}

void appendIfSupported(const QRawFont &font, char32_t codePoint,
                       PreviewSelection &selection) {
  if (selection.isComplete() ||
      QChar::isNonCharacter(codePoint) ||
      !isStandalonePreviewCharacter(codePoint) ||
      !font.supportsCharacter(static_cast<uint>(codePoint))) {
    return;
  }

  selection.text.append(QString::fromUcs4(&codePoint, 1));
  ++selection.characterCount;
}

PreviewSelection selectFromText(const QRawFont &font, const QString &text) {
  PreviewSelection selection;
  for (const uint codePoint : text.toUcs4()) {
    appendIfSupported(font, static_cast<char32_t>(codePoint), selection);
    if (selection.isComplete())
      break;
  }
  return selection;
}

PreviewSelection selectFromRange(const QRawFont &font,
                                 const UnicodeRange &range) {
  PreviewSelection selection;
  for (char32_t codePoint = range.first;
       codePoint <= range.last && !selection.isComplete(); ++codePoint) {
    appendIfSupported(font, codePoint, selection);
  }
  return selection;
}

void keepBestSelection(const PreviewSelection &candidate,
                       PreviewSelection &best) {
  if (candidate.characterCount > best.characterCount)
    best = candidate;
}

QString choosePreviewText(const QRawFont &font) {
  static const QStringList preferredSamples = {
      QStringLiteral("\u0410\u0431\u0432"), // Cyrillic sample
      QStringLiteral("Abc"),
      QStringLiteral("123")
  };

  for (const QString &sample : preferredSamples) {
    const PreviewSelection selection = selectFromText(font, sample);
    if (selection.isComplete())
      return selection.text;
  }

  PreviewSelection bestSelection;
  const QList<QFontDatabase::WritingSystem> writingSystems =
      font.supportedWritingSystems();
  for (const QFontDatabase::WritingSystem writingSystem : writingSystems) {
    if (writingSystem == QFontDatabase::Any)
      continue;

    const PreviewSelection selection = selectFromText(
        font, QFontDatabase::writingSystemSample(writingSystem));
    if (selection.isComplete())
      return selection.text;
    keepBestSelection(selection, bestSelection);
  }

  // The OS/2 metadata used by supportedWritingSystems() can be absent or
  // inaccurate. Scan likely symbol ranges first, then the remaining Unicode
  // ranges, and retain the best partial result for very small fonts.
  for (const UnicodeRange &range : kFallbackUnicodeRanges) {
    const PreviewSelection selection = selectFromRange(font, range);
    if (selection.isComplete())
      return selection.text;
    keepBestSelection(selection, bestSelection);
  }

  return bestSelection.text;
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
