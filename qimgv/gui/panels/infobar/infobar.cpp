#include "infobar.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QSpacerItem>

InfoBar::InfoBar(QWidget *parent) :
    QWidget(parent)
{
    setupUi();
    path->setText("No file opened.");
}

InfoBar::~InfoBar() = default;

void InfoBar::setupUi() {
    this->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    this->setAccessibleName("InfoBar");

    QHBoxLayout *horizontalLayout = new QHBoxLayout(this);
    horizontalLayout->setSpacing(11);
    horizontalLayout->setContentsMargins(10, 0, 10, 1);

    index = new QLabel(this);
    horizontalLayout->addWidget(index);

    path = new QLabel(this);
    horizontalLayout->addWidget(path);

    horizontalLayout->addSpacerItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));

    info = new QLabel(this);
    info->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);
    horizontalLayout->addWidget(info);
}

void InfoBar::setInfo(QString position, QString fileName, QString fileInfo) {
    index->setText(position);
    path->setText(fileName);
    info->setText(fileInfo);
}

void InfoBar::wheelEvent(QWheelEvent *event) {
    event->accept();
}

void InfoBar::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}
