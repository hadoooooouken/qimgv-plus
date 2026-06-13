#include "proxystyle.h"

void ProxyStyle::drawPrimitive(QStyle::PrimitiveElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const {
    if(PE_FrameFocusRect == element) {
        // do not draw focus rectangle
    } else {
        QProxyStyle::drawPrimitive(element, option, painter, widget);
    }
}

int ProxyStyle::styleHint(QStyle::StyleHint hint, const QStyleOption *option, const QWidget *widget, QStyleHintReturn *returnData) const {
    if (hint == QStyle::SH_Slider_AbsoluteSetButtons) {
        return Qt::LeftButton;
    }
    return QProxyStyle::styleHint(hint, option, widget, returnData);
}
