#include "thumbnailwidget.h"
#include "settings.h"
#include <QPainterPath>

ThumbnailWidget::ThumbnailWidget(QGraphicsItem *parent) :
    QGraphicsWidget(parent),
    isLoaded(false),
    thumbnail(nullptr),
    highlighted(false),
    hovered(false),
    dropHovered(false),
    mThumbnailSize(100),
    padding(5),
    marginX(2),
    marginY(2),
    labelSpacing(9),
    textHeight(15),
    thumbStyle(THUMB_SIMPLE)
{
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    dpr = qApp->devicePixelRatio();
    if(trunc(dpr) == dpr) // don't enable for fractional scaling
        setCacheMode(QGraphicsItem::DeviceCoordinateCache);
    setAcceptHoverEvents(true);
    font.setBold(false);
    QFontMetrics fm(font);
    textHeight = fm.height();
}

void ThumbnailWidget::updateDpr(qreal newDpr) {
    if(dpr != newDpr) {
        dpr = newDpr;
        if(trunc(dpr) == dpr) // don't enable for fractional scaling
            setCacheMode(QGraphicsItem::DeviceCoordinateCache);
        updateThumbnailDrawPosition();
        updateBackgroundRect();
    }
}

void ThumbnailWidget::setThumbnailSize(int size) {
    if(mThumbnailSize != size && size > 0) {
        isLoaded = false;
        mThumbnailSize = size;
        updateBoundingRect();
        updateGeometry();
        updateThumbnailDrawPosition();
        updateBackgroundRect();
        setupTextLayout();
        update();
    }
}

void ThumbnailWidget::setPadding(int _padding) {
    padding = _padding;
    updateBoundingRect();
}

void ThumbnailWidget::setMargins(int _marginX, int _marginY) {
    marginX = _marginX;
    marginY = _marginY;
    updateBoundingRect();
}

int ThumbnailWidget::thumbnailSize() {
    return mThumbnailSize;
}

void ThumbnailWidget::reset() {
    if(thumbnail)
        thumbnail.reset();
    highlighted = false;
    hovered = false;
    isLoaded = false;
    update();
}

void ThumbnailWidget::setThumbStyle(ThumbnailStyle _style) {
    if(thumbStyle != _style) {
        thumbStyle = _style;
        updateBoundingRect();
        updateThumbnailDrawPosition();
        setupTextLayout();
        updateBackgroundRect();
        updateGeometry();
        update();
    }
}

void ThumbnailWidget::updateGeometry() {
    QGraphicsWidget::updateGeometry();
}

void ThumbnailWidget::setGeometry(const QRectF &rect) {
    QGraphicsWidget::setGeometry(QRectF(rect.topLeft(), boundingRect().size()));
}

QRectF ThumbnailWidget::geometry() const {
    return QRectF(QGraphicsWidget::geometry().topLeft(), boundingRect().size());
}

QSizeF ThumbnailWidget::effectiveSizeHint(Qt::SizeHint which, const QSizeF &constraint) const {
    return sizeHint(which, constraint);
}

void ThumbnailWidget::setThumbnail(std::shared_ptr<Thumbnail> _thumbnail) {
    if(_thumbnail) {
        thumbnail = _thumbnail;
        isLoaded = true;
        updateThumbnailDrawPosition();
        setupTextLayout();
        updateBackgroundRect();
        update();
    }
}

void ThumbnailWidget::unsetThumbnail() {
    if(thumbnail)
        thumbnail.reset();
    isLoaded = false;
}

void ThumbnailWidget::setupTextLayout() {
    if(thumbStyle != THUMB_SIMPLE) {
        qreal h = mThumbnailSize * 0.75;
        nameRect = QRect(padding + marginX,
                          padding + marginY + h + labelSpacing,
                          mThumbnailSize, textHeight);
        infoRect = nameRect.adjusted(0, textHeight + 2, 0, textHeight + 2);
    }
}

void ThumbnailWidget::updateBackgroundRect() {
    bgRect = boundingRect().adjusted(marginX, marginY, -marginX, -marginY);
}

void ThumbnailWidget::setHighlighted(bool mode) {
    if(highlighted != mode) {
        highlighted = mode;
        update();
    }
}

bool ThumbnailWidget::isHighlighted() {
    return highlighted;
}

void ThumbnailWidget::setDropHovered(bool mode) {
    if(dropHovered != mode) {
        dropHovered = mode;
        update();
    }
}

bool ThumbnailWidget::isDropHovered() {
    return dropHovered;
}

QRectF ThumbnailWidget::boundingRect() const {
    return mBoundingRect;
}

void ThumbnailWidget::updateBoundingRect() {
    qreal h = mThumbnailSize * 0.75;
    mBoundingRect = QRectF(0, 0,
                           mThumbnailSize + (padding + marginX) * 2,
                           h + (padding + marginY) * 2);

    if(thumbStyle != THUMB_SIMPLE)
        mBoundingRect.adjust(0, 0, 0, labelSpacing + textHeight * 2);
}

qreal ThumbnailWidget::width() {
    return boundingRect().width();
}

qreal ThumbnailWidget::height() {
    return boundingRect().height();
}

void ThumbnailWidget::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    Q_UNUSED(widget)
    Q_UNUSED(option)
    painter->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);

    // update dpr since on wayland we only get correct values on the first paint event
    // actual thumbnail still has incorrect dpr but it is still drawn "correctly" so whatever
    updateDpr(painter->paintEngine()->paintDevice()->devicePixelRatioF());

    if(isHovered() && !isHighlighted())
        drawHoverBg(painter);
    if(isHighlighted())
        drawHighlight(painter);

    if(!thumbnail) { // not loaded
        QPixmap loadingIcon(shrRes->getPixmap(ShrIcon::SHR_ICON_LOADING, dpr));
        if(isHighlighted())
            ImageLib::recolor(loadingIcon, settings->colorScheme().accent);
        else
            ImageLib::recolor(loadingIcon, settings->colorScheme().thumbpanel_hc2);
        drawIcon(painter, &loadingIcon);
    } else {
        if(!thumbnail->pixmap() || thumbnail->pixmap().get()->width() == 0) { // invalid thumb
            QPixmap errorIcon(shrRes->getPixmap(ShrIcon::SHR_ICON_ERROR, dpr));
            if(isHighlighted())
                ImageLib::recolor(errorIcon, settings->colorScheme().accent);
            else
                ImageLib::recolor(errorIcon, settings->colorScheme().thumbpanel_hc2);
            drawIcon(painter, &errorIcon);
        } else {
            drawThumbnail(painter, thumbnail->pixmap().get());
            if(isHovered())
                drawHoverHighlight(painter);
        }
        if(thumbStyle != THUMB_SIMPLE)
            drawLabel(painter);
    }
    if(isDropHovered())
        drawDropHover(painter);
}

void ThumbnailWidget::drawHighlight(QPainter *painter) {
    if(isHighlighted()) {
        auto hints = painter->renderHints();
        auto op = painter->opacity();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setOpacity(0.40f * op);
        painter->setPen(Qt::NoPen);
        painter->setBrush(settings->colorScheme().accent);
        painter->drawRoundedRect(bgRect, 8.0, 8.0);
        painter->setBrush(Qt::NoBrush);
        painter->setOpacity(0.70f * op);
        QPen pen(settings->colorScheme().accent, 2);
        painter->setPen(pen);
        painter->drawRoundedRect(bgRect.adjusted(1,1,-1,-1), 7.0, 7.0); // 2px pen
        painter->setOpacity(op);
        painter->setRenderHints(hints);
    }
}

void ThumbnailWidget::drawHoverBg(QPainter *painter) {
    auto op = painter->opacity();
    auto hints = painter->renderHints();
    painter->setRenderHint(QPainter::Antialiasing);
    QColor hoverBg = mUseThumbPanelColors ? settings->colorScheme().thumbpanel_hc : settings->colorScheme().folderview_hc;
    painter->setPen(Qt::NoPen);
    painter->setBrush(hoverBg);
    painter->drawRoundedRect(bgRect, 8.0, 8.0);
    painter->setBrush(Qt::NoBrush);
    painter->setOpacity(op);
    painter->setRenderHints(hints);
}

void ThumbnailWidget::drawHoverHighlight(QPainter *painter) {
    auto op = painter->opacity();
    auto mode = painter->compositionMode();
    painter->setCompositionMode(QPainter::CompositionMode_Plus);
    painter->setOpacity(0.2f);
    painter->save();
    QPainterPath path;
    path.addRoundedRect(drawRectCentered, 4.0, 4.0);
    painter->setClipPath(path, Qt::IntersectClip);
    painter->drawPixmap(drawRectCentered, *thumbnail->pixmap());
    painter->restore();
    painter->setOpacity(op);
    painter->setCompositionMode(mode);
}

void ThumbnailWidget::drawLabel(QPainter *painter) {
    if(thumbnail) {
        // Determine current background color under the text
        QColor currentBg = mUseThumbPanelColors ? settings->colorScheme().thumbpanel : settings->colorScheme().folderview;
        if(isHighlighted()) {
            currentBg = settings->colorScheme().accent;
        } else if(isHovered()) {
            currentBg = mUseThumbPanelColors ? settings->colorScheme().thumbpanel_hc : settings->colorScheme().folderview_hc;
        }

        // Contrast text against current background
        QColor textColor = (qGray(currentBg.rgb()) > 128) ? QColor(0, 0, 0) : QColor(255, 255, 255);

        drawSingleLineText(painter, nameRect, thumbnail->name(), textColor);
        auto op = painter->opacity();
        painter->setOpacity(op * 0.62f);
        drawSingleLineText(painter, infoRect, thumbnail->info(), textColor);
        painter->setOpacity(op);
    }
}

void ThumbnailWidget::drawSingleLineText(QPainter *painter, QRect rect, QString text, const QColor &color) {
    QFontMetrics fm(font);
    bool fits = !(fm.horizontalAdvance(text) > rect.width());
    // filename
    int flags;
    painter->setFont(font);
    if(fits) {
        flags = Qt::TextSingleLine | Qt::AlignVCenter | Qt::AlignHCenter;
        painter->setPen(color);
        painter->drawText(rect, flags, text);
    } else {
        // fancy variant with text fade effect - uses temporary surface to paint; slow
        QPixmap textLayer(rect.width() * dpr, rect.height() * dpr);
        textLayer.fill(Qt::transparent);
        textLayer.setDevicePixelRatio(dpr);
        QPainter textPainter(&textLayer);
        textPainter.setFont(font);
        // paint text onto tmp layer
        flags = Qt::TextSingleLine | Qt::AlignVCenter;
        textPainter.setPen(color);
        QRect textRect = QRect(0, 0, rect.width(), rect.height());
        textPainter.drawText(textRect, flags, text);
        QRectF fadeRect = textRect.adjusted(textRect.width() - 6,0,0,0);
        // fade effect
        QLinearGradient gradient(fadeRect.topLeft(), fadeRect.topRight());
        gradient.setColorAt(0, Qt::transparent);
        gradient.setColorAt(1, Qt::red); // any color, this is just a transparency mask
        textPainter.setCompositionMode(QPainter::CompositionMode_DestinationOut);
        textPainter.fillRect(fadeRect, gradient);
        // write text layer into graphicsitem
        painter->setCompositionMode(QPainter::CompositionMode_SourceOver);
        painter->drawPixmap(rect.topLeft(), textLayer);
    }
}

void ThumbnailWidget::drawDropHover(QPainter *painter) {
    // save
    auto hints = painter->renderHints();
    auto op = painter->opacity();

    painter->setRenderHint(QPainter::Antialiasing);
    QColor clr(190,60,25);
    painter->setOpacity(0.1f * op);
    painter->setPen(Qt::NoPen);
    painter->setBrush(clr);
    painter->drawRoundedRect(bgRect, 8.0, 8.0);
    painter->setBrush(Qt::NoBrush);
    painter->setOpacity(op);
    QPen pen(clr, 2);
    painter->setPen(pen);
    painter->drawRoundedRect(bgRect.adjusted(1,1,-1,-1), 7.0, 7.0);
    painter->setRenderHints(hints);
}

void ThumbnailWidget::drawThumbnail(QPainter* painter, const QPixmap *pixmap) {
    painter->save();
    QPainterPath path;
    path.addRoundedRect(drawRectCentered, 4.0, 4.0);
    painter->setClipPath(path, Qt::IntersectClip);
    painter->drawPixmap(drawRectCentered, *pixmap);
    painter->restore();
}

void ThumbnailWidget::drawIcon(QPainter* painter, const QPixmap *pixmap) {
    QPointF drawPosCentered(width()  / 2 - pixmap->width()  / (2 * pixmap->devicePixelRatioF()),
                            height() / 2 - pixmap->height() / (2 * pixmap->devicePixelRatioF()));
    painter->drawPixmap(drawPosCentered, *pixmap, QRectF(QPoint(0,0), pixmap->size()));
}

QSizeF ThumbnailWidget::sizeHint(Qt::SizeHint which, const QSizeF &constraint) const {
    Q_UNUSED(which);
    Q_UNUSED(constraint);

    return boundingRect().size();
}

void ThumbnailWidget::hoverEnterEvent(QGraphicsSceneHoverEvent *event) {
    event->ignore();
    setHovered(true);
}

void ThumbnailWidget::hoverLeaveEvent(QGraphicsSceneHoverEvent *event) {
    event->ignore();
    setHovered(false);
}

void ThumbnailWidget::setHovered(bool mode) {
    if(hovered != mode) {
        hovered = mode;
        update();
    }
}

bool ThumbnailWidget::isHovered() {
    return hovered;
}

void ThumbnailWidget::updateThumbnailDrawPosition() {
    if(thumbnail && thumbnail->pixmap()) {
        QPoint topLeft;
        qreal h = mThumbnailSize * 0.75;
        // Always scale to fit the current cell dimensions
        QSize pixmapSize = thumbnail->pixmap()->size();
        if(pixmapSize.width() > mThumbnailSize || pixmapSize.height() > h)
            pixmapSize = pixmapSize.scaled(mThumbnailSize, h, Qt::KeepAspectRatio);

        topLeft.setX((width()  - pixmapSize.width())  / 2.0);
        if(thumbStyle == THUMB_SIMPLE)
            topLeft.setY((height() - pixmapSize.height()) / 2.0);
        else if(thumbStyle == THUMB_NORMAL_CENTERED)
            topLeft.setY((height() - pixmapSize.height()) / 2.0 - textHeight);
        else // THUMB_NORMAL - center thumbnail vertically in the image area
            topLeft.setY(padding + marginY + (h - pixmapSize.height()) / 2.0);
        drawRectCentered = QRect(topLeft, pixmapSize);
    }
}
