#include "entryinfoitem.h"

EntryInfoItem::EntryInfoItem(QWidget *parent) : QWidget(parent) {
    outerLayout.setContentsMargins(9,0,9,0);
    outerLayout.setSpacing(2);
    setLayout(&outerLayout);

    headerLayout.setContentsMargins(0,0,0,0);
    headerLayout.setSpacing(0);
    outerLayout.addLayout(&headerLayout);

    headerLayout.addWidget(&nameLabel);
    nameLabel.setFixedSize(120,30);
    nameLabel.setAlignment(Qt::AlignTop | Qt::AlignLeft);
    // AlignLeft must be explicit here: once valueLabel is pulled out of
    // headerLayout in stacked mode, nameLabel becomes the only item in a
    // row that's wider than it is, and without a horizontal alignment flag
    // Qt centers the lone widget in the leftover space instead of pinning
    // it left.
    headerLayout.setAlignment(&nameLabel, Qt::AlignTop | Qt::AlignLeft);

    // width stays fixed to match the overlay's column layout in the default
    // (inline) mode; height grows with wrapped content instead of silently
    // clipping long values (prompts can be hundreds of characters long)
    valueLabel.setFixedWidth(142);
    valueLabel.setMinimumHeight(30);
    valueLabel.setWordWrap(true);
    valueLabel.setAlignment(Qt::AlignTop | Qt::AlignLeft);

    // add some padding for easier text selection
    valueLabel.setContentsMargins(3,0,0,0);
    valueLabel.setTextInteractionFlags(Qt::TextSelectableByMouse);
    valueLabel.setCursor(Qt::IBeamCursor);

    // default (non-stacked) placement: value beside name in the header row
    headerLayout.addWidget(&valueLabel);
    headerLayout.setAlignment(&valueLabel, Qt::AlignTop | Qt::AlignLeft);
}

void EntryInfoItem::setInfo(QString _name, QString _value, bool stacked) {
    name = _name;
    value = _value;
    nameLabel.setText(name);
    valueLabel.setText(value);

    if (stacked != isStacked) {
        applyLayoutMode(stacked);
        isStacked = stacked;
    }
};

void EntryInfoItem::applyLayoutMode(bool stacked) {
    // detach valueLabel from whichever layout currently owns it
    if (isStacked) {
        outerLayout.removeWidget(&valueLabel);
    } else {
        headerLayout.removeWidget(&valueLabel);
    }

    if (stacked) {
        // name on its own line; value wraps at the full panel width below
        // it. Width is still fixed (not unconstrained) - see the comment
        // on STACKED_VALUE_WIDTH for why.
        valueLabel.setFixedWidth(STACKED_VALUE_WIDTH);
        outerLayout.addWidget(&valueLabel);
    } else {
        valueLabel.setFixedWidth(142);
        headerLayout.addWidget(&valueLabel);
        headerLayout.setAlignment(&valueLabel, Qt::AlignTop | Qt::AlignLeft);
    }
}

void EntryInfoItem::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}
