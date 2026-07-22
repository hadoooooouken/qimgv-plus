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

namespace {
constexpr int kContextMenuIconSizePx = UiMetrics::kStandardIconSizePx;
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
    event->accept();
    ThumbnailWidget *item = dynamic_cast<ThumbnailWidget*>(itemAt(event->pos()));
    int index = -1;
    if(item) {
        index = thumbnails.indexOf(item);
        item->setDropHovered(false);
    }
    emit droppedInto(event->mimeData(), event->source(), index);
}

void FolderGridView::dragEnterEvent(QDragEnterEvent *event) {
    event->accept();
}

void FolderGridView::dragMoveEvent(QDragMoveEvent *event) {
    event->accept();
    ThumbnailWidget *item = dynamic_cast<ThumbnailWidget*>(itemAt(event->pos()));
    int index = -1;
    if(item)
        index = thumbnails.indexOf(item);
    // unselect previous
    if(index != lastDragTarget && checkRange(lastDragTarget))
        thumbnails.at(lastDragTarget)->setDropHovered(false);
    emit draggedOver(index);
    lastDragTarget = index;
}

void FolderGridView::dragLeaveEvent(QDragLeaveEvent *event) {
    event->accept();
    if(lastDragTarget < 0 || lastDragTarget >= thumbnails.count())
        return;
    thumbnails.at(lastDragTarget)->setDropHovered(false);
}

void FolderGridView::setDragHover(int index) {
    if(!checkRange(index))
        return;
    auto item = thumbnails.at(index);
    item->setDropHovered(true);
}

void FolderGridView::onitemSelected() {
    shiftedCol = -1;
}

void FolderGridView::updateScrollbarIndicator() {
    if(!thumbnails.count() || !selection().count())
        return;
    ThumbnailWidget *thumb = thumbnails.at(lastSelected());
    qreal itemCenter = (thumb->pos().y() + (thumb->height() / 2)) / scene.height();
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
    ThumbnailStyle style = mode ? THUMB_NORMAL : THUMB_SIMPLE;
    for(int i = 0; i < thumbnails.count(); i++)
        thumbnails.at(i)->setThumbStyle(style);
    updateLayout();
    fitSceneToContents();
    focusOnSelection();
}

void FolderGridView::focusOnSelection() {
    if(!thumbnails.count() || lastSelected() == -1)
        return;
    ThumbnailWidget *thumb = thumbnails.at(lastSelected());
    ensureVisible(thumb, 0, 0);
}

void FolderGridView::selectAll() {
    QList<int> list;
    for(int i = 0; i < thumbnails.count(); i++)
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
    if(!thumbnails.count() || lastSelected() == -1 || flowLayout->sameRow(0, lastSelected()))
        return;
    int newIndex;
    newIndex = flowLayout->itemAbove(lastSelected());
    if(shiftedCol >= 0) {
        int diff = shiftedCol - flowLayout->columnOf(lastSelected());
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
    if(!thumbnails.count() || lastSelected() == -1 || flowLayout->sameRow(lastSelected(), thumbnails.count() - 1))
        return;
    shiftedCol = -1;
    int newIndex = flowLayout->itemBelow(lastSelected());
    if(!checkRange(newIndex))
        newIndex = thumbnails.count() - 1;
    if(flowLayout->columnOf(newIndex) != flowLayout->columnOf(lastSelected()))
        shiftedCol = flowLayout->columnOf(lastSelected());
    if(rangeSelection)
        addSelectionRange(newIndex);
    else
        select(newIndex);
    scrollToCurrent();
}

void FolderGridView::selectNext() {
    if(!thumbnails.count() || lastSelected() == thumbnails.count() - 1)
        return;
    if(!rangeSelection && lastSelected() == thumbnails.count() - 1) {
        select(lastSelected());
        return;
    }
    shiftedCol = -1;
    int newIndex = lastSelected() + 1;
    if(!checkRange(newIndex))
        newIndex = thumbnails.count() - 1;
    if(rangeSelection)
        addSelectionRange(newIndex);
    else
        select(newIndex);
    scrollToCurrent();
}

void FolderGridView::selectPrev() {
    if(!thumbnails.count() || lastSelected() == 0)
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
    if(!thumbnails.count() || lastSelected() == -1 || flowLayout->sameRow(0, lastSelected()))
        return;
    int newIndex = lastSelected();
    int tmp;
    // 4 rows up
    for(int i = 0; i < 4; i++) {
        tmp = flowLayout->itemAbove(newIndex);
        if(checkRange(tmp))
            newIndex = tmp;
    }
    if(shiftedCol >= 0) {
        int diff = shiftedCol - flowLayout->columnOf(newIndex);
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
    if(!thumbnails.count() || lastSelected() == -1 || flowLayout->sameRow(lastSelected(), thumbnails.count() - 1))
        return;
    shiftedCol = -1;
    int newIndex = lastSelected();
    int tmp;
    // 4 rows down
    for(int i = 0; i < 4; i++) {
        tmp = flowLayout->itemBelow(newIndex);
        if(checkRange(tmp))
            newIndex = tmp;
    }
    if(flowLayout->columnOf(newIndex) != flowLayout->columnOf(lastSelected()))
        shiftedCol = flowLayout->columnOf(lastSelected());
    if(rangeSelection)
        addSelectionRange(newIndex);
    else
        select(newIndex);
    scrollToCurrent();
}

void FolderGridView::selectFirst() {
    if(!thumbnails.count())
        return;
    shiftedCol = -1;
    if(rangeSelection)
        addSelectionRange(0);
    else
        select(0);
    scrollToCurrent();
}

void FolderGridView::selectLast() {
    if(!thumbnails.count())
        return;
    shiftedCol = -1;
    if(rangeSelection)
        addSelectionRange(thumbnails.count() - 1);
    else
        select(thumbnails.count() - 1);
    scrollToCurrent();
}

void FolderGridView::scrollToCurrent() {
    scrollToItem(lastSelected());
}

// same as scrollToItem minus the animation
void FolderGridView::focusOn(int index) {
    if(!checkRange(index))
        return;
    ThumbnailWidget *thumb = thumbnails.at(index);
    ensureVisible(thumb, 0, 0);
    loadVisibleThumbnailsDelayed();
}

void FolderGridView::setupLayout() {
    this->setAlignment(Qt::AlignHCenter);

    flowLayout = new FlowLayout();
    flowLayout->setContentsMargins(9,6,9,0);
    setFrameShape(QFrame::NoFrame);
    scene.addItem(&holderWidget);
    holderWidget.setLayout(flowLayout);
    holderWidget.setContentsMargins(0,0,0,0);
}

ThumbnailWidget* FolderGridView::createThumbnailWidget() {
    ThumbnailWidget *widget = new ThumbnailWidget();
    widget->setPadding(8);
    ThumbnailStyle style = THUMB_NORMAL;
    widget->setThumbStyle(style);
    widget->setThumbnailSize(this->mThumbnailSize);
    return widget;
}

void FolderGridView::addItemToLayout(ThumbnailWidget* widget, int pos) {
    scene.addItem(widget);
    flowLayout->insertItem(pos, widget);
}

void FolderGridView::removeItemFromLayout(int pos) {
    flowLayout->removeAt(pos);
}

void FolderGridView::removeAll() {
    flowLayout->clear();
    qDeleteAll(thumbnails);
    thumbnails.clear();
}

void FolderGridView::updateLayout() {
    shiftedCol = -1;
    flowLayout->invalidate();
    flowLayout->activate();
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
    mThumbnailSize = newSize;
    for(int i = 0; i < thumbnails.count(); i++) {
        thumbnails.at(i)->setThumbnailSize(newSize);
    }
    updateLayout();
    fitSceneToContents();
    if(lastSelected() != -1)
        ensureVisible(thumbnails.at(lastSelected()), 0, 40);
    emit thumbnailSizeChanged(mThumbnailSize);
    loadVisibleThumbnailsDelayed();
}

void FolderGridView::fitSceneToContents() {
    if(scrollBar->isVisible())
        holderWidget.setGeometry(0,0, width() - scrollBar->width(), height());
    else
        holderWidget.setGeometry(0,0, width(), height());
    ThumbnailView::fitSceneToContents();
}

void FolderGridView::resizeEvent(QResizeEvent *event) {
    if(this->isVisible()) {
        ThumbnailView::resizeEvent(event);
        fitSceneToContents();
        //focusOn(selectedIndex());
        loadVisibleThumbnailsDelayed();
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
            " padding: 4px 0px;"
            "}"
            "QMenu::separator {"
            " height: 1px;"
            " background-color: %2;"
            " margin: 4px 11px;"
            "}";
        auto scheme = settings->colorScheme();
        stylesheet = stylesheet.arg(
            scheme.widget.name(),
            scheme.widget_border.name());
        menu.setStyleSheet(stylesheet);

        bool hasSelection = !selection().isEmpty();

        auto addCustomAction = [&](const QString &text, FluentIcon icon, const QString &shortcut = "") {
            QWidgetAction *wa = new QWidgetAction(&menu);
            ContextMenuItem *item = new ContextMenuItem(this);
            item->setText(text);
            item->setIcon(icon, kContextMenuIconSizePx);
            item->setMinimumWidth(212);
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

        ContextMenuItem *itemAddFolder = addCustomAction(tr("Add folder"), FluentIcon::FolderAdd, actionManager->shortcutForAction("createDirectory"));
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

        ContextMenuItem *itemTrash = addCustomAction(tr("Move to trash"), FluentIcon::Delete16);
        itemTrash->setTextColor(settings->colorScheme().trash);
        itemTrash->setIconColor(settings->colorScheme().trash);
        itemTrash->setEnabled(hasSelection);
        connect(itemTrash, &ContextMenuItem::pressed, this, [this, &menu]() {
            menu.close();
            actionManager->invokeAction("moveToTrash");
        });

        ContextMenuItem *itemDelete = addCustomAction(tr("Delete permanently"), FluentIcon::Dismiss20);
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
