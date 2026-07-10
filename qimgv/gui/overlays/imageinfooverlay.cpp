#include "imageinfooverlay.h"
#include "gui/customwidgets/iconwidget.h"
#include "gui/customwidgets/iconbutton.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>

ImageInfoOverlay::ImageInfoOverlay(FloatingWidgetContainer *parent) :
    OverlayWidget(parent)
{
    setupUi();
    closeButton->setIconPath(":/res/icons/common/overlay/close-dim16.png");
    headerIcon->setIconPath(":/res/icons/common/overlay/info16.png");
    entryStub.setFixedSize(280, 48);
    entryStub.setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    connect(closeButton,  &IconButton::clicked, this, &ImageInfoOverlay::hide);
    this->setPosition(FloatingWidgetPosition::RIGHT);
    setMouseTracking(true);

    if(parent)
        setContainerSize(parent->size());
}

ImageInfoOverlay::~ImageInfoOverlay() {
    for(auto i = entries.count() - 1; i >= 0; i--)
        delete entries.takeAt(i);
}

void ImageInfoOverlay::setupUi() {
    this->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::MinimumExpanding);
    this->setMinimumSize(280, 0);
    this->setMaximumSize(280, 16777215);

    QVBoxLayout *verticalLayout = new QVBoxLayout(this);
    verticalLayout->setSpacing(0);
    verticalLayout->setContentsMargins(0, 0, 0, 4);
    verticalLayout->setSizeConstraint(QLayout::SetFixedSize);

    // --- header ---
    QWidget *header = new QWidget(this);
    header->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    header->setAccessibleName("OverlayHeaderWidget");

    QHBoxLayout *horizontalLayout = new QHBoxLayout(header);
    horizontalLayout->setSpacing(0);
    horizontalLayout->setContentsMargins(0, 0, 0, 0);

    headerIcon = new IconWidget(header);
    headerIcon->setAccessibleName("OverlayHeaderIcon");
    horizontalLayout->addWidget(headerIcon);

    QLabel *label = new QLabel(tr("Image info"), header);
    QSizePolicy labelPolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    labelPolicy.setHorizontalStretch(1);
    label->setSizePolicy(labelPolicy);
    horizontalLayout->addWidget(label);

    closeButton = new IconButton(header);
    closeButton->setAccessibleName("OverlayHeaderButton");
    horizontalLayout->addWidget(closeButton);

    verticalLayout->addWidget(header);

    // --- entry layout ---
    entryLayout = new QVBoxLayout();
    entryLayout->setSpacing(0);
    entryLayout->setContentsMargins(1, 2, 1, 2);
    entryLayout->setSizeConstraint(QLayout::SetFixedSize);
    verticalLayout->addLayout(entryLayout);
}

void ImageInfoOverlay::setExifInfo(QList<QPair<QString, QString>> info) {
    // remove/add entries
    int entryCount = entries.count();
    if(entryCount > info.count()) {
        for(auto i = entryCount - 1; i >= info.count(); i--) {
            entryLayout->removeWidget(entries.last());
            delete entries.takeLast();
        }
    } else if(entryCount < info.count()) {
        for(auto i = entryCount; i < info.count(); i++) {
            entries.append(new EntryInfoItem(this));
            entryLayout->addWidget(entries.last());
        }
    }
    // Iterate in list order (as opposed to a QMap, which would always be
    // sorted by key) so the display order set upstream is preserved.
    for(int entryIdx = 0; entryIdx < info.count(); entryIdx++) {
        entries.at(entryIdx)->setInfo(info.at(entryIdx).first, info.at(entryIdx).second);
    }

    // Hiding/showing entryStub causes flicker,
    // so we just remove it from layout and clear the text.
    // It's still there but basically not visible
    if(entries.count()) {
        entryLayout->removeWidget(&entryStub);
        entryStub.setText("");
    } else {
        entryLayout->addWidget(&entryStub);
        entryStub.setText("<no metadata found>");
    }

    if(!isHidden() && entryCount != info.count()) {
        // wait for layout change
        qApp->processEvents();
        // reposition
        recalculateGeometry();
    }
}

void ImageInfoOverlay::show() {
    mManuallyPositioned = false;
    OverlayWidget::show();
    adjustSize();
    recalculateGeometry();
}

void ImageInfoOverlay::wheelEvent(QWheelEvent *event) {
    event->accept();
}

void ImageInfoOverlay::mousePressEvent(QMouseEvent *event) {
    // allow drag only from the header area (top ~30px)
    if(event->button() == Qt::LeftButton && event->pos().y() < 30) {
        mDragging = true;
        mDragStartPos = event->globalPosition().toPoint();
        mDragStartWidgetPos = this->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
    } else {
        FloatingWidget::mousePressEvent(event);
    }
}

void ImageInfoOverlay::mouseMoveEvent(QMouseEvent *event) {
    if(mDragging) {
        QPoint delta = event->globalPosition().toPoint() - mDragStartPos;
        QPoint newPos = mDragStartWidgetPos + delta;
        // clamp within parent bounds
        QSize cs = containerSize();
        newPos.setX(qBound(0, newPos.x(), cs.width() - width()));
        newPos.setY(qBound(0, newPos.y(), cs.height() - height()));
        move(newPos);
        mManuallyPositioned = true;
        event->accept();
    } else {
        // show open hand cursor when hovering the header area
        if(event->pos().y() < 30)
            setCursor(Qt::OpenHandCursor);
        else
            setCursor(Qt::ArrowCursor);
        event->accept();
    }
}

void ImageInfoOverlay::mouseReleaseEvent(QMouseEvent *event) {
    if(mDragging) {
        mDragging = false;
        setCursor(Qt::ArrowCursor);
        event->accept();
    } else {
        FloatingWidget::mouseReleaseEvent(event);
    }
}

void ImageInfoOverlay::recalculateGeometry() {
    if(mManuallyPositioned) {
        // just clamp to container bounds, don't reset position
        QPoint p = pos();
        QSize cs = containerSize();
        p.setX(qBound(0, p.x(), qMax(0, cs.width() - width())));
        p.setY(qBound(0, p.y(), qMax(0, cs.height() - height())));
        move(p);
        return;
    }
    OverlayWidget::recalculateGeometry();
}
