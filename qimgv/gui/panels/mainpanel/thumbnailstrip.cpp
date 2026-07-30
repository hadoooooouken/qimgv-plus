#include "thumbnailstrip.h"
#include "settings.h"

namespace {
constexpr qreal kPreloadSideCount = 2.0;
constexpr int kPoolSafetyItemCount = 1;
}

ThumbnailStrip::ThumbnailStrip(QWidget *parent) : ThumbnailView(Qt::Horizontal, parent) {
    this->setAttribute(Qt::WA_NoMousePropagation, true);
    this->setFocusPolicy(Qt::NoFocus);
    setupLayout();
    readSettings();
}

void ThumbnailStrip::updateScrollbarIndicator() {
    if(!itemCount() || lastSelected() == -1)
        return;
    qreal itemCenter = (qreal)(lastSelected() + 0.5) / itemCount();
    if(scrollBar->orientation() == Qt::Horizontal)
        indicator = QRect(scrollBar->width() * itemCenter - indicatorSize, 2, indicatorSize, scrollBar->height() - 4);
    else
        indicator = QRect(2, scrollBar->height() * itemCenter - indicatorSize, scrollBar->width() - 4, indicatorSize);
}

//  no layout; manual item positioning
//  graphical issues otherwise
void ThumbnailStrip::setupLayout() {
    this->setAlignment(Qt::AlignLeft | Qt::AlignTop);
}

std::unique_ptr<ThumbnailWidget> ThumbnailStrip::createThumbnailWidget() {
    auto widget = std::make_unique<ThumbnailWidget>();
    widget->setPadding(thumbPadding);
    widget->setMargins(thumbMarginX, thumbMarginY);
    widget->setThumbStyle(mCurrentStyle);
    widget->setThumbnailSize(mThumbnailSize);
    widget->setUseThumbPanelColors(true);
    return widget;
}

void ThumbnailStrip::focusOn(int index) {
    if(!checkRange(index))
        return;
    const QRectF geometry = itemGeometry(index);
    if(settings->panelCenterSelection()) {
        if(settings->enableSmoothScroll()) {
            const QPointF targetCenter = geometry.center();
            const QPointF currentCenter = mapToScene(viewport()->rect().center());
            const int delta = orientation() == Qt::Horizontal
                ? static_cast<int>(currentCenter.x() - targetCenter.x())
                : static_cast<int>(currentCenter.y() - targetCenter.y());
            scrollSmooth(delta);
        } else {
            QGraphicsView::centerOn(geometry.center());
        }
    } else {
        // partially show the next thumb if possible
        if(orientation() == Qt::Vertical) {
            if(height() > geometry.height() * 2)
                ensureVisible(geometry, 0, geometry.height() / 2);
            else
                ensureVisible(geometry, 0, 0);
        } else {
            if(width() > geometry.width() * 2)
                ensureVisible(geometry, geometry.width() / 2, 0);
            else
                ensureVisible(geometry, 0, 0);
        }
    }
    refreshVisibleItems();
    loadVisibleThumbnails();
}

void ThumbnailStrip::focusOnSelection() {
    if(selection().isEmpty())
        return;
    focusOn(selection().constLast());
}

void ThumbnailStrip::readSettings() {
    int currentRes = settings->thumbnailResolution();
    const int oldThumbnailSize = mThumbnailSize;
    if (currentRes != lastThumbnailResolution) {
        unloadAllThumbnails();
        lastThumbnailResolution = currentRes;
    }
    if(settings->thumbPanelStyle() == TH_PANEL_SIMPLE)
        mCurrentStyle = THUMB_SIMPLE;
    else
        mCurrentStyle = THUMB_NORMAL_CENTERED;

    mThumbnailSize = qBound(20, settings->panelPreviewsSize(), 300);

    if(settings->panelPosition() == PANEL_TOP || settings->panelPosition() == PANEL_BOTTOM) {
        ThumbnailView::setOrientation(Qt::Horizontal);
        thumbMarginX = 2;
        thumbMarginY = 4;
    } else {
        ThumbnailView::setOrientation(Qt::Vertical);
        thumbMarginX = 12;
        thumbMarginY = 2;
    }

    // apply style, size & reposition
    cachedItemSize = {};
    if(oldThumbnailSize != mThumbnailSize)
        unloadAllThumbnails();
    for(auto *widget : std::as_const(thumbnails)) {
        widget->setPadding(thumbPadding);
        widget->setMargins(thumbMarginX, thumbMarginY);
        widget->setThumbStyle(mCurrentStyle);
        widget->setThumbnailSize(mThumbnailSize);
        widget->setUseThumbPanelColors(true);
    }
    fitSceneToContents();
    updateLayout();
    setCropThumbnails(settings->squareThumbnails());
    focusOn(lastSelected());
}

QSize ThumbnailStrip::itemSize() const {
    if(cachedItemSize.isValid())
        return cachedItemSize;
    ThumbnailWidget prototype;
    prototype.setPadding(thumbPadding);
    prototype.setMargins(thumbMarginX, thumbMarginY);
    prototype.setThumbStyle(mCurrentStyle);
    prototype.setThumbnailSize(mThumbnailSize);
    prototype.setUseThumbPanelColors(true);
    cachedItemSize = prototype.boundingRect().size().toSize();
    return cachedItemSize;
}

QRectF ThumbnailStrip::itemGeometry(int index) const {
    if(index < 0 || index >= itemCount())
        return {};
    const QSize size = itemSize();
    if(orientation() == Qt::Horizontal)
        return QRectF(index * size.width(), 0, size.width(), size.height());
    return QRectF(0, index * size.height(), size.width(), size.height());
}

QSizeF ThumbnailStrip::contentSize() const {
    const QSize size = itemSize();
    if(orientation() == Qt::Horizontal)
        return QSizeF(itemCount() * size.width(), size.height());
    return QSizeF(size.width(), itemCount() * size.height());
}

QPair<int, int> ThumbnailStrip::itemRangeForRect(const QRectF &rect) const {
    if(itemCount() == 0)
        return {-1, -1};
    const QSize size = itemSize();
    const qreal start = orientation() == Qt::Horizontal ? rect.left() : rect.top();
    const qreal end = orientation() == Qt::Horizontal ? rect.right() : rect.bottom();
    const int extent = orientation() == Qt::Horizontal ? size.width() : size.height();
    const int first = qBound(0, static_cast<int>(std::floor(start / extent)), itemCount() - 1);
    const int last = qBound(0, static_cast<int>(std::floor(end / extent)), itemCount() - 1);
    return first <= last ? QPair<int, int>{first, last} : QPair<int, int>{-1, -1};
}

int ThumbnailStrip::widgetPoolCapacity() const {
    if(itemCount() == 0)
        return 0;
    const QSize size = itemSize();
    const int viewportExtent = orientation() == Qt::Horizontal ? viewport()->width() : viewport()->height();
    const int itemExtent = orientation() == Qt::Horizontal ? size.width() : size.height();
    const qreal virtualExtent = viewportExtent + kPreloadSideCount * offscreenPreloadArea;
    return static_cast<int>(std::ceil(virtualExtent / itemExtent)) + kPoolSafetyItemCount;
}

void ThumbnailStrip::resizeEvent(QResizeEvent *event) {
    ThumbnailView::resizeEvent(event);
    loadVisibleThumbnailsDelayed();
}
