#include "sidepanel.h"

SidePanel::SidePanel(QWidget *parent) :
    QWidget(parent),
    mWidget(nullptr)
{
    setupUi();
    this->setObjectName("SidePanel");
    this->hide();
}

SidePanel::~SidePanel() = default;

void SidePanel::setupUi() {
    this->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);

    QVBoxLayout *verticalLayout_2 = new QVBoxLayout(this);
    verticalLayout_2->setSpacing(0);
    verticalLayout_2->setContentsMargins(0, 0, 0, 0);
    verticalLayout_2->setSizeConstraint(QLayout::SetMinAndMaxSize);

    layout = new QVBoxLayout();
    layout->setContentsMargins(0, 0, 0, 0);
    verticalLayout_2->addLayout(layout);
}

void SidePanel::setWidget(SidePanelWidget* w) {
    if(mWidget) {
        mWidget->hide();
        layout->removeWidget(mWidget);
    }
    mWidget = w;
    layout->addWidget(w);
    w->show();
}

SidePanelWidget* SidePanel::widget() {
    return mWidget;
}

void SidePanel::show() {
    QWidget::show();
    if(mWidget)
        mWidget->show();
}

void SidePanel::hide() {
    if(mWidget) {
        mWidget->hide();
    }
    QWidget::hide();
}

void SidePanel::paintEvent(QPaintEvent *) {
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}
