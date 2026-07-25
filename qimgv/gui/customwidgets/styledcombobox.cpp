#include "styledcombobox.h"
#include "proxystyle.h"
#include "settings.h"

StyledComboBox::StyledComboBox(QWidget *parent) : QComboBox(parent)
{
    dpr = this->devicePixelRatioF();
    connect(settings, &Settings::settingsChanged, [this]() {
        refreshIcon();
    });
}

void StyledComboBox::setIcon(FluentIcon icon, int sizePx) {
    setProperty(ProxyStyle::kCustomComboBoxIndicatorProperty, true);
    iconGlyph = icon;
    iconSizePx = sizePx;
    iconSet = true;
    refreshIcon();
    updateGeometry();
}

void StyledComboBox::setIconOffset(const QPoint &offset) {
    if (iconOffset == offset)
        return;

    iconOffset = offset;
    update();
}

int StyledComboBox::iconAreaWidth() const {
    if (downArrow.isNull())
        return kIconRightMargin;

    qreal logicalWidth = downArrow.width() / downArrow.devicePixelRatio();
    return kIconRightMargin + qRound(logicalWidth);
}

QColor StyledComboBox::iconColor() const {
    return applyEnabledState(settings->colorScheme().icons);
}

QColor StyledComboBox::applyEnabledState(const QColor &color) const {
    return ProxyStyle::iconColorForState(color, isEnabled());
}

void StyledComboBox::refreshIcon() {
    if (!iconSet)
        return;
    downArrow = IconFontManager::pixmap(iconGlyph, iconSizePx, iconColor(), dpr);
    update();
}

void StyledComboBox::paintEvent(QPaintEvent *e) {
    QComboBox::paintEvent(e);
    if (downArrow.isNull())
        return;
    QPainter p(this);
    QPointF pos(width() - kIconRightMargin - downArrow.width() / downArrow.devicePixelRatio(),
                height() / 2 - downArrow.height() / (2 * downArrow.devicePixelRatio()));
    pos += QPointF(iconOffset);
    p.drawPixmap(pos, downArrow);
}

void StyledComboBox::keyPressEvent(QKeyEvent *event) {
    event->ignore();
}

bool StyledComboBox::event(QEvent *e) {
    if (e->type() == QEvent::DevicePixelRatioChange) {
        qreal newDpr = devicePixelRatioF();
        if (!qFuzzyCompare(newDpr, dpr)) {
            dpr = newDpr;
            refreshIcon(); // re-render the glyph at the new physical resolution
        }
    } else if (e->type() == QEvent::EnabledChange) {
        refreshIcon();
    }
    return QComboBox::event(e);
}
