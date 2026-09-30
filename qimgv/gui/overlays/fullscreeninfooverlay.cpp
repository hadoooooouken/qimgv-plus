#include "fullscreeninfooverlay.h"
#include <QHBoxLayout>
#include <QLabel>

namespace {
constexpr int kInfoHideTimeoutMs = 2000;
} // namespace

FullscreenInfoOverlay::FullscreenInfoOverlay(FloatingWidgetContainer *parent) :
    OverlayWidget(parent)
{
    setupUi();
    setPosition(FloatingWidgetPosition::TOPLEFT);
    this->setHorizontalMargin(0);
    this->setVerticalMargin(0);
    nameLabel->setText("No file opened");
    setFadeEnabled(true);
    setFadeDuration(230);
    hideTimer.setSingleShot(true);
    connect(&hideTimer, &QTimer::timeout, this, &FullscreenInfoOverlay::onHideTimeout);
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

void FullscreenInfoOverlay::show() {
    mActive = true;
    hideTimer.stop();
    OverlayWidget::show();
    hideTimer.start(kInfoHideTimeoutMs);
}

void FullscreenInfoOverlay::hide() {
    mActive = false;
    hideTimer.stop();
    OverlayWidget::hide();
}

void FullscreenInfoOverlay::onPointerMoved() {
    if (!mActive)
        return;

    if (isHidden()) {
        OverlayWidget::show();
    }

    hideTimer.start(kInfoHideTimeoutMs);
}

void FullscreenInfoOverlay::onHideTimeout() {
    if (!mActive)
        return;
    hideAnimated();
}
