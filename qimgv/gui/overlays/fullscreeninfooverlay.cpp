#include "fullscreeninfooverlay.h"
#include <QHBoxLayout>
#include <QLabel>

FullscreenInfoOverlay::FullscreenInfoOverlay(FloatingWidgetContainer *parent) :
    OverlayWidget(parent)
{
    setupUi();
    setPosition(FloatingWidgetPosition::TOPLEFT);
    this->setHorizontalMargin(0);
    this->setVerticalMargin(0);
    nameLabel->setText("No file opened");
    if(parent)
        setContainerSize(parent->size());
}

FullscreenInfoOverlay::~FullscreenInfoOverlay() = default;

void FullscreenInfoOverlay::setupUi() {
    this->setEnabled(false);
    this->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    this->setMaximumSize(600, 40);
    this->setAccessibleName("FullscreenInfoOverlay");

    QHBoxLayout *horizontalLayout = new QHBoxLayout(this);
    horizontalLayout->setSpacing(12);
    horizontalLayout->setContentsMargins(9, 5, 9, 5);
    horizontalLayout->setSizeConstraint(QLayout::SetDefaultConstraint);

    posLabel = new QLabel(this);
    horizontalLayout->addWidget(posLabel);

    nameLabel = new QLabel(this);
    horizontalLayout->addWidget(nameLabel);

    infoLabel = new QLabel(this);
    horizontalLayout->addWidget(infoLabel);
}

void FullscreenInfoOverlay::setInfo(QString pos, QString fileName, QString info) {
    posLabel->setText(pos);
    nameLabel->setText(fileName);
    infoLabel->setText(info);
    this->adjustSize();
}
