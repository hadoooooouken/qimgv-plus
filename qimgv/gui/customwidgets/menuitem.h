// Base class for various menu items.
// Displays entry name, shortcut and an icon.

#pragma once

#include <QLabel>
#include <QStyleOption>
#include <QHBoxLayout>
#include <QSpacerItem>
#include <QColor>
#include "gui/customwidgets/iconbutton.h"
#include "components/actionmanager/actionmanager.h"
#include "utils/iconfontmanager.h"

class MenuItem : public QWidget {
    Q_OBJECT
public:
    MenuItem(QWidget *parent = nullptr);
    void setText(QString mTextLabel);
    QString text();
    void setShortcutText(QString mTextLabel);
    QString shortcut();
    void setIconPath(QString path);
    // MenuItem icons are always constrained to 16x16 (see IconWidget's
    // "MenuItemIcon" accessible-name check), so sizePx defaults to that.
    void setIcon(FluentIcon icon, int sizePx = IconWidget::kMenuItemIconSizePx);
    void setPassthroughClicks(bool mode);
    void setTextColor(QColor color);
    void setIconColor(QColor color);

protected:
    IconButton mIconWidget;
    QLabel mTextLabel, mShortcutLabel;
    QSpacerItem *spacer;
    QHBoxLayout mLayout;
    bool passthroughClicks = true;
    // Padding around the icon inside mIconWidget's box (5px each side,
    // matches the old stylesheet-driven 26 = 16 + 5*2). Box is resized to
    // iconSizePx + 2*kIconPaddingPx on every setIcon() call, so it always
    // fits the requested size. Expressed in logical px like the rest of Qt's
    // widget geometry - HiDPI scaling (100%/200%/fractional) is handled
    // automatically by Qt, no extra scale math needed here.
    static constexpr int kIconPaddingPx = 5;
    void paintEvent(QPaintEvent *event);
    void mousePressEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);

    virtual void onClick();
    virtual void onPress();

private:
    // Vertically nudges the icon glyph so it lands on mTextLabel's optical
    // (cap-height) center instead of the label's geometric center. See the
    // implementation in menuitem.cpp for the rationale.
    void alignIconToTextBaseline();
};
