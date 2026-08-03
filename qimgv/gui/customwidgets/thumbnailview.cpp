#include "thumbnailview.h"
#include "settings.h"
#include "utils/displayutils.h"

#include <QScopedValueRollback>

#include <algorithm>
#include <utility>

ThumbnailView::ThumbnailView(Qt::Orientation _orientation, QWidget *parent)
    : QGraphicsView(parent),
      blockThumbnailLoading(false),
      mCropThumbnails(false),
      mouseReleaseSelect(false),
      mDrawScrollbarIndicator(true),
      mThumbnailSize(120),
      rangeSelection(false),
      selectMode(ACTIVATE_BY_PRESS),
      lastScrollFrameTime(0),
      scrollTimeLine(nullptr)
{
    setAccessibleName("thumbnailView");
    this->setMouseTracking(true);
    this->setAcceptDrops(false);
    this->setScene(&scene);
    setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
    setAttribute(Qt::WA_TranslucentBackground, false);
    this->setOptimizationFlag(QGraphicsView::DontAdjustForAntialiasing, true);
    this->setOptimizationFlag(QGraphicsView::DontSavePainterState, true);
    setRenderHint(QPainter::Antialiasing, false);
    setRenderHint(QPainter::SmoothPixmapTransform, false);

    setOrientation(_orientation);

    lastTouchpadScroll.start();

    connect(&loadTimer, &QTimer::timeout, this, &ThumbnailView::loadVisibleThumbnails);
    loadTimer.setInterval(static_cast<const int>(LOAD_DELAY));
    loadTimer.setSingleShot(true);

    createScrollTimeLine();

    horizontalScrollBar()->setContextMenuPolicy(Qt::NoContextMenu);
    horizontalScrollBar()->installEventFilter(this);
    connect(horizontalScrollBar(), &QScrollBar::valueChanged, [this]() {
        if(!layoutUpdateInProgress)
            loadVisibleThumbnails();
    });
    verticalScrollBar()->setContextMenuPolicy(Qt::NoContextMenu);
    verticalScrollBar()->installEventFilter(this);
    connect(verticalScrollBar(), &QScrollBar::valueChanged, [this]() {
        if(!layoutUpdateInProgress)
            loadVisibleThumbnails();
    });
    if(qApp->platformName() == "wayland")
        wayland = true;

    connect(this, &QGraphicsView::rubberBandChanged, this, [this](QRect viewportRect, QPointF fromScenePoint, QPointF toScenePoint) {
        if(viewportRect.isNull()) {
            return;
        }
        QPainterPath path;
        path.addRect(QRectF(fromScenePoint, toScenePoint).normalized());
        QList<QGraphicsItem *> items = scene.items(path, Qt::IntersectsItemBoundingRect);

        QList<int> newSelection = rubberBandStartSelection;
        bool ctrl = qApp->keyboardModifiers() & Qt::ControlModifier;
        bool shift = qApp->keyboardModifiers() & Qt::ShiftModifier;

        for(auto *item : items) {
            ThumbnailWidget* widget = qgraphicsitem_cast<ThumbnailWidget*>(item);
            if(widget) {
                const int idx = indexForWidget(widget);
                if(!checkRange(idx))
                    continue;
                if (ctrl) {
                    if (rubberBandStartSelection.contains(idx)) {
                        newSelection.removeAll(idx);
                    } else {
                        if (!newSelection.contains(idx)) newSelection << idx;
                    }
                } else if (shift) {
                    if (!newSelection.contains(idx)) newSelection << idx;
                } else {
                    if (!newSelection.contains(idx)) newSelection << idx;
                }
            }
        }
        std::sort(newSelection.begin(), newSelection.end());
        select(newSelection);
    });

    connect(settings, &Settings::settingsChanged, this, [this]() {
        for (auto *widget : thumbnails) {
            if (widget) {
                widget->update();
            }
        }
        if (viewport()) {
            viewport()->update();
        }
    });
}

Qt::Orientation ThumbnailView::orientation() const {
    return mOrientation;
}

void ThumbnailView::setOrientation(Qt::Orientation _orientation) {
    mOrientation = _orientation;
    if(mOrientation == Qt::Horizontal) {
        this->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        this->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scrollBar = this->horizontalScrollBar();
        centerOn = [this](int value) {
            QGraphicsView::centerOn(value + 1, viewportCenter.y());
        };
    } else {
        this->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        this->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        scrollBar = this->verticalScrollBar();
        centerOn = [this](int value) {
            QGraphicsView::centerOn(viewportCenter.x(), value + 1);
        };
    }
}

void ThumbnailView::hideEvent(QHideEvent *event) {
    QGraphicsView::hideEvent(event);
    rangeSelection = false;
}

void ThumbnailView::createScrollTimeLine() {
    if(scrollTimeLine) {
        scrollTimeLine->stop();
        scrollTimeLine->deleteLater();
    }
    scrollRefreshRate = DisplayUtils::animationTimerIntervalMs(this);
    scrollTimeLine = new QTimeLine(SCROLL_DURATION, this);
    scrollTimeLine->setEasingCurve(QEasingCurve::OutSine);
    scrollTimeLine->setUpdateInterval(scrollRefreshRate);

    connect(scrollTimeLine, &QTimeLine::frameChanged, [this](int value) {
        scrollFrameTimer.start();
        this->centerOn(value);
        lastScrollFrameTime = scrollFrameTimer.elapsed();
        if(scrollTimeLine->state() == QTimeLine::Running && lastScrollFrameTime > scrollRefreshRate) {
            scrollTimeLine->setPaused(true);
            int newTime = qMin(scrollTimeLine->duration(), scrollTimeLine->currentTime() + lastScrollFrameTime);
            scrollTimeLine->setCurrentTime(newTime);
            scrollTimeLine->setPaused(false);
        }
    });

    connect(scrollTimeLine, &QTimeLine::finished, [this]() {
        blockThumbnailLoading = false;
        loadVisibleThumbnails();
    });
}

bool ThumbnailView::eventFilter(QObject *o, QEvent *ev) {
    if (o == horizontalScrollBar() || o == verticalScrollBar()) {
        if(ev->type() == QEvent::Wheel) {
            this->wheelEvent(dynamic_cast<QWheelEvent*>(ev));
            return true;
        } else if(ev->type() == QEvent::Paint && mDrawScrollbarIndicator) {
            QPainter p(scrollBar);
            p.setOpacity(0.3f);
            p.fillRect(indicator, QBrush(Qt::gray));
            p.setOpacity(1.0f);
            return false;
        }

    }
    return QObject::eventFilter(o, ev);
}

void ThumbnailView::setDirectoryPath(QString path) {
    Q_UNUSED(path)
}

void ThumbnailView::select(QList<int> indices) {
    for(auto i : std::as_const(mSelection))
        if(auto *widget = widgetForIndex(i))
            widget->setHighlighted(false);
    QList<int>::iterator it = indices.begin();
    while(it != indices.end()) {
        // sanity check
        if(*it < 0 || *it >= itemCount()) {
            it = indices.erase(it);
        } else {
            if(auto *widget = widgetForIndex(*it))
                widget->setHighlighted(true);
            ++it;
        }
    }
    mSelection = indices;
    updateScrollbarIndicator();
    emit selectionChanged();
}

void ThumbnailView::select(int index) {
    // fallback
    if(!checkRange(index))
        index = 0;
    this->select(QList<int>() << index);
}

void ThumbnailView::deselect(int index) {
    if(!checkRange(index))
            return;
    if(mSelection.count() > 1) {
        mSelection.removeAll(index);
        if(auto *widget = widgetForIndex(index))
            widget->setHighlighted(false);
        emit selectionChanged();
    }
}

void ThumbnailView::addSelectionRange(int indexTo) {
    if(!rangeSelectionSnapshot.count() || !selection().count())
        return;
    auto list = rangeSelectionSnapshot;
    if(indexTo > rangeSelectionSnapshot.last()) {
        for(int i = rangeSelectionSnapshot.last() + 1; i <= indexTo; i++) {
            if(list.contains(i))
                list.removeAll(i);
            list << i;
        }
    } else {
        for(int i = rangeSelectionSnapshot.last() - 1; i >= indexTo; i--) {
            if(list.contains(i))
                list.removeAll(i);
            list << i;
        }
    }
    select(list);
}

QList<int> ThumbnailView::selection() {
    return mSelection;
}

void ThumbnailView::clearSelection() {
    for(auto i : std::as_const(mSelection))
        if(auto *widget = widgetForIndex(i))
            widget->setHighlighted(false);
    mSelection.clear();
    emit selectionChanged();
}

int ThumbnailView::lastSelected() {
    if(!selection().count())
        return -1;
    else
        return selection().constLast();
}

int ThumbnailView::itemCount() const {
    return mItemCount;
}

void ThumbnailView::show() {
    QGraphicsView::show();
    focusOnSelection();
    loadVisibleThumbnails();
}

void ThumbnailView::showEvent(QShowEvent *event) {
    QGraphicsView::showEvent(event);
    fitSceneToContents();
    refreshVisibleItems();
    updateScrollbarIndicator();
    loadVisibleThumbnails();
}

void ThumbnailView::populate(int newCount) {
    if(newCount < 0) {
        qWarning() << "ThumbnailView received a negative item count:" << newCount;
        return;
    }
    clearSelection();
    lastScrollDirection = SCROLL_FORWARDS;
    mItemCount = newCount;
    loadedThumbnails.clear();
    pendingThumbnailRequests.clear();
    unavailableThumbnails.clear();
    visibleThumbnailsReadyReported = false;
    fitSceneToContents();
    resetViewport();
    refreshVisibleItems(true);
    updateScrollbarIndicator();
    loadVisibleThumbnails();
    notifyIfVisibleThumbnailsReady();
}

void ThumbnailView::addItem() {
    insertItem(itemCount());
}

// insert at index
void ThumbnailView::insertItem(int index) {
    if(index < 0 || index > itemCount()) {
        qWarning() << "ThumbnailView cannot insert out-of-range index:" << index;
        return;
    }

    auto newSelection = mSelection;
    for(int i=0; i < newSelection.count(); i++) {
        if(index <= newSelection[i])
            newSelection[i]++;
    }
    shiftCachedItems(index, 1);
    shiftBoundItems(index, 1);
    ++mItemCount;
    fitSceneToContents();
    refreshVisibleItems();
    select(newSelection);

    updateScrollbarIndicator();
    loadVisibleThumbnailsDelayed();
}

void ThumbnailView::removeItem(int index) {
    if(checkRange(index)) {
        auto newSelection = mSelection;
        clearSelection();
        if(auto *widget = widgetForIndex(index))
            unbindWidget(widget);
        loadedThumbnails.remove(index);
        pendingThumbnailRequests.remove(index);
        unavailableThumbnails.remove(index);
        shiftCachedItems(index + 1, -1);
        shiftBoundItems(index + 1, -1);
        --mItemCount;
        newSelection.removeAll(index);
        for(int i=0; i < newSelection.count(); i++) {
            if(newSelection[i] >= index)
                newSelection[i]--;
        }
        if(!newSelection.count() && itemCount())
            newSelection << ((index >= itemCount()) ? itemCount() - 1 : index);
        fitSceneToContents();
        refreshVisibleItems();
        select(newSelection);
        updateScrollbarIndicator();
        loadVisibleThumbnailsDelayed();
    } else {
        qWarning() << "ThumbnailView cannot remove out-of-range index:" << index;
    }
}

void ThumbnailView::reloadItem(int index) {
    if(!checkRange(index))
        return;
    loadedThumbnails.remove(index);
    unavailableThumbnails.remove(index);
    pendingThumbnailRequests.insert(index);
    if(auto *widget = widgetForIndex(index))
        widget->unsetThumbnail();
    emit thumbnailsRequested(QList<int>() << index, static_cast<int>(qApp->devicePixelRatio() * mThumbnailSize), mCropThumbnails, true);
}

void ThumbnailView::setDragHover(int index) {

}

void ThumbnailView::setCropThumbnails(bool mode) {
    if(mode != mCropThumbnails) {
        unloadAllThumbnails();
        mCropThumbnails = mode;
        loadVisibleThumbnails();
    }
}

void ThumbnailView::setDrawScrollbarIndicator(bool mode) {
    mDrawScrollbarIndicator = mode;
}

void ThumbnailView::setThumbnail(int pos, std::shared_ptr<Thumbnail> thumb) {
    if(thumb && thumb->size() == floor(mThumbnailSize * qApp->devicePixelRatio()) && checkRange(pos)) {
        pendingThumbnailRequests.remove(pos);
        unavailableThumbnails.remove(pos);
        auto *widget = widgetForIndex(pos);
        if(widget || !settings->unloadThumbs())
            loadedThumbnails.insert(pos, thumb);
        if(widget)
            widget->setThumbnail(std::move(thumb));
        notifyIfVisibleThumbnailsReady();
    }
}

void ThumbnailView::setThumbnailUnavailable(int pos, int size) {
    const int expectedSize = floor(mThumbnailSize * qApp->devicePixelRatio());
    if(size != expectedSize || !checkRange(pos))
        return;

    pendingThumbnailRequests.remove(pos);
    unavailableThumbnails.insert(pos);
    notifyIfVisibleThumbnailsReady();
}

void ThumbnailView::unloadAllThumbnails() {
    loadedThumbnails.clear();
    pendingThumbnailRequests.clear();
    unavailableThumbnails.clear();
    visibleThumbnailsReadyReported = false;
    for(auto *widget : std::as_const(thumbnails))
        widget->unsetThumbnail();
}

void ThumbnailView::setBlockThumbnailLoading(bool block) {
    if (blockThumbnailLoading != block) {
        blockThumbnailLoading = block;
        if (!blockThumbnailLoading) {
            loadVisibleThumbnails();
        }
    }
}

void ThumbnailView::loadVisibleThumbnails() {
    loadTimer.stop();
    if(isVisible())
        refreshVisibleItems();
    if(isVisible() && !blockThumbnailLoading) {
        QList<int> loadList;
        loadList.reserve(boundWidgets.size());
        for(auto it = boundWidgets.cbegin(); it != boundWidgets.cend(); ++it) {
            if(!it.value()->isLoaded &&
               !pendingThumbnailRequests.contains(it.key()) &&
               !unavailableThumbnails.contains(it.key()))
                loadList.append(it.key());
        }
        std::sort(loadList.begin(), loadList.end());
        if(lastScrollDirection == SCROLL_BACKWARDS)
            std::reverse(loadList.begin(), loadList.end());
        if(!loadList.isEmpty()) {
            for(const int index : std::as_const(loadList))
                pendingThumbnailRequests.insert(index);
            emit thumbnailsRequested(loadList, static_cast<int>(qApp->devicePixelRatio() * mThumbnailSize), mCropThumbnails, false);
        }
        if(settings->unloadThumbs()) {
            for(auto it = loadedThumbnails.begin(); it != loadedThumbnails.end();) {
                if(!boundWidgets.contains(it.key()))
                    it = loadedThumbnails.erase(it);
                else
                    ++it;
            }
        }
        notifyIfVisibleThumbnailsReady();
    }
}

bool ThumbnailView::visibleThumbnailsLoaded() const {
    if(!isVisible())
        return false;

    const QRectF visibleRect = mapToScene(viewport()->rect()).boundingRect();
    const auto range = itemRangeForRect(visibleRect);
    if(!checkRange(range.first) || !checkRange(range.second))
        return itemCount() == 0;

    for(int index = range.first; index <= range.second; ++index) {
        if(!loadedThumbnails.contains(index) &&
           !unavailableThumbnails.contains(index))
            return false;
    }
    return true;
}

void ThumbnailView::notifyIfVisibleThumbnailsReady() {
    if(!visibleThumbnailsReadyReported && visibleThumbnailsLoaded()) {
        visibleThumbnailsReadyReported = true;
        emit visibleThumbnailsReady();
    }
}

void ThumbnailView::loadVisibleThumbnailsDelayed() {
    loadTimer.stop();
    loadTimer.start();
}

void ThumbnailView::resetViewport() {
    if(scrollTimeLine->state() == QTimeLine::Running)
        scrollTimeLine->stop();
    blockThumbnailLoading = false;
    const QScopedValueRollback updatingLayout(layoutUpdateInProgress, true);
    scrollBar->setValue(0);
}

int ThumbnailView::thumbnailSize() {
    return mThumbnailSize;
}

bool ThumbnailView::atSceneStart() {
    if(mOrientation == Qt::Horizontal) {
        if(viewportTransform().dx() == 0.0)
            return true;
    } else {
        if(viewportTransform().dy() == 0.0)
            return true;
    }
    return false;
}

bool ThumbnailView::atSceneEnd() {
    if(mOrientation == Qt::Horizontal) {
        if(viewportTransform().dx() == viewport()->width() - sceneRect().width())
            return true;
    } else {
        if(viewportTransform().dy() == viewport()->height() - sceneRect().height())
            return true;
    }
    return false;
}

bool ThumbnailView::checkRange(int pos) const {
    return pos >= 0 && pos < itemCount();
}

void ThumbnailView::updateLayout() {
    refreshVisibleItems();
}

// Fit the scene using logical geometry, independent of pooled graphics items.
void ThumbnailView::fitSceneToContents() {
    const QScopedValueRollback updatingLayout(layoutUpdateInProgress, true);
    const QPointF center = mapToScene(viewport()->rect().center());
    const QSizeF logicalSize = contentSize();
    if(mOrientation == Qt::Vertical) {
        const qreal height = qMax(logicalSize.height(), static_cast<qreal>(viewport()->height()));
        const qreal width = qMax(logicalSize.width(), static_cast<qreal>(viewport()->width()));
        scene.setSceneRect(QRectF(QPointF(0, 0), QSizeF(width, height)));
        QGraphicsView::centerOn(0, center.y() + 1);
    } else {
        const qreal width = qMax(logicalSize.width(), static_cast<qreal>(viewport()->width()));
        const qreal height = qMax(logicalSize.height(), static_cast<qreal>(viewport()->height()));
        scene.setSceneRect(QRectF(QPointF(0, 0), QSizeF(width, height)));
        QGraphicsView::centerOn(center.x() + 1, 0);
    }
}

QRectF ThumbnailView::preloadRect() const {
    QRectF rect = mapToScene(viewport()->rect()).boundingRect();
    if(mOrientation == Qt::Horizontal)
        rect.adjust(-offscreenPreloadArea, 0, offscreenPreloadArea, 0);
    else
        rect.adjust(0, -offscreenPreloadArea, 0, offscreenPreloadArea);
    return rect;
}

ThumbnailWidget *ThumbnailView::widgetForIndex(int index) const {
    return boundWidgets.value(index, nullptr);
}

int ThumbnailView::indexForWidget(const ThumbnailWidget *widget) const {
    return widgetIndices.value(widget, -1);
}

void ThumbnailView::bindWidget(ThumbnailWidget *widget, int index) {
    if(!widget || !checkRange(index)) {
        qWarning() << "ThumbnailView cannot bind widget to index:" << index;
        return;
    }
    if(widgetIndices.contains(widget))
        unbindWidget(widget);
    else
        widget->reset();
    boundWidgets.insert(index, widget);
    widgetIndices.insert(widget, index);
    widget->setGeometry(itemGeometry(index));
    widget->setHighlighted(mSelection.contains(index));
    const auto loaded = loadedThumbnails.constFind(index);
    if(loaded != loadedThumbnails.cend())
        widget->setThumbnail(loaded.value());
    widget->show();
}

void ThumbnailView::unbindWidget(ThumbnailWidget *widget) {
    if(!widget)
        return;
    const auto binding = widgetIndices.constFind(widget);
    if(binding != widgetIndices.cend()) {
        boundWidgets.remove(binding.value());
        widgetIndices.remove(widget);
    }
    widget->reset();
    widget->hide();
}

void ThumbnailView::refreshVisibleItems(bool clearBindings) {
    if(clearBindings) {
        for(auto *widget : std::as_const(thumbnails))
            unbindWidget(widget);
    }

    const auto range = itemRangeForRect(preloadRect());
    const bool hasRange = checkRange(range.first) && checkRange(range.second) && range.first <= range.second;

    for(auto it = boundWidgets.begin(); it != boundWidgets.end();) {
        if(!hasRange || it.key() < range.first || it.key() > range.second) {
            ThumbnailWidget *widget = it.value();
            widgetIndices.remove(widget);
            it = boundWidgets.erase(it);
            widget->reset();
            widget->hide();
        } else {
            ++it;
        }
    }

    if(hasRange) {
        QList<ThumbnailWidget*> freeWidgets;
        for(auto *widget : std::as_const(thumbnails)) {
            if(!widgetIndices.contains(widget))
                freeWidgets.append(widget);
        }

        for(int index = range.first; index <= range.second; ++index) {
            ThumbnailWidget *widget = widgetForIndex(index);
            if(!widget) {
                if(freeWidgets.isEmpty()) {
                    auto createdWidget = createThumbnailWidget();
                    if(!createdWidget) {
                        qWarning() << "ThumbnailView failed to create a pooled widget";
                        break;
                    }
                    widget = createdWidget.get();
                    widget->setParent(this);
                    scene.addItem(widget);
                    thumbnails.append(widget);
                    createdWidget.release();
                } else {
                    widget = freeWidgets.takeLast();
                }
                bindWidget(widget, index);
            } else {
                const QRectF geometry = itemGeometry(index);
                if(widget->geometry() != geometry)
                    widget->setGeometry(geometry);
            }
        }
    }

    trimWidgetPool();
}

void ThumbnailView::trimWidgetPool() {
    const int capacity = qMin(itemCount(), qMax(0, widgetPoolCapacity()));
    for(int i = thumbnails.count() - 1; i >= 0 && thumbnails.count() > capacity; --i) {
        ThumbnailWidget *widget = thumbnails.at(i);
        if(widgetIndices.contains(widget))
            continue;
        thumbnails.removeAt(i);
        scene.removeItem(widget);
        widget->deleteLater();
    }
}

void ThumbnailView::shiftBoundItems(int firstIndex, int offset) {
    if(offset == 0)
        return;
    QList<int> indices;
    for(auto it = boundWidgets.cbegin(); it != boundWidgets.cend(); ++it) {
        if(it.key() >= firstIndex)
            indices.append(it.key());
    }
    std::sort(indices.begin(), indices.end());
    if(offset > 0)
        std::reverse(indices.begin(), indices.end());
    for(const int oldIndex : std::as_const(indices)) {
        ThumbnailWidget *widget = boundWidgets.take(oldIndex);
        const int newIndex = oldIndex + offset;
        boundWidgets.insert(newIndex, widget);
        widgetIndices.insert(widget, newIndex);
    }
}

void ThumbnailView::shiftCachedItems(int firstIndex, int offset) {
    if(offset == 0)
        return;
    QHash<int, std::shared_ptr<Thumbnail>> shiftedThumbnails;
    shiftedThumbnails.reserve(loadedThumbnails.size());
    for(auto it = loadedThumbnails.cbegin(); it != loadedThumbnails.cend(); ++it) {
        const int index = it.key() >= firstIndex ? it.key() + offset : it.key();
        shiftedThumbnails.insert(index, it.value());
    }
    loadedThumbnails.swap(shiftedThumbnails);

    QSet<int> shiftedRequests;
    shiftedRequests.reserve(pendingThumbnailRequests.size());
    for(const int index : std::as_const(pendingThumbnailRequests))
        shiftedRequests.insert(index >= firstIndex ? index + offset : index);
    pendingThumbnailRequests.swap(shiftedRequests);

    QSet<int> shiftedUnavailable;
    shiftedUnavailable.reserve(unavailableThumbnails.size());
    for(const int index : std::as_const(unavailableThumbnails))
        shiftedUnavailable.insert(index >= firstIndex ? index + offset : index);
    unavailableThumbnails.swap(shiftedUnavailable);
}

//################### scrolling ######################
void ThumbnailView::wheelEvent(QWheelEvent *event) {
    event->accept();

    int pixelDelta = event->pixelDelta().y();
    int angleDelta = event->angleDelta().y();
    bool isWheel = true;
    if(settings->trackpadDetection()) {
        if(wayland) // we should have scroll phase support
            isWheel = (event->phase() == Qt::NoScrollPhase);
        else // fallback to guesswork
            isWheel = angleDelta && (abs(angleDelta)>=120 && !(angleDelta % 60)) && lastTouchpadScroll.elapsed() > 250;
    }
    //qDebug() << "isWheel:" << isWheel << " angle / pixel delta:" << angleDelta << pixelDelta << lastTouchpadScroll.elapsed() << event->phase() << " wayland:" << wayland;

    if(isWheel) {
        angleDelta *= settings->mouseScrollingSpeed();
        if(!settings->enableSmoothScroll()) {
            if(pixelDelta)
                scrollByItem(pixelDelta);
            else if(angleDelta)
                scrollByItem(angleDelta);
        } else if(angleDelta) { // what about pixelDelta?
            scrollSmooth(angleDelta, WHEEL_SCROLL_MULTIPLIER, SCROLL_ACCELERATION, true);
        }
    } else {
        lastTouchpadScroll.restart();
        // one of these (pixel/angleDelta) may be multiplied by some scale value
        // we'll use whichever is larger
        bool useAngleDelta = abs(angleDelta) > abs(pixelDelta);
        if(useAngleDelta)
            scrollPrecise(angleDelta);
        else
            scrollPrecise(pixelDelta);
    }
}

void ThumbnailView::scrollToEdge(bool end) {
    viewportCenter = mapToScene(viewport()->rect().center());
    int start = (mOrientation == Qt::Horizontal) ? viewportCenter.x() : viewportCenter.y();
    int target = end ? (mOrientation == Qt::Horizontal ? sceneRect().width() : sceneRect().height()) : 0;

    if (start == target)
        return;

    int distance = std::abs(target - start);
    int duration = qBound(300, (int)(250 + std::sqrt(distance) * 5), 1500);

    scrollTimeLine->stop();
    scrollTimeLine->setDuration(duration);
    scrollTimeLine->setFrameRange(start, target);
    scrollTimeLine->start();
}

void ThumbnailView::scrollPrecise(int delta) {
    if(delta < 0)
        lastScrollDirection = SCROLL_FORWARDS;
    else
        lastScrollDirection = SCROLL_BACKWARDS;
    viewportCenter = mapToScene(viewport()->rect().center());
    if(scrollTimeLine->state() == QTimeLine::Running) {
        scrollTimeLine->stop();
        blockThumbnailLoading = false;
    }
    // ignore if we reached boundaries
    if( (delta > 0 && atSceneStart()) || (delta < 0 && atSceneEnd()) )
        return;
    // pixel scrolling (precise)
    if(mOrientation == Qt::Horizontal)
        centerOn(static_cast<int>(viewportCenter.x() - delta));
    else
        centerOn(static_cast<int>(viewportCenter.y() - delta));
}

// windows explorer-like behavior
// scrolls exactly by item width / height
void ThumbnailView::scrollByItem(int delta) {
    // do not scroll less than a certain value in px, to avoid feeling unresponsive
    constexpr int kMaximumMinimumScrollPx = 100;
    const int minimumScroll = qMin(thumbnailSize() / 2, kMaximumMinimumScrollPx);
    const QRectF visibleRect = mapToScene(viewport()->rect()).boundingRect().adjusted(
        -minimumScroll, -minimumScroll, minimumScroll, minimumScroll);
    const auto range = itemRangeForRect(visibleRect);
    if(!checkRange(range.first) || !checkRange(range.second))
        return;
    const int target = delta > 0 ? range.first - 1 : range.second + 1;
    scrollToItem(target);
}

void ThumbnailView::scrollToItem(int index) {
    if(!checkRange(index))
        return;
    const QRectF visibleRect = mapToScene(viewport()->rect()).boundingRect();
    const QRectF targetRect = itemGeometry(index);
    const bool visible = visibleRect.contains(targetRect);
    if(!visible) {
        int delta = 0;
        if(mOrientation == Qt::Vertical) {
            if(targetRect.top() >= visibleRect.top())
                delta = visibleRect.bottom() - targetRect.bottom();
            else // DOWN
                delta = visibleRect.top() - targetRect.top();
        } else {
            if(targetRect.left() >= visibleRect.left())
                delta = visibleRect.right() - targetRect.right();
            else // RIGHT
                delta = visibleRect.left() - targetRect.left();
        }
        if(settings->enableSmoothScroll())
            scrollSmooth(delta);
        else
            scrollPrecise(delta);
    }
}

void ThumbnailView::scrollSmooth(int delta, qreal multiplier, qreal acceleration, bool additive) {
    if(delta < 0)
        lastScrollDirection = SCROLL_FORWARDS;
    else
        lastScrollDirection = SCROLL_BACKWARDS;
    viewportCenter = mapToScene(viewport()->rect().center());
    // ignore if we reached boundaries
    if( (delta > 0 && atSceneStart()) || (delta < 0 && atSceneEnd()) ) {
        return;
    }
    int center;
    if(mOrientation == Qt::Horizontal)
        center = static_cast<int>(viewportCenter.x());
    else
        center = static_cast<int>(viewportCenter.y());
    bool redirect = false, accelerate = false;
    int newEndFrame = center - static_cast<int>(delta * multiplier);
    if( (newEndFrame < center && center < scrollTimeLine->endFrame()) ||
        (newEndFrame > center && center > scrollTimeLine->endFrame()) )
    {
        redirect = true;
    }
    if(scrollTimeLine->state() == QTimeLine::Running || scrollTimeLine->state() == QTimeLine::Paused) {      
        int oldEndFrame = scrollTimeLine->endFrame();
        if(scrollTimeLine->currentTime() < SCROLL_ACCELERATION_THRESHOLD)
            accelerate = true;
        // QTimeLine has this weird issue when it is already finished (at the last frame)
        // but is stuck in the running state. So we just create a new one.
        if(oldEndFrame == center)
            createScrollTimeLine();
        if(!redirect && additive)
            newEndFrame = oldEndFrame - static_cast<int>(delta * multiplier * acceleration);
        // force load thumbs inbetween scroll animations
        blockThumbnailLoading = false;
        loadVisibleThumbnails();
    }
    scrollTimeLine->stop();
    if(accelerate)
        scrollTimeLine->setDuration(SCROLL_DURATION / SCROLL_ACCELERATION);
    else
        scrollTimeLine->setDuration(SCROLL_DURATION);
    blockThumbnailLoading = true;
    scrollTimeLine->setFrameRange(center, newEndFrame);
    scrollTimeLine->start();
}

void ThumbnailView::scrollSmooth(int delta, qreal multiplier, qreal acceleration) {
    scrollSmooth(delta, multiplier, acceleration, false);
}

void ThumbnailView::scrollSmooth(int angleDelta) {
    scrollSmooth(angleDelta, 1.0, 1.0, false);
}

void ThumbnailView::mousePressEvent(QMouseEvent *event) {
    mouseReleaseSelect = false;
    dragStartPos = event->pos();

    if (event->button() == Qt::RightButton) {
        mouseInteraction = THUMB_INTERACTION_NONE;
        return;
    }

    ThumbnailWidget *item = dynamic_cast<ThumbnailWidget*>(itemAt(event->pos()));
    if(item) {
        setDragMode(QGraphicsView::NoDrag);
        const int index = indexForWidget(item);
        if(!checkRange(index))
            return;
        if(event->button() == Qt::LeftButton) {
            if(event->modifiers() & Qt::ControlModifier) {
                if(!selection().contains(index))
                    select(selection() << index);
                else
                    deselect(index);
            } else if(event->modifiers() & Qt::ShiftModifier) {
                addSelectionRange(index);
            } else if (selection().count() <= 1) {
                if(selectMode == ACTIVATE_BY_PRESS) {
                    emit itemActivated(index);
                    return;
                } else {
                    select(index);
                }
            } else {
                mouseReleaseSelect = true;
            }
        }
    } else {
        if(event->button() == Qt::LeftButton) {
            setDragMode(QGraphicsView::RubberBandDrag);
            if (!(event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier))) {
                clearSelection();
            }
            rubberBandStartSelection = mSelection;
        }
    }
    if(event->button() == Qt::BackButton) {
        emit backRequested();
        return;
    } else if(event->button() == Qt::ForwardButton) {
        emit forwardRequested();
        return;
    }
    QGraphicsView::mousePressEvent(event);
}

void ThumbnailView::mouseMoveEvent(QMouseEvent *event) {
    QGraphicsView::mouseMoveEvent(event);

    if (event->buttons() & Qt::RightButton) {
        if (mouseInteraction == THUMB_INTERACTION_NONE) {
            int dx = event->pos().x() - dragStartPos.x();
            int dy = event->pos().y() - dragStartPos.y();
            if (mOrientation == Qt::Horizontal) {
                if (std::abs(dx) > gestureThreshold) {
                    mouseInteraction = THUMB_INTERACTION_GESTURE;
                    scrollToEdge(dx < 0);
                }
            } else {
                if (std::abs(dy) > gestureThreshold) {
                    mouseInteraction = THUMB_INTERACTION_GESTURE;
                    scrollToEdge(dy < 0);
                }
            }
        }
        return;
    }

    if(event->buttons() != Qt::LeftButton || !selection().count())
        return;
    if(QLineF(dragStartPos, event->pos()).length() >= 40) {
        auto *item = dynamic_cast<ThumbnailWidget*>(itemAt(dragStartPos));
        if(item && selection().contains(indexForWidget(item)))
            emit draggedOut();
    }
}

void ThumbnailView::mouseReleaseEvent(QMouseEvent *event) {
    QGraphicsView::mouseReleaseEvent(event);
    if (event->button() == Qt::RightButton) {
        if (mouseInteraction == THUMB_INTERACTION_NONE) {
            ThumbnailWidget *item = dynamic_cast<ThumbnailWidget*>(itemAt(event->pos()));
            if (item) {
                const int clickedIndex = indexForWidget(item);
                if (!selection().contains(clickedIndex)) {
                    select(clickedIndex);
                }
            }
        }
        mouseInteraction = THUMB_INTERACTION_NONE;
        return;
    }
    if(mouseReleaseSelect && QLineF(dragStartPos, event->pos()).length() < 40) {
        ThumbnailWidget *item = dynamic_cast<ThumbnailWidget*>(itemAt(event->pos()));
        if(item) {
            const int index = indexForWidget(item);
            select(index);
        }
    }
}

void ThumbnailView::mouseDoubleClickEvent(QMouseEvent *event) {
    if(event->button() == Qt::LeftButton) {
        ThumbnailWidget *item = dynamic_cast<ThumbnailWidget*>(itemAt(event->pos()));
        if(item) {
            emit itemActivated(indexForWidget(item));
            return;
        }
    }
    event->ignore();
}

void ThumbnailView::focusOutEvent(QFocusEvent *event) {
    QGraphicsView::focusOutEvent(event);
    rangeSelection = false;
}

void ThumbnailView::keyPressEvent(QKeyEvent *event) {
    if(event->key() == Qt::Key_Shift)
        rangeSelectionSnapshot = selection();
    if(event->modifiers() & Qt::ShiftModifier)
        rangeSelection = true;
}

void ThumbnailView::keyReleaseEvent(QKeyEvent *event) {
    if(event->key() == Qt::Key_Shift)
        rangeSelection = false;
}

void ThumbnailView::resizeEvent(QResizeEvent *event) {
    QGraphicsView::resizeEvent(event);
    fitSceneToContents();
    refreshVisibleItems();
    updateScrollbarIndicator();
}

void ThumbnailView::setSelectMode(ThumbnailSelectMode mode) {
    selectMode = mode;
}
