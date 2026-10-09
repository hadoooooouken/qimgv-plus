#include "overlaystate.h"

OverlayState::OverlayState(bool takesKeyboardFocus, QObject *parent)
    : QObject(parent), mTakesKeyboardFocus(takesKeyboardFocus) {}

bool OverlayState::isOpen() const {
    return mOpen;
}

bool OverlayState::isCreated() const {
    return mCreated;
}

bool OverlayState::takesKeyboardFocus() const {
    return mTakesKeyboardFocus;
}

QPointF OverlayState::anchor() const {
    return mAnchor;
}

void OverlayState::setOpen(bool open) {
    if (open && !mCreated) {
        mCreated = true;
        emit createdChanged();
    }
    if (mOpen == open)
        return;
    mOpen = open;
    emit openChanged();
}

void OverlayState::setAnchor(QPointF anchor) {
    if (mAnchor == anchor)
        return;
    mAnchor = anchor;
    emit anchorChanged();
}

void OverlayState::toggle() {
    setOpen(!mOpen);
}

void OverlayState::close() {
    setOpen(false);
}
