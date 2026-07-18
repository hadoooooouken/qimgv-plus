#include "menuitem.h"

MenuItem::MenuItem(QWidget *parent)
    : QWidget(parent)
{
    mLayout.setContentsMargins(6,0,8,0);
    mLayout.setSpacing(2);

    setAccessibleName("MenuItem");
    this->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    mTextLabel.setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    mShortcutLabel.setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    // Default box for the default 16px icon (16 + 5px padding * 2 = 26).
    // Grows automatically in setIcon() if a larger sizePx is requested.
    mIconWidget.setMinimumSize(IconWidget::kMenuItemIconSizePx + 2 * kIconPaddingPx,
                                IconWidget::kMenuItemIconSizePx + 2 * kIconPaddingPx);
    mIconWidget.installEventFilter(this);
    spacer = new QSpacerItem(3, 1, QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    mIconWidget.setAttribute(Qt::WA_TransparentForMouseEvents, true);
    mIconWidget.setAccessibleName("MenuItemIcon");
    mTextLabel.setAccessibleName("MenuItemText");
    mShortcutLabel.setAccessibleName("MenuItemShortcutLabel");
    mLayout.addWidget(&mIconWidget);
    mLayout.addWidget(&mTextLabel);
    mLayout.addSpacerItem(spacer);
    mLayout.addWidget(&mShortcutLabel);
    mLayout.setStretch(1,1);

    setLayout(&mLayout);
}

void MenuItem::setText(QString text) {
    this->mTextLabel.setText(text);
}

QString MenuItem::text() {
    return mTextLabel.text();
}

void MenuItem::setShortcutText(QString text) {
    this->mShortcutLabel.setText(text);
    this->adjustSize();
}

QString MenuItem::shortcut() {
    return mShortcutLabel.text();
}

void MenuItem::setIconPath(QString path) {
    mIconWidget.setIconPath(path);
}

void MenuItem::setIcon(FluentIcon icon, int sizePx) {
    mIconWidget.setIcon(icon, sizePx);
    int box = sizePx + 2 * kIconPaddingPx;
    mIconWidget.setMinimumSize(box, box);
}

void MenuItem::setPassthroughClicks(bool mode) {
    passthroughClicks = mode;
}

void MenuItem::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

void MenuItem::onPress() {
}

void MenuItem::onClick() {
}

void MenuItem::mousePressEvent(QMouseEvent *event) {
    onPress();
    QWidget::mousePressEvent(event);
    if(!passthroughClicks)
        event->accept();
}

void MenuItem::mouseReleaseEvent(QMouseEvent *event) {
    onClick();
    QWidget::mouseReleaseEvent(event);
    if(!passthroughClicks)
        event->accept();
}

void MenuItem::setTextColor(QColor color) {
    mTextLabel.setStyleSheet(QString("color: %1;").arg(color.name()));
}

void MenuItem::setIconColor(QColor color) {
    mIconWidget.setColor(color);
}
