#include "foldergridview.h"
#include "settings.h"
#include "utils/imagelib.h"
#include "gui/customwidgets/contextmenuitem.h"
#include "gui/uimetrics.h"
#include <QMenu>
#include <QAction>
#include <QWidgetAction>
#include <QTimer>
#include <QIcon>
#include <QCursor>
#include <cmath>
#include <utility>

namespace {
constexpr int kContextMenuIconSizePx = UiMetrics::kStandardIconSizePx;
constexpr int kContextMenuWidthPx = 212;
constexpr int kContextMenuHorizontalPaddingPx = 3;
constexpr int kGridLeftMarginPx = 9;
constexpr int kGridTopMarginPx = 6;
constexpr int kGridRightMarginPx = 9;
constexpr int kGridBottomMarginPx = 0;
constexpr int kGridThumbnailPaddingPx = 8;
constexpr int kPageNavigationRows = 4;
constexpr int kSelectionFocusMarginPx = 40;
constexpr qreal kPreloadSideCount = 2.0;
constexpr int kPoolSafetyRowCount = 1;

constexpr bool isSupportedDropAction(Qt::DropAction action) {
    return action == Qt::CopyAction || action == Qt::MoveAction;
}
}

FolderGridView::FolderGridView(QWidget *parent)
    : ThumbnailView(Qt::Vertical, parent),
      shiftedCol(-1)
{
    offscreenPreloadArea = 2300;

    this->setAcceptDrops(true);

    this->viewport()->setAttribute(Qt::WA_OpaquePaintEvent, true);
    this->scene.setBackgroundBrush(settings->colorScheme().folderview);
    this->setCacheMode(QGraphicsView::CacheBackground);

    // turn this off until [multi]selection is implemented
    setDrawScrollbarIndicator(false);
    setSelectMode(ACTIVATE_BY_DOUBLECLICK);

    connect(settings, &Settings::settingsChanged, [this]() {
        this->scene.setBackgroundBrush(settings->colorScheme().folderview);
    });

    setupLayout();
    connect(this, &ThumbnailView::itemActivated,
            this, &FolderGridView::onitemSelected);
}

void FolderGridView::dropEvent(QDropEvent *event) {
    if(!isSupportedDropAction(event->dropAction())) {
        event->ignore();
        return;
    }

    event->accept();
    ThumbnailWidget *item = dynamic_cast<ThumbnailWidget*>(itemAt(event->pos()));
    int index = -1;
    if(item) {
        index = indexForWidget(item);
        item->setDropHovered(false);
    }
    emit droppedInto(event->mimeData(), event->source(), index, event->dropAction());
}

void FolderGridView::dragEnterEvent(QDragEnterEvent *event) {
    if(isSupportedDropAction(event->dropAction()))
        event->accept();
    else
        event->ignore();
}

void FolderGridView::dragMoveEvent(QDragMoveEvent *event) {
    if(!isSupportedDropAction(event->dropAction())) {
        event->ignore();
        if(auto *widget = widgetForIndex(lastDragTarget))
            widget->setDropHovered(false);
        lastDragTarget = -1;
        return;
    }

    event->accept();
    ThumbnailWidget *item = dynamic_cast<ThumbnailWidget*>(itemAt(event->pos()));
    int index = -1;
    if(item)
        index = indexForWidget(item);
    // unselect previous
    if(index != lastDragTarget)
        if(auto *widget = widgetForIndex(lastDragTarget))
            widget->setDropHovered(false);
    emit draggedOver(index);
    lastDragTarget = index;
}

void FolderGridView::dragLeaveEvent(QDragLeaveEvent *event) {
    event->accept();
    if(auto *widget = widgetForIndex(lastDragTarget))
        widget->setDropHovered(false);
}

void FolderGridView::setDragHover(int index) {
    if(!checkRange(index))
        return;
    if(auto *widget = widgetForIndex(index))
        widget->setDropHovered(true);
}

void FolderGridView::onitemSelected() {
    shiftedCol = -1;
}

void FolderGridView::updateScrollbarIndicator() {
    if(!itemCount() || !selection().count())
        return;
    const QRectF geometry = itemGeometry(lastSelected());
    const qreal itemCenter = geometry.center().y() / scene.height();
    indicator = QRect(2, scrollBar->height() * itemCenter - indicatorSize, scrollBar->width() - 4, indicatorSize);
}

// probably unneeded
void FolderGridView::show() {
    ThumbnailView::show();
    setFocus();
}

// probably unneeded
void FolderGridView::hide() {
    ThumbnailView::hide();
    clearFocus();
}

void FolderGridView::setShowLabels(bool mode) {
    thumbnailStyle = mode ? THUMB_NORMAL : THUMB_SIMPLE;
    cachedItemSize = {};
    for(auto *widget : std::as_const(thumbnails))
        widget->setThumbStyle(thumbnailStyle);
    fitSceneToContents();
    updateLayout();
    focusOnSelection();
}

void FolderGridView::focusOnSelection() {
    if(!itemCount() || lastSelected() == -1)
        return;
    ensureVisible(itemGeometry(lastSelected()), 0, 0);
    refreshVisibleItems();
}

void FolderGridView::selectAll() {
    QList<int> list;
    for(int i = 0; i < itemCount(); i++)
        list << i;
    // preserve last selected index by putting it at the end of a new selection
    // this is simpler but it changes selection order a bit
    if(lastSelected() != -1) {
        // in this case list is sorted so no need to indexOf()
        list.move(lastSelected(), list.count() - 1);
    }
    select(list);
}

void FolderGridView::selectAbove() {
    if(!itemCount() || lastSelected() == -1 || sameRow(0, lastSelected()))
        return;
    int newIndex = itemAbove(lastSelected());
    if(shiftedCol >= 0) {
        int diff = shiftedCol - columnOf(lastSelected());
        newIndex += diff;
        shiftedCol = -1;
    }
    if(!checkRange(newIndex))
        newIndex = 0;
    if(rangeSelection)
        addSelectionRange(newIndex);
    else
        select(newIndex);
    scrollToCurrent();
}

void FolderGridView::selectBelow() {
    if(!itemCount() || lastSelected() == -1 || sameRow(lastSelected(), itemCount() - 1))
        return;
    shiftedCol = -1;
    int newIndex = itemBelow(lastSelected());
    if(!checkRange(newIndex))
        newIndex = itemCount() - 1;
    if(columnOf(newIndex) != columnOf(lastSelected()))
        shiftedCol = columnOf(lastSelected());
    if(rangeSelection)
        addSelectionRange(newIndex);
    else
        select(newIndex);
    scrollToCurrent();
}

void FolderGridView::selectNext() {
    if(!itemCount() || lastSelected() == itemCount() - 1)
        return;
    if(!rangeSelection && lastSelected() == itemCount() - 1) {
        select(lastSelected());
        return;
    }
    shiftedCol = -1;
    int newIndex = lastSelected() + 1;
    if(!checkRange(newIndex))
        newIndex = itemCount() - 1;
    if(rangeSelection)
        addSelectionRange(newIndex);
    else
        select(newIndex);
    scrollToCurrent();
}

void FolderGridView::selectPrev() {
    if(!itemCount() || lastSelected() == 0)
        return;
    shiftedCol = -1;
    int newIndex = lastSelected() - 1;
    if(!checkRange(newIndex))
        newIndex = 0;
    if(rangeSelection)
        addSelectionRange(newIndex);
    else
        select(newIndex);
    scrollToCurrent();
}

void FolderGridView::pageUp() {
    if(!itemCount() || lastSelected() == -1 || sameRow(0, lastSelected()))
        return;
    int newIndex = lastSelected();
    int tmp;
    for(int i = 0; i < kPageNavigationRows; i++) {
        tmp = itemAbove(newIndex);
        if(checkRange(tmp))
            newIndex = tmp;
    }
    if(shiftedCol >= 0) {
        int diff = shiftedCol - columnOf(newIndex);
        newIndex += diff;
        shiftedCol = -1;
    }
    if(rangeSelection)
        addSelectionRange(newIndex);
    else
        select(newIndex);
    scrollToCurrent();
}

void FolderGridView::pageDown() {
    if(!itemCount() || lastSelected() == -1 || sameRow(lastSelected(), itemCount() - 1))
        return;
    shiftedCol = -1;
    int newIndex = lastSelected();
    int tmp;
    for(int i = 0; i < kPageNavigationRows; i++) {
        tmp = itemBelow(newIndex);
        if(checkRange(tmp))
            newIndex = tmp;
    }
    if(columnOf(newIndex) != columnOf(lastSelected()))
        shiftedCol = columnOf(lastSelected());
    if(rangeSelection)
        addSelectionRange(newIndex);
    else
        select(newIndex);
    scrollToCurrent();
}

void FolderGridView::selectFirst() {
    if(!itemCount())
        return;
    shiftedCol = -1;
    if(rangeSelection)
        addSelectionRange(0);
    else
        select(0);
    scrollToCurrent();
}

void FolderGridView::selectLast() {
    if(!itemCount())
        return;
    shiftedCol = -1;
    if(rangeSelection)
        addSelectionRange(itemCount() - 1);
    else
        select(itemCount() - 1);
    scrollToCurrent();
}

void FolderGridView::scrollToCurrent() {
    scrollToItem(lastSelected());
}

// same as scrollToItem minus the animation
void FolderGridView::focusOn(int index) {
    if(!checkRange(index))
        return;
    ensureVisible(itemGeometry(index), 0, 0);
    refreshVisibleItems();
    loadVisibleThumbnailsDelayed();
}

void FolderGridView::setupLayout() {
    setAlignment(Qt::AlignLeft | Qt::AlignTop);
    setFrameShape(QFrame::NoFrame);
}

std::unique_ptr<ThumbnailWidget> FolderGridView::createThumbnailWidget() {
    auto widget = std::make_unique<ThumbnailWidget>();
    widget->setPadding(kGridThumbnailPaddingPx);
    widget->setThumbStyle(thumbnailStyle);
    widget->setThumbnailSize(this->mThumbnailSize);
    return widget;
}

FolderGridView::GridGeometry FolderGridView::gridGeometry() const {
    const QSizeF itemSize = thumbnailItemSize();
    const qreal availableWidth = qMax(
        itemSize.width(),
        static_cast<qreal>(viewport()->width() - kGridLeftMarginPx - kGridRightMarginPx));
    const int columns = qMax(1, static_cast<int>(availableWidth / itemSize.width()));
    const int rows = itemCount() > 0 ? (itemCount() + columns - 1) / columns : 0;
    const qreal horizontalOffset = itemCount() >= columns
        ? std::fmod(availableWidth, itemSize.width()) / 2.0
        : 0.0;
    return GridGeometry{itemSize, columns, rows, horizontalOffset};
}

QSizeF FolderGridView::thumbnailItemSize() const {
    if(cachedItemSize.isValid())
        return cachedItemSize;
    ThumbnailWidget prototype;
    prototype.setPadding(kGridThumbnailPaddingPx);
    prototype.setThumbStyle(thumbnailStyle);
    prototype.setThumbnailSize(mThumbnailSize);
    cachedItemSize = prototype.boundingRect().size();
    return cachedItemSize;
}

QRectF FolderGridView::itemGeometry(int index) const {
    if(index < 0 || index >= itemCount())
        return {};
    const GridGeometry geometry = gridGeometry();
    const int row = index / geometry.columns;
    const int column = index % geometry.columns;
    const QPointF position(
        kGridLeftMarginPx + geometry.horizontalOffset + column * geometry.itemSize.width(),
        kGridTopMarginPx + row * geometry.itemSize.height());
    return QRectF(position, geometry.itemSize);
}

QSizeF FolderGridView::contentSize() const {
    const GridGeometry geometry = gridGeometry();
    const qreal height = kGridTopMarginPx
        + geometry.rows * geometry.itemSize.height()
        + kGridBottomMarginPx;
    const qreal width = qMax(
        static_cast<qreal>(viewport()->width()),
        kGridLeftMarginPx + geometry.itemSize.width() + kGridRightMarginPx);
    return QSizeF(width, height);
}

QPair<int, int> FolderGridView::itemRangeForRect(const QRectF &rect) const {
    if(itemCount() == 0)
        return {-1, -1};
    const GridGeometry geometry = gridGeometry();
    const qreal rowHeight = geometry.itemSize.height();
    const int firstRow = qMax(
        0,
        static_cast<int>(std::floor((rect.top() - kGridTopMarginPx) / rowHeight)));
    const int lastRow = qMin(
        geometry.rows - 1,
        static_cast<int>(std::floor((rect.bottom() - kGridTopMarginPx) / rowHeight)));
    if(firstRow > lastRow)
        return {-1, -1};
    return {
        firstRow * geometry.columns,
        qMin(itemCount() - 1, (lastRow + 1) * geometry.columns - 1)
    };
}

int FolderGridView::widgetPoolCapacity() const {
    if(itemCount() == 0)
        return 0;
    const GridGeometry geometry = gridGeometry();
    const qreal virtualHeight = viewport()->height() + kPreloadSideCount * offscreenPreloadArea;
    const int rows = static_cast<int>(std::ceil(virtualHeight / geometry.itemSize.height()))
        + kPoolSafetyRowCount;
    return rows * geometry.columns;
}

int FolderGridView::itemAbove(int index) const {
    const int columns = gridGeometry().columns;
    return checkRange(index - columns) ? index - columns : index;
}

int FolderGridView::itemBelow(int index) const {
    const int columns = gridGeometry().columns;
    return qMin(itemCount() - 1, index + columns);
}

int FolderGridView::columnOf(int index) const {
    return checkRange(index) ? index % gridGeometry().columns : -1;
}

bool FolderGridView::sameRow(int one, int two) const {
    if(!checkRange(one) || !checkRange(two))
        return false;
    const int columns = gridGeometry().columns;
    return one / columns == two / columns;
}

// block native tab-switching so we can use it in shortcuts
bool FolderGridView::focusNextPrevChild(bool) {
    return false;
}

void FolderGridView::keyPressEvent(QKeyEvent *event) {
    ThumbnailView::keyPressEvent(event);
    event->accept();

    if(event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return) {
        emit itemActivated(lastSelected());
        return;
    }

    // temporary, will be configurable later
    if(event->key() == Qt::Key_Backspace) {
        actionManager->invokeAction("goUp");
        return;
    }

    if(event->modifiers() & Qt::ControlModifier) {
        if(ShortcutBuilder::fromEvent(event) == "Ctrl+A")
            selectAll();
        else
            actionManager->processEvent(event);
        return;
    }

    // handle selection
    switch (event->key()) {
    case Qt::Key_Left:
        selectPrev();
        break;
    case Qt::Key_Right:
        selectNext();
        break;
    case Qt::Key_Up:
        selectAbove();
        break;
    case Qt::Key_Down:
        selectBelow();
        break;
    case Qt::Key_PageUp:
        pageUp();
        break;
    case Qt::Key_PageDown:
        pageDown();
        break;
    case Qt::Key_Home:
        selectFirst();
        break;
    case Qt::Key_End:
        selectLast();
        break;
    default:
        actionManager->processEvent(event);
    }
}

void FolderGridView::wheelEvent(QWheelEvent *event) {
    if(event->modifiers().testFlag(Qt::ControlModifier)) {
        if(event->pixelDelta().y() > 0 || event->angleDelta().y() > 0)
            zoomIn();
        else if(event->pixelDelta().y() < 0 || event->angleDelta().y() < 0)
            zoomOut();
    } else {
        ThumbnailView::wheelEvent(event);
    }
}

void FolderGridView::zoomIn() {
    setThumbnailSize(this->mThumbnailSize + ZOOM_STEP);
}

void FolderGridView::zoomOut() {
    setThumbnailSize(this->mThumbnailSize - ZOOM_STEP);
}

void FolderGridView::setThumbnailSize(int newSize) {
    newSize = clamp(newSize, THUMBNAIL_SIZE_MIN, THUMBNAIL_SIZE_MAX);
    if(newSize == mThumbnailSize)
        return;
    unloadAllThumbnails();
    mThumbnailSize = newSize;
    cachedItemSize = {};
    for(auto *widget : std::as_const(thumbnails))
        widget->setThumbnailSize(newSize);
    fitSceneToContents();
    updateLayout();
    if(lastSelected() != -1)
        ensureVisible(itemGeometry(lastSelected()), 0, kSelectionFocusMarginPx);
    emit thumbnailSizeChanged(mThumbnailSize);
    loadVisibleThumbnailsDelayed();
}

void FolderGridView::resizeEvent(QResizeEvent *event) {
    if(this->isVisible()) {
        ThumbnailView::resizeEvent(event);
        loadVisibleThumbnailsDelayed();
    } else {
        QGraphicsView::resizeEvent(event);
    }
}

void FolderGridView::mouseReleaseEvent(QMouseEvent *event) {
    bool isRightClick = (event->button() == Qt::RightButton);
    bool wasGesture = (mouseInteraction == THUMB_INTERACTION_GESTURE);
    ThumbnailView::mouseReleaseEvent(event);

    if (isRightClick && !wasGesture) {
        QMenu menu(this);
        menu.setAttribute(Qt::WA_TranslucentBackground);
        menu.setWindowFlags(menu.windowFlags() | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);

        // styling:
        QString stylesheet =
            "QMenu {"
            " background-color: %1;"
            " border: 1px solid %2;"
            " border-radius: 8px;"
            " padding: 4px %3px;"
            "}"
            "QMenu::separator {"
            " height: 1px;"
            " background-color: %2;"
            " margin: 4px 11px;"
            "}";
        auto scheme = settings->colorScheme();
        stylesheet = stylesheet.arg(
            scheme.widget.name(),
            scheme.widget_border.name())
            .arg(kContextMenuHorizontalPaddingPx);
        menu.setStyleSheet(stylesheet);

        bool hasSelection = !selection().isEmpty();

        auto addCustomAction = [&](const QString &text, FluentIcon icon, const QString &shortcut = "") {
            QWidgetAction *wa = new QWidgetAction(&menu);
            ContextMenuItem *item = new ContextMenuItem(this);
            item->setText(text);
            item->setIcon(icon, kContextMenuIconSizePx);
            item->setMinimumWidth(kContextMenuWidthPx - 2 * kContextMenuHorizontalPaddingPx);
            if (!shortcut.isEmpty()) {
                item->setShortcutText(shortcut);
            }
            wa->setDefaultWidget(item);
            menu.addAction(wa);
            return item;
        };

        ContextMenuItem *itemOpen = addCustomAction(tr("Open only selected"), FluentIcon::OpenOnlySelected20);
        itemOpen->setEnabled(hasSelection);
        connect(itemOpen, &ContextMenuItem::pressed, this, [this, &menu]() {
            menu.close();
            QTimer::singleShot(0, this, [this]() { emit openSelectedRequested(); });
        });

        ContextMenuItem *itemBatch = addCustomAction(tr("Batch convert"), FluentIcon::BatchConvert20);
        itemBatch->setEnabled(hasSelection);
        connect(itemBatch, &ContextMenuItem::pressed, this, [this, &menu]() {
            menu.close();
            emit batchRequested();
        });

        ContextMenuItem *itemAddFolder = addCustomAction(tr("Add folder"), FluentIcon::FolderAdd20, actionManager->shortcutForAction("createDirectory"));
        itemAddFolder->setIconOffset(0, -1);
        connect(itemAddFolder, &ContextMenuItem::pressed, this, [this, &menu]() {
            menu.close();
            actionManager->invokeAction("createDirectory");
        });

        ContextMenuItem *itemShowInFolder = addCustomAction(tr("Show in folder"), FluentIcon::ShowInFolder20, actionManager->shortcutForAction("showInDirectory"));
        itemShowInFolder->setEnabled(hasSelection);
        connect(itemShowInFolder, &ContextMenuItem::pressed, this, [this, &menu]() {
            menu.close();
            actionManager->invokeAction("showInDirectory");
        });

        ContextMenuItem *itemRename = addCustomAction(tr("Rename"), FluentIcon::Rename20);
        itemRename->setEnabled(hasSelection);
        connect(itemRename, &ContextMenuItem::pressed, this, [this, &menu]() {
            menu.close();
            actionManager->invokeAction("renameFile");
        });

        menu.addSeparator();

        ContextMenuItem *itemTrash = addCustomAction(tr("Move to trash"), FluentIcon::Delete20);
        itemTrash->setTextColor(settings->colorScheme().trash);
        itemTrash->setIconColor(settings->colorScheme().trash);
        itemTrash->setEnabled(hasSelection);
        connect(itemTrash, &ContextMenuItem::pressed, this, [this, &menu]() {
            menu.close();
            actionManager->invokeAction("moveToTrash");
        });

        ContextMenuItem *itemDelete = addCustomAction(tr("Delete permanently"), FluentIcon::Dismiss20);
        itemDelete->setIconOffset(0, 1);
        itemDelete->setTextColor(settings->colorScheme().danger);
        itemDelete->setIconColor(settings->colorScheme().danger);
        itemDelete->setEnabled(hasSelection);
        connect(itemDelete, &ContextMenuItem::pressed, this, [this, &menu]() {
            menu.close();
            actionManager->invokeAction("removeFile");
        });

        menu.exec(event->globalPos());
    }

}
