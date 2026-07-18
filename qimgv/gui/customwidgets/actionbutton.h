#pragma once

#include "gui/customwidgets/iconbutton.h"
#include "components/actionmanager/actionmanager.h"

enum TriggerMode {
    PressTrigger,
    ClickTrigger
};

class ActionButton : public IconButton {
public:
    ActionButton(QWidget *parent = nullptr);
    ActionButton(QString _actionName, QString _iconPath, QWidget *parent = nullptr);
    ActionButton(QString _actionName, QString _iconPath, int _size, QWidget *parent = nullptr);
    // Glyph-mode equivalents of the two constructors above. _iconSizePx is
    // the glyph's own render size (matches what the PNG filename's size
    // suffix used to encode), independent of _size, which is the button's
    // fixed widget box.
    ActionButton(QString _actionName, FluentIcon _icon, int _iconSizePx, QWidget *parent = nullptr);
    ActionButton(QString _actionName, FluentIcon _icon, int _iconSizePx, int _size, QWidget *parent = nullptr);
    void setAction(QString _actionName);
    void setTriggerMode(TriggerMode mode);
    TriggerMode triggerMode();

protected:
    void mousePressEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    QString actionName;
    TriggerMode mTriggerMode;
};
