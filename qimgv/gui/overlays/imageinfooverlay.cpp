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
    closeButton->setIconPath(":res/icons/common/overlay/close-dim16.png");
    headerIcon->setIconPath(":res/icons/common/overlay/info16.png");
    entryStub.setFixedSize(280, 48);
    entryStub.setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    connect(closeButton,  &IconButton::clicked, this, &ImageInfoOverlay::hide);
    this->setPosition(FloatingWidgetPosition::RIGHT);

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

    QLabel *label = new QLabel(tr("EXIF Tags"), header);
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
    entryLayout->setSizeConstraint(QLayout::SetFixedSize);
    verticalLayout->addLayout(entryLayout);
}

void ImageInfoOverlay::setExifInfo(QMap<QString, QString> info) {
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
    QMap<QString, QString>::const_iterator i = info.constBegin();
    int entryIdx = 0;
    while(i != info.constEnd()) {
        entries.at(entryIdx)->setInfo(i.key(), i.value());
        ++i;
        ++entryIdx;
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
    OverlayWidget::show();
    adjustSize();
    recalculateGeometry();
}

void ImageInfoOverlay::wheelEvent(QWheelEvent *event) {
    event->accept();
}
