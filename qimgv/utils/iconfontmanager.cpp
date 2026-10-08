#include "iconfontmanager.h"

#include <QDebug>
#include <QFontDatabase>
#include <QPainter>
#include <QPixmapCache>

namespace {

// Fraction of the target box height used as the QFont pixel size. Fluent
// glyphs are drawn with internal padding, similar to most icon fonts; a
// factor of 1.0 undersizes them relative to the source PNGs, which were
// exported edge-to-edge.
constexpr qreal kGlyphFontSizeFactor = 1.0;

} // namespace

QString IconFontManager::fontFamily;
bool IconFontManager::initialized = false;

bool IconFontManager::init() {
    if (initialized)
        return !fontFamily.isEmpty();

    initialized = true;
    int fontId = QFontDatabase::addApplicationFont(":/res/fonts/FluentSystemIcons-Custom.ttf");
    if (fontId == -1) {
        qWarning() << "IconFontManager: failed to load FluentSystemIcons-Custom.ttf from resources";
        return false;
    }
    QStringList families = QFontDatabase::applicationFontFamilies(fontId);
    if (families.isEmpty()) {
        qWarning() << "IconFontManager: font loaded but reports no family name";
        return false;
    }
    fontFamily = families.first();
    return true;
}

const QString &IconFontManager::family() {
    return fontFamily;
}

QPixmap IconFontManager::pixmap(FluentIcon icon, int sizePx, QColor color, qreal dpr) {
    if (fontFamily.isEmpty()) {
        qWarning() << "IconFontManager::pixmap() called before a successful init()";
        return QPixmap();
    }
    const std::optional<char32_t> codepoint = FluentIcons::codepoint(icon);
    if (!codepoint) {
        qWarning() << "IconFontManager: no codepoint registered for this FluentIcon value";
        return QPixmap();
    }

    const QString cacheKey = QStringLiteral("iconfont:%1:%2:%3:%4")
        .arg(static_cast<int>(icon))
        .arg(sizePx)
        .arg(color.rgba(), 0, 16)
        .arg(dpr, 0, 'f', 2);

    QPixmap cached;
    if (QPixmapCache::find(cacheKey, &cached))
        return cached;

    const int physicalSize = qRound(sizePx * dpr);
    QPixmap result(physicalSize, physicalSize);
    result.fill(Qt::transparent);
    result.setDevicePixelRatio(dpr);

    const QString glyph = QString::fromUcs4(&*codepoint, 1);

    QFont font(fontFamily);
    font.setPixelSize(qRound(sizePx * kGlyphFontSizeFactor));

    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setFont(font);
    painter.setPen(color);

    // Qt::AlignCenter (previously used with drawText(QRectF, flags, text))
    // centers the glyph's font *advance box*, not its visible ink. Fluent
    // glyphs can have asymmetric left/right side bearings within that
    // advance box (chevrons in particular), so advance-box centering left
    // the visible arrow shifted off to one side inside the button. Centering
    // the glyph's tight ink bounding rect instead fixes this regardless of
    // bearing asymmetry.
    QFontMetricsF metrics(font);
    const QRectF inkRect = metrics.tightBoundingRect(glyph);
    const qreal drawX = (sizePx - inkRect.width()) / 2.0 - inkRect.left();
    const qreal drawY = (sizePx - inkRect.height()) / 2.0 - inkRect.top();
    painter.drawText(QPointF(drawX, drawY), glyph);
    painter.end();

    QPixmapCache::insert(cacheKey, result);
    return result;
}
