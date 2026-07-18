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
    void paintEvent(QPaintEvent *event);
    void mousePressEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);

    virtual void onClick();
    virtual void onPress();
};
