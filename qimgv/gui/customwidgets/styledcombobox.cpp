#include "styledcombobox.h"

#include <QAbstractItemView>
#include <QDebug>

#include "proxystyle.h"
#include "settings.h"

namespace {
constexpr auto kContextMenuPopupProperty = "qimgvContextMenuPopup";
constexpr auto kContextMenuPopupStyleSheet =
    "[qimgvContextMenuPopup=\"true\"] {"
    " background-color: transparent;"
    " border: none;"
    "}";
} // namespace

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

void StyledComboBox::setContextMenuPopupStyle(bool enabled) {
    contextMenuPopupStyle = enabled;
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

void StyledComboBox::showPopup() {
    if (contextMenuPopupStyle) {
        QAbstractItemView *popupView = view();
        QWidget *popupWindow = popupView ? popupView->window() : nullptr;
        if (!popupWindow) {
            qWarning() << "StyledComboBox could not prepare its popup window";
        } else {
            popupWindow->setWindowFlags(
                popupWindow->windowFlags()
                | Qt::FramelessWindowHint
                | Qt::NoDropShadowWindowHint);
            popupWindow->setAttribute(Qt::WA_TranslucentBackground, true);
            popupWindow->setProperty(kContextMenuPopupProperty, true);
            popupWindow->setStyleSheet(
                QString::fromLatin1(kContextMenuPopupStyleSheet));
        }
    }

    QComboBox::showPopup();
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
