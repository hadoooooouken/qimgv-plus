#pragma once

#include <QApplication>
#include <QProxyStyle>

class ProxyStyle : public QProxyStyle {
public:
    explicit ProxyStyle(QStyle *baseStyle = nullptr) : QProxyStyle(baseStyle) {}
    virtual void drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget = 0) const;
    int styleHint(StyleHint hint, const QStyleOption *option = nullptr, const QWidget *widget = nullptr, QStyleHintReturn *returnData = nullptr) const override;
};
