#include "entryinfoitem.h"

EntryInfoItem::EntryInfoItem(QWidget *parent) : QWidget(parent) {
    layout.setContentsMargins(9,0,9,0);
    layout.setSpacing(0);
    layout.addWidget(&nameLabel);
    layout.addWidget(&valueLabel);
    setLayout(&layout);

    nameLabel.setFixedSize(120,30);
    nameLabel.setAlignment(Qt::AlignTop | Qt::AlignLeft);
    layout.setAlignment(&nameLabel, Qt::AlignTop);

    // width stays fixed to match the overlay's column layout; height grows
    // with wrapped content instead of silently clipping long values (prompts
    // can be hundreds of characters long)
    valueLabel.setFixedWidth(142);
    valueLabel.setMinimumHeight(30);
    valueLabel.setWordWrap(true);
    valueLabel.setAlignment(Qt::AlignTop | Qt::AlignLeft);
    layout.setAlignment(&valueLabel, Qt::AlignTop);

    // add some padding for easier text selection
    valueLabel.setContentsMargins(3,0,0,0);
    valueLabel.setTextInteractionFlags(Qt::TextSelectableByMouse);
    valueLabel.setCursor(Qt::IBeamCursor);
}

void EntryInfoItem::setInfo(QString _name, QString _value) {
    name = _name;
    value = _value;
    nameLabel.setText(name);
    valueLabel.setText(value);
};

void EntryInfoItem::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}
