#include "thumbnailstripproxy.h"
#include "gui/customwidgets/slidepanel.h"

ThumbnailStripProxy::ThumbnailStripProxy(QWidget *parent)
    : QWidget(parent)
{
    layout.setContentsMargins(0,0,0,0);
}

void ThumbnailStripProxy::init() {
    initEager();
    applyBufferedState();
}

// Lightweight construction — creates the ThumbnailStrip widget and wires up
// signals, but does NOT populate or apply buffered state. Safe to call while
// the panel is still hidden so the heavy QGraphicsView/Scene allocation
// happens outside the animation path.
void ThumbnailStripProxy::initEager() {
    if(thumbnailStrip)
        return;
    QMutexLocker ml(&m);
    thumbnailStrip.reset(new ThumbnailStrip());
    thumbnailStrip->setParent(this);
    ml.unlock();
    layout.addWidget(thumbnailStrip.get());
    this->setFocusProxy(thumbnailStrip.get());
    this->setLayout(&layout);

    connect(thumbnailStrip.get(), &ThumbnailStrip::itemActivated, this, &ThumbnailStripProxy::itemActivated);
    connect(thumbnailStrip.get(), &ThumbnailStrip::thumbnailsRequested, this, &ThumbnailStripProxy::thumbnailsRequested);
    connect(thumbnailStrip.get(), &ThumbnailStrip::backRequested, this, &ThumbnailStripProxy::backRequested);
    connect(thumbnailStrip.get(), &ThumbnailStrip::forwardRequested, this, &ThumbnailStripProxy::forwardRequested);

    thumbnailStrip->show();
    mNeedsBufferApply = true;

    QWidget *parent = parentWidget();
    SlidePanel *slidePanel = nullptr;
    while (parent) {
        slidePanel = qobject_cast<SlidePanel*>(parent);
        if (slidePanel) {
            break;
        }
        parent = parent->parentWidget();
    }
    if (slidePanel) {
        connect(slidePanel, &SlidePanel::animationStarted, this, &ThumbnailStripProxy::onAnimationStarted);
        connect(slidePanel, &SlidePanel::animationFinished, this, &ThumbnailStripProxy::onAnimationFinished);
    }

    if(stateBuf.itemCount > 0) {
        applyBufferedState();
    }
}

// Applies the buffered state (populate, select, focus) to the real strip.
// This is the expensive part that was previously blocking the animation.
void ThumbnailStripProxy::applyBufferedState() {
    if(!mNeedsBufferApply || !thumbnailStrip)
        return;
    mNeedsBufferApply = false;
    thumbnailStrip->populate(stateBuf.itemCount);
    thumbnailStrip->select(stateBuf.selection);
    thumbnailStrip->focusOnSelection();
}

bool ThumbnailStripProxy::isInitialized() {
    return (thumbnailStrip != nullptr);
}

void ThumbnailStripProxy::populate(int count) {
    QMutexLocker ml(&m);
    stateBuf.itemCount = count;
    if(thumbnailStrip) {
        mNeedsBufferApply = false;
        ml.unlock();
        thumbnailStrip->populate(stateBuf.itemCount);
    } else {
        stateBuf.selection.clear();
    }
}

void ThumbnailStripProxy::setThumbnail(int pos, std::shared_ptr<Thumbnail> thumb) {
    if(thumbnailStrip) {
        thumbnailStrip->setThumbnail(pos, thumb);
    }
}

void ThumbnailStripProxy::setThumbnailUnavailable(int pos, int size) {
    if(thumbnailStrip)
        thumbnailStrip->setThumbnailUnavailable(pos, size);
}

void ThumbnailStripProxy::select(QList<int> indices) {
    if(thumbnailStrip) {
        thumbnailStrip->select(indices);
    } else {
        stateBuf.selection = indices;
    }
}

void ThumbnailStripProxy::select(int index) {
    if(thumbnailStrip) {
        thumbnailStrip->select(index);
    } else {
        stateBuf.selection.clear();
        stateBuf.selection << index;
    }
}

QList<int> ThumbnailStripProxy::selection() {
    if(thumbnailStrip) {
        return thumbnailStrip->selection();
    } else {
        return stateBuf.selection;
    }
}

void ThumbnailStripProxy::focusOn(int index) {
    if(thumbnailStrip) {
        thumbnailStrip->focusOn(index);
    }
}

void ThumbnailStripProxy::focusOnSelection() {
    if(thumbnailStrip) {
        thumbnailStrip->focusOnSelection();
    }
}

void ThumbnailStripProxy::insertItem(int index) {
    if(thumbnailStrip) {
        thumbnailStrip->insertItem(index);
    } else {
        stateBuf.itemCount++;
    }
}

void ThumbnailStripProxy::removeItem(int index) {
    if(thumbnailStrip) {
        thumbnailStrip->removeItem(index);
    } else {
        stateBuf.itemCount--;
        stateBuf.selection.removeAll(index);
        for(int i=0; i < stateBuf.selection.count(); i++) {
            if(stateBuf.selection[i] > index)
                stateBuf.selection[i]--;
        }
        if(!stateBuf.selection.count())
            stateBuf.selection << ((index >= stateBuf.itemCount) ? stateBuf.itemCount - 1 : index);
    }
}

void ThumbnailStripProxy::reloadItem(int index) {
    if(thumbnailStrip)
        thumbnailStrip->reloadItem(index);
}

void ThumbnailStripProxy::setDragHover(int index) {
    if(thumbnailStrip)
        thumbnailStrip->setDragHover(index);
}

void ThumbnailStripProxy::setDirectoryPath(QString path) {
    Q_UNUSED(path)
}

void ThumbnailStripProxy::addItem() {
    if(thumbnailStrip) {
        thumbnailStrip->addItem();
    } else {
        stateBuf.itemCount++;
    }
}

QSize ThumbnailStripProxy::itemSize() {
    return thumbnailStrip->itemSize();
}

void ThumbnailStripProxy::readSettings() {
    if(thumbnailStrip)
        thumbnailStrip->readSettings();
}

void ThumbnailStripProxy::showEvent(QShowEvent *event) {
    if(!thumbnailStrip)
        initEager(); // lightweight construction only
    if(mNeedsBufferApply) {
        QTimer::singleShot(0, this, &ThumbnailStripProxy::applyBufferedState);
    } else {
        thumbnailStrip->updateGeometry();
        thumbnailStrip->focusOnSelection();
    }
    QWidget::showEvent(event);
}

void ThumbnailStripProxy::onAnimationStarted() {
    if (thumbnailStrip) {
        thumbnailStrip->setBlockThumbnailLoading(true);
    }
}

void ThumbnailStripProxy::onAnimationFinished() {
    if (thumbnailStrip) {
        thumbnailStrip->setBlockThumbnailLoading(false);
    }
}
