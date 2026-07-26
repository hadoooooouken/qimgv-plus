#include "proxystyle.h"

#include <QComboBox>
#include <QDebug>
#include <QPainter>
#include <QStyleOption>
#include <QWidget>

#include "utils/iconfontmanager.h"

namespace {

constexpr int kSelectionIndicatorSizePx = 16;
constexpr int kComboBoxChevronSizePx = 12;
constexpr qreal kDisabledIconOpacity = 0.45;
constexpr qreal kComboBoxCornerRadiusPx = 4.0;
constexpr qreal kComboBoxBorderInsetPx = 0.5;
constexpr qreal kComboBoxBorderWidthPx = 1.0;

QColor indicatorColor(const QColor &configuredColor, const QStyleOption *option) {
    QColor color = configuredColor.isValid()
        ? configuredColor
        : option->palette.color(QPalette::Text);
    return ProxyStyle::iconColorForState(
        color, option->state.testFlag(QStyle::State_Enabled));
}

bool drawCenteredIcon(QPainter *painter, const QRect &rect, FluentIcon icon,
                      int sizePx, qreal centerY, const QColor &color, qreal dpr) {
    const QPixmap pixmap = IconFontManager::pixmap(icon, sizePx, color, dpr);
    if (pixmap.isNull())
        return false;

    const QSizeF logicalSize = pixmap.deviceIndependentSize();
    const QPointF position(QRectF(rect).center().x() - logicalSize.width() / 2.0,
                           centerY - logicalSize.height() / 2.0);
    painter->drawPixmap(position, pixmap);
    return true;
}

qreal indicatorCenterY(const QStyleOption *option) {
    const auto *buttonOption = qstyleoption_cast<const QStyleOptionButton *>(option);
    if (!buttonOption || buttonOption->text.isEmpty())
        return QRectF(option->rect).center().y();

    const QFontMetrics &fontMetrics = buttonOption->fontMetrics;
    if (fontMetrics.capHeight() <= 0)
        return QRectF(option->rect).center().y();

    const qreal lineTop = option->rect.top()
        + (option->rect.height() - fontMetrics.height()) / 2.0;
    const qreal baseline = lineTop + fontMetrics.ascent();
    return baseline - fontMetrics.capHeight() / 2.0;
}

qreal targetDevicePixelRatio(const QPainter *painter, const QWidget *widget) {
    if (widget)
        return widget->devicePixelRatioF();
    if (painter && painter->device())
        return painter->device()->devicePixelRatioF();
    return 1.0;
}

const QComboBox *comboBoxForStyleOption(const QStyleOption *option,
                                        const QWidget *widget) {
    if (const auto *comboBox = qobject_cast<const QComboBox *>(widget))
        return comboBox;
    if (option && option->styleObject)
        return qobject_cast<const QComboBox *>(option->styleObject);
    return nullptr;
}

QColor resolvedColor(const QColor &configuredColor, const QColor &fallbackColor) {
    return configuredColor.isValid() ? configuredColor : fallbackColor;
}

void drawFlatComboBoxPanel(QPainter *painter, const QStyleOption *option,
                           const ProxyStyleColors &colors) {
    const bool enabled = option->state.testFlag(QStyle::State_Enabled);
    QColor background = resolvedColor(
        colors.control, option->palette.color(QPalette::Button));
    if (option->state.testFlag(QStyle::State_Sunken)
        || option->state.testFlag(QStyle::State_On)) {
        background = resolvedColor(
            colors.controlPressed, option->palette.color(QPalette::Dark));
    } else if (enabled && option->state.testFlag(QStyle::State_MouseOver)) {
        background = resolvedColor(
            colors.controlHover, option->palette.color(QPalette::Light));
    }

    QColor border = resolvedColor(
        colors.controlBorder, option->palette.color(QPalette::Mid));
    if (enabled && option->state.testFlag(QStyle::State_HasFocus)) {
        border = resolvedColor(
            colors.controlFocusBorder,
            option->palette.color(QPalette::Highlight));
    }
    border = ProxyStyle::iconColorForState(border, enabled);

    QRectF panelRect(option->rect);
    panelRect.adjust(kComboBoxBorderInsetPx, kComboBoxBorderInsetPx,
                     -kComboBoxBorderInsetPx, -kComboBoxBorderInsetPx);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(QPen(border, kComboBoxBorderWidthPx));
    painter->setBrush(background);
    painter->drawRoundedRect(panelRect, kComboBoxCornerRadiusPx,
                             kComboBoxCornerRadiusPx);
    painter->restore();
}

} // namespace

QColor ProxyStyle::iconColorForState(QColor color, bool enabled) {
    if (!enabled)
        color.setAlphaF(color.alphaF() * kDisabledIconOpacity);
    return color;
}

void ProxyStyle::setColors(const ProxyStyleColors &colors) {
    if (mColors == colors)
        return;

    mColors = colors;
    for (QWidget *widget : QApplication::allWidgets())
        widget->update();
}

void ProxyStyle::drawComplexControl(QStyle::ComplexControl control,
                                    const QStyleOptionComplex *option,
                                    QPainter *painter,
                                    const QWidget *widget) const {
    if (!option || !painter) {
        qWarning() << "ProxyStyle::drawComplexControl called without a style option or painter";
        return;
    }

    if (control != CC_ComboBox) {
        QProxyStyle::drawComplexControl(control, option, painter, widget);
        return;
    }

    const auto *comboBoxOption = qstyleoption_cast<const QStyleOptionComboBox *>(option);
    if (!comboBoxOption) {
        QProxyStyle::drawComplexControl(control, option, painter, widget);
        return;
    }

    const bool drawArrow = comboBoxOption->subControls.testFlag(SC_ComboBoxArrow);
    QStyleOptionComboBox contentOption = *comboBoxOption;
    contentOption.subControls.setFlag(SC_ComboBoxArrow, false);
    QProxyStyle::drawComplexControl(control, &contentOption, painter, widget);

    if (!drawArrow)
        return;

    const QComboBox *comboBox = comboBoxForStyleOption(option, widget);
    if (comboBox
        && comboBox->property(kCustomComboBoxIndicatorProperty).toBool()) {
        return;
    }

    const QRect arrowRect = QProxyStyle::subControlRect(
        control, comboBoxOption, SC_ComboBoxArrow, widget);
    const QColor color = indicatorColor(mColors.icons, comboBoxOption);
    const qreal dpr = targetDevicePixelRatio(painter, widget);
    if (drawCenteredIcon(painter, arrowRect, FluentIcon::ChevronDown12,
                         kComboBoxChevronSizePx, QRectF(arrowRect).center().y() + 1.0,
                         color, dpr)) {
        return;
    }

    QStyleOptionComboBox fallbackOption = *comboBoxOption;
    fallbackOption.subControls = SC_ComboBoxArrow;
    QProxyStyle::drawComplexControl(control, &fallbackOption, painter, widget);
}

void ProxyStyle::drawPrimitive(QStyle::PrimitiveElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const {
    if (!option || !painter) {
        qWarning() << "ProxyStyle::drawPrimitive called without a style option or painter";
        return;
    }

    if (element == PE_FrameFocusRect)
        return;

    if (element == PE_PanelButtonCommand
        && comboBoxForStyleOption(option, widget)) {
        drawFlatComboBoxPanel(painter, option, mColors);
        return;
    }

    const QColor color = indicatorColor(mColors.icons, option);
    const qreal dpr = targetDevicePixelRatio(painter, widget);
    FluentIcon icon;
    int sizePx = kSelectionIndicatorSizePx;
    qreal centerY = indicatorCenterY(option);

    switch (element) {
    case PE_IndicatorCheckBox:
    case PE_IndicatorItemViewItemCheck:
        if (option->state & State_NoChange)
            icon = FluentIcon::CheckboxIndeterminate16;
        else if (option->state & State_On)
            icon = FluentIcon::CheckboxChecked16;
        else
            icon = FluentIcon::CheckboxUnchecked16;
        break;
    case PE_IndicatorRadioButton:
        icon = option->state & State_On
            ? FluentIcon::Record16
            : FluentIcon::RadioButton16;
        break;
    case PE_IndicatorArrowDown: {
        const QComboBox *comboBox = comboBoxForStyleOption(option, widget);
        if (!comboBox) {
            QProxyStyle::drawPrimitive(element, option, painter, widget);
            return;
        }
        if (comboBox->property(kCustomComboBoxIndicatorProperty).toBool())
            return;
        icon = FluentIcon::ChevronDown12;
        sizePx = kComboBoxChevronSizePx;
        centerY = QRectF(option->rect).center().y() + 1.0;
        break;
    }
    default:
        QProxyStyle::drawPrimitive(element, option, painter, widget);
        return;
    }

    if (!drawCenteredIcon(painter, option->rect, icon, sizePx, centerY,
                          color, dpr))
        QProxyStyle::drawPrimitive(element, option, painter, widget);
}

int ProxyStyle::pixelMetric(QStyle::PixelMetric metric, const QStyleOption *option,
                            const QWidget *widget) const {
    switch (metric) {
    case PM_IndicatorWidth:
    case PM_IndicatorHeight:
    case PM_ExclusiveIndicatorWidth:
    case PM_ExclusiveIndicatorHeight:
        return kSelectionIndicatorSizePx;
    default:
        return QProxyStyle::pixelMetric(metric, option, widget);
    }
}

int ProxyStyle::styleHint(QStyle::StyleHint hint, const QStyleOption *option, const QWidget *widget, QStyleHintReturn *returnData) const {
    if (hint == QStyle::SH_Slider_AbsoluteSetButtons) {
        return Qt::LeftButton;
    }
    return QProxyStyle::styleHint(hint, option, widget, returnData);
}
