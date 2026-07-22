#include "floatingmessage.h"
#include "settings.h"
#include "gui/customwidgets/iconwidget.h"
#include <QHBoxLayout>

namespace {
constexpr int kMessageIconSizePx = 20;
}

FloatingMessage::FloatingMessage(FloatingWidgetContainer *parent) :
    OverlayWidget(parent),
    preferredPosition(FloatingWidgetPosition::BOTTOM)
{
    setupUi();
    hideDelay = 700;

    visibilityTimer.setSingleShot(true);
    visibilityTimer.setInterval(hideDelay);

    setFadeEnabled(true);
    setFadeDuration(300);

    setIcon(FloatingMessageIcon::NO_ICON);

    this->setAccessibleName("FloatingMessage");
    connect(&visibilityTimer, &QTimer::timeout, this, &FloatingMessage::hideAnimated);

    readSettings();

    connect(settings, &Settings::settingsChanged, this, &FloatingMessage::readSettings);

    if(parent)
        setContainerSize(parent->size());
}

FloatingMessage::~FloatingMessage() = default;

void FloatingMessage::setupUi() {
    QHBoxLayout *horizontalLayout = new QHBoxLayout(this);
    horizontalLayout->setSpacing(8);
    horizontalLayout->setContentsMargins(12, 11, 12, 11);
    horizontalLayout->setSizeConstraint(QLayout::SetDefaultConstraint);

    iconLabel = new IconWidget(this);
    iconLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    iconLabel->setMinimumSize(QSize(16, 16));
    iconLabel->setMaximumSize(QSize(24, 24));
    horizontalLayout->addWidget(iconLabel);

    textLabel = new QLabel(this);
    textLabel->setMaximumSize(QSize(16777215, 24));
    horizontalLayout->addWidget(textLabel);
}

void FloatingMessage::readSettings() {
    /*
    // don't interfere with the main panel
    if(settings->panelEnabled() && settings->panelPosition() == PanelHPosition::PANEL_BOTTOM) {
        preferredPosition = FloatingWidgetPosition::TOP;
    } else {
        preferredPosition = FloatingWidgetPosition::BOTTOM;
    }
    */
}

void FloatingMessage::showMessage(QString text, FloatingWidgetPosition position, FloatingMessageIcon icon, int duration) {
    setPosition(position);
    doShowMessage(text, icon, duration);
}

void FloatingMessage::showMessage(QString text, FloatingMessageIcon icon, int duration) {
    setPosition(preferredPosition);
    doShowMessage(text, icon, duration);
}

void FloatingMessage::doShowMessage(QString text, FloatingMessageIcon icon, int duration) {
    hideDelay = duration;
    setIcon(icon);
    setText(text);
    show();
}

void FloatingMessage::setText(QString text) {
    textLabel->setText(text);
    text.isEmpty()?textLabel->hide():textLabel->show();
    recalculateGeometry();
    update();
}

void FloatingMessage::setIcon(FloatingMessageIcon icon) {
    switch (icon) {
        case FloatingMessageIcon::NO_ICON:
        case FloatingMessageIcon::ICON_WARNING:
        case FloatingMessageIcon::ICON_ERROR:
            iconLabel->hide();
            break;
        case FloatingMessageIcon::ICON_DIRECTORY:
            iconLabel->show();
            iconLabel->setIcon(FluentIcon::Folder20, kMessageIconSizePx);
            break;
        case FloatingMessageIcon::ICON_LEFT_EDGE:
            iconLabel->show();
            iconLabel->setIcon(FluentIcon::ArrowPrevious20, kMessageIconSizePx);
            break;
        case FloatingMessageIcon::ICON_RIGHT_EDGE:
            iconLabel->show();
            iconLabel->setIcon(FluentIcon::ArrowNext20, kMessageIconSizePx);
            break;
        case FloatingMessageIcon::ICON_SUCCESS:
            iconLabel->show();
            iconLabel->setIcon(FluentIcon::CheckmarkCircle20, kMessageIconSizePx);
            break;
    }
}

void FloatingMessage::mousePressEvent(QMouseEvent *event) {
    Q_UNUSED (event)
}

// "blink" the widget; show then fade out immediately
void FloatingMessage::show() {
    visibilityTimer.stop();
    OverlayWidget::show();
    // fade out after delay
    visibilityTimer.setInterval(hideDelay);
    visibilityTimer.start();
}
