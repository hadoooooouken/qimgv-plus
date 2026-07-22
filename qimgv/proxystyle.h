#pragma once

#include <QApplication>
#include <QColor>
#include <QProxyStyle>

struct ProxyStyleColors {
    QColor icons;
    QColor control;
    QColor controlHover;
    QColor controlPressed;
    QColor controlBorder;
    QColor controlFocusBorder;

    bool operator==(const ProxyStyleColors &) const = default;
};

class ProxyStyle : public QProxyStyle {
public:
    static constexpr auto kCustomComboBoxIndicatorProperty = "qimgvCustomComboBoxIndicator";

    explicit ProxyStyle(QStyle *baseStyle = nullptr) : QProxyStyle(baseStyle) {}
    static QColor iconColorForState(QColor color, bool enabled);
    void setColors(const ProxyStyleColors &colors);

    void drawComplexControl(ComplexControl control, const QStyleOptionComplex *option,
                            QPainter *painter,
                            const QWidget *widget = nullptr) const override;
    void drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget = nullptr) const override;
    int pixelMetric(PixelMetric metric, const QStyleOption *option = nullptr, const QWidget *widget = nullptr) const override;
    int styleHint(StyleHint hint, const QStyleOption *option = nullptr, const QWidget *widget = nullptr, QStyleHintReturn *returnData = nullptr) const override;

private:
    ProxyStyleColors mColors;
};
