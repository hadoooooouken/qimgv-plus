#include "ssidebar.h"
#include "settings.h"
#include <utility>

namespace {
constexpr int kSidebarIconSizePx = 32;
}

SSideBar::SSideBar(QWidget *parent) : QWidget{parent} {
    layout = new QBoxLayout(QBoxLayout::TopToBottom);
    layout->setSpacing(0);
    layout->setContentsMargins(8,8,9,9);
    layout->addStretch();
    setLayout(layout);
    addEntry(FluentIcon::Settings32,          tr("General"));
    addEntry(FluentIcon::Eye32,               tr("View"));
    addEntry(FluentIcon::ColorFill32,         tr("Theme"));
    addEntry(FluentIcon::Controls32,               tr("Controls"));
    addEntry(FluentIcon::Scripts32,         tr("Scripts"));
    addEntry(FluentIcon::WrenchScrewdriver32, tr("Advanced"));
    addEntry(FluentIcon::BrainSparkle32,      tr("AI Upscale"));
    addEntry(FluentIcon::Info32,              tr("About"));
}

void SSideBar::addEntry(FluentIcon icon, const QString &name) {
    SSideBarItem *entry = new SSideBarItem(icon, name);
    layout->insertWidget(entries.count(), entry);
    entries.append(entry);
    if(entries.count() == 1)
        selectEntry(0);
}

void SSideBar::selectEntry(int idx) {
    if(idx >= 0 && idx < entries.count()) {
        for(SSideBarItem *entry : std::as_const(entries))
            entry->setHighlighted(false);
        entries[idx]->setHighlighted(true);
        emit entrySelected(idx);
    }
}

void SSideBar::mousePressEvent(QMouseEvent *event) {
    event->accept();
    if(!(event->buttons() & Qt::LeftButton))
        return;
    selectEntryAt(event->pos());

}

void SSideBar::mouseMoveEvent(QMouseEvent *event) {
    event->accept();
    if(!(event->buttons() & Qt::LeftButton))
        return;
    selectEntryAt(event->pos());
}

void SSideBar::selectEntryAt(QPoint pos) {
    int newSelection = -1;
    for(int i = 0; i < entries.count(); i++) {
        if(entries[i]->geometry().contains(pos)) {
            if(!entries[i]->highlighted())
                newSelection = i;
            break;
        }
    }
    selectEntry(newSelection);
}

void SSideBar::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

// -------------------------------------------------------------------

SSideBarItem::SSideBarItem(FluentIcon icon, const QString &name, QWidget *parent) : QWidget{parent} {
    QPalette p;
    if(p.base().color().valueF() <= 0.45f)
        iconWidget.setColor(QColor(184,184,185));
    else
        iconWidget.setColor(QColor(70,70,70));
    iconWidget.setIcon(icon, kSidebarIconSizePx);
    textLabel.setText(name);
    layout = new QBoxLayout(QBoxLayout::LeftToRight);
    layout->setContentsMargins(6,4,6,4);
    layout->setSpacing(7);
    layout->addWidget(&iconWidget);
    layout->addWidget(&textLabel);
    layout->addStretch();
    setLayout(layout);
    setMouseTracking(true);
}

void SSideBarItem::setHighlighted(bool mode) {
    mHighlighted = mode;
    this->setProperty("checked", mHighlighted);
    style()->unpolish(this);
    style()->polish(this);
}

bool SSideBarItem::highlighted() {
    return mHighlighted;
}

void SSideBarItem::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

void SSideBarItem::changeEvent(QEvent *event) {
    if (event->type() == QEvent::PaletteChange) {
        QPalette p = palette();
        if (p.base().color().valueF() <= 0.45f)
            iconWidget.setColor(QColor(184,184,185));
        else
            iconWidget.setColor(QColor(70,70,70));
    }
    QWidget::changeEvent(event);
}
