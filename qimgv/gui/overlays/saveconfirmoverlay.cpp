#include "saveconfirmoverlay.h"
#include "settings.h"
#include "gui/customwidgets/iconwidget.h"
#include <QShowEvent>
#include "gui/customwidgets/iconbutton.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSpacerItem>

SaveConfirmOverlay::SaveConfirmOverlay(FloatingWidgetContainer *parent) :
    OverlayWidget(parent)
{
    setupUi();

    auto *saveButton    = findChild<QPushButton*>("saveButton");
    auto *saveAsButton  = findChild<QPushButton*>("saveAsButton");
    auto *discardButton = findChild<QPushButton*>("discardButton");

    connect(saveButton,    &QPushButton::clicked, this, &SaveConfirmOverlay::saveClicked);
    connect(saveAsButton,  &QPushButton::clicked, this, &SaveConfirmOverlay::saveAsClicked);
    connect(discardButton, &QPushButton::clicked, this, &SaveConfirmOverlay::discardClicked);
    this->setFocusPolicy(Qt::NoFocus);
    closeButton->setIcon(FluentIcon::Dismiss16, kCloseIconSizePx);
    headerIcon->setIcon(FluentIcon::Rename20, kHeaderIconSizePx);
    readSettings();
    connect(settings, &Settings::settingsChanged, this, &SaveConfirmOverlay::readSettings);

    if(parent)
        setContainerSize(parent->size());

    this->hide();
}

void SaveConfirmOverlay::setupUi() {
    QVBoxLayout *verticalLayout = new QVBoxLayout(this);
    verticalLayout->setSpacing(10);
    verticalLayout->setContentsMargins(0, 0, 0, 0);
    verticalLayout->setSizeConstraint(QLayout::SetMinAndMaxSize);

    // --- header ---
    QWidget *header = new QWidget(this);
    header->setAccessibleName("OverlayHeaderWidget");

    QHBoxLayout *headerLayout = new QHBoxLayout(header);
    headerLayout->setSpacing(0);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSizeConstraint(QLayout::SetMinimumSize);

    headerIcon = new IconWidget(header);
    headerIcon->setAccessibleName("OverlayHeaderIcon");
    headerLayout->addWidget(headerIcon);

    QLabel *label = new QLabel(tr("Unsaved edits"), header);
    QSizePolicy labelPolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    labelPolicy.setHorizontalStretch(1);
    label->setSizePolicy(labelPolicy);
    label->setAccessibleName("OverlayHeaderLabel");
    label->setTextInteractionFlags(Qt::NoTextInteraction);
    headerLayout->addWidget(label);

    closeButton = new IconButton(header);
    closeButton->setAccessibleName("OverlayHeaderButton");
    headerLayout->addWidget(closeButton);
    connect(closeButton, &IconButton::clicked, this, &SaveConfirmOverlay::hide);

    verticalLayout->addWidget(header);

    // --- buttons row ---
    QHBoxLayout *buttonsLayout = new QHBoxLayout();
    buttonsLayout->setSpacing(0);
    buttonsLayout->setContentsMargins(9, 0, 9, 9);
    buttonsLayout->setSizeConstraint(QLayout::SetMinimumSize);

    QPushButton *saveButton = new QPushButton(tr("Save"), this);
    saveButton->setObjectName("saveButton");
    saveButton->setFocusPolicy(Qt::NoFocus);
    saveButton->setAccessibleName("ButtonSetLeft");
    buttonsLayout->addWidget(saveButton);

    QPushButton *saveAsButton = new QPushButton(tr("Save as"), this);
    saveAsButton->setObjectName("saveAsButton");
    saveAsButton->setFocusPolicy(Qt::NoFocus);
    saveAsButton->setAccessibleName("ButtonSetRight");
    buttonsLayout->addWidget(saveAsButton);

    buttonsLayout->addSpacerItem(new QSpacerItem(8, 10, QSizePolicy::Fixed, QSizePolicy::Minimum));

    QPushButton *discardButton = new QPushButton(tr("Discard"), this);
    discardButton->setObjectName("discardButton");
    discardButton->setFocusPolicy(Qt::NoFocus);
    discardButton->setAccessibleName("Button");
    buttonsLayout->addWidget(discardButton);

    verticalLayout->addLayout(buttonsLayout);
}

void SaveConfirmOverlay::readSettings() {
    // don't interfere with the main panel
    if (settings->panelEnabled() &&
        (settings->panelPosition() == PanelPosition::PANEL_BOTTOM ||
         settings->panelPosition() == PanelPosition::PANEL_LEFT ||
         settings->panelPosition() == PanelPosition::PANEL_RIGHT)) {
        setPosition(FloatingWidgetPosition::TOP);
        setVerticalMargin(35);
    } else {
        setPosition(FloatingWidgetPosition::BOTTOM);
        setVerticalMargin(80); // offset to avoid taskbar
    }
    update();
}

void SaveConfirmOverlay::showEvent(QShowEvent *event) {
    OverlayWidget::showEvent(event);
    ensurePolished();
    recalculateGeometry();
}

SaveConfirmOverlay::~SaveConfirmOverlay() = default;
