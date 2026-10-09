#include "cropselection.h"

#include <QSizeF>
#include <QtGlobal>

#include <cmath>
#include <utility>

namespace {
// The handle on the other side after a resize crossed its anchor.
CropHandle mirroredHorizontally(CropHandle handle) {
    switch (handle) {
    case CropHandle::TopLeft: return CropHandle::TopRight;
    case CropHandle::TopRight: return CropHandle::TopLeft;
    case CropHandle::BottomLeft: return CropHandle::BottomRight;
    case CropHandle::BottomRight: return CropHandle::BottomLeft;
    case CropHandle::Left: return CropHandle::Right;
    case CropHandle::Right: return CropHandle::Left;
    default: return handle;
    }
}

CropHandle mirroredVertically(CropHandle handle) {
    switch (handle) {
    case CropHandle::TopLeft: return CropHandle::BottomLeft;
    case CropHandle::TopRight: return CropHandle::BottomRight;
    case CropHandle::BottomLeft: return CropHandle::TopLeft;
    case CropHandle::BottomRight: return CropHandle::TopRight;
    case CropHandle::Top: return CropHandle::Bottom;
    case CropHandle::Bottom: return CropHandle::Top;
    default: return handle;
    }
}

bool isCorner(CropHandle handle) {
    return handle == CropHandle::TopLeft || handle == CropHandle::TopRight ||
           handle == CropHandle::BottomLeft || handle == CropHandle::BottomRight;
}

bool isLeft(CropHandle handle) {
    return handle == CropHandle::TopLeft || handle == CropHandle::BottomLeft;
}

bool isTop(CropHandle handle) {
    return handle == CropHandle::TopLeft || handle == CropHandle::TopRight;
}

// The corner a corner handle drags and the opposite corner it resizes from.
std::pair<QPoint, QPoint> cornerAndAnchor(const QRect &rect, CropHandle handle) {
    switch (handle) {
    case CropHandle::TopLeft: return {rect.topLeft(), rect.bottomRight()};
    case CropHandle::TopRight: return {rect.topRight(), rect.bottomLeft()};
    case CropHandle::BottomLeft: return {rect.bottomLeft(), rect.topRight()};
    default: return {rect.bottomRight(), rect.topLeft()};
    }
}
} // namespace

QSize CropSelection::imageSize() const {
    return mImageSize;
}

void CropSelection::setImageSize(QSize size) {
    mImageSize = size;
    fitToAspectRatio();
}

QRect CropSelection::rect() const {
    return mRect;
}

bool CropSelection::hasSelection() const {
    return mRect.width() > 0 && mRect.height() > 0;
}

bool CropSelection::isAspectLocked() const {
    return mAspectLocked;
}

QPointF CropSelection::aspectRatio() const {
    return mAspectRatio;
}

bool CropSelection::setAspectRatio(QPointF ratio) {
    if (qFuzzyIsNull(ratio.x()) || qFuzzyIsNull(ratio.y()))
        return false;
    mAspectRatio = ratio;
    mAspectLocked = true;
    fitToAspectRatio();
    return true;
}

void CropSelection::setAspectLocked(bool locked) {
    mAspectLocked = locked;
}

void CropSelection::fitToAspectRatio() {
    if (mImageSize.isEmpty())
        return;
    if (!mAspectLocked) {
        mRect = imageRect();
        return;
    }
    const qreal targetRatio = mAspectRatio.x() / mAspectRatio.y();
    const qreal imageRatio = qreal(mImageSize.width()) / mImageSize.height();
    int width = mImageSize.width();
    int height = mImageSize.height();
    if (targetRatio > imageRatio)
        height = qRound(width / targetRatio);
    else
        width = qRound(height * targetRatio);
    const QRect centred((mImageSize.width() - width) / 2, (mImageSize.height() - height) / 2,
                        width, height);
    mRect = centred.intersected(imageRect());
}

void CropSelection::selectAll() {
    mRect = imageRect();
}

void CropSelection::clear() {
    mRect = QRect();
}

bool CropSelection::place(QRect requested) {
    if (requested.width() <= 0 || requested.height() <= 0)
        return false;
    mRect = placeInside(requested, imageRect());
    return mRect != requested;
}

//------------------------------------------------------------------------------
void CropSelection::beginDrag(CropHandle handle) {
    if (handle == CropHandle::None || !hasSelection()) {
        mDragKind = DragKind::None;
        return;
    }
    mDragKind = DragKind::Existing;
    mDragStart = mRect;
    mDragHandle = handle;
}

void CropSelection::beginNewSelection(QPoint imagePoint) {
    mDragKind = DragKind::NewSelection;
    mDragStart = QRect(imagePoint, imagePoint);
    mDragHandle = CropHandle::None;
    mRect = mDragStart;
}

CropHandle CropSelection::dragBy(QPointF travel) {
    if (mDragKind == DragKind::None)
        return CropHandle::None;
    if (mDragKind == DragKind::NewSelection && mDragHandle == CropHandle::None) {
        // The direction is known once the pointer moved along both axes.
        if (qFuzzyIsNull(travel.x()) || qFuzzyIsNull(travel.y()))
            return CropHandle::None;
        if (travel.x() > 0)
            mDragHandle = travel.y() > 0 ? CropHandle::BottomRight : CropHandle::TopRight;
        else
            mDragHandle = travel.y() > 0 ? CropHandle::BottomLeft : CropHandle::TopLeft;
    }

    const QPoint delta = travel.toPoint();
    CropHandle result = mDragHandle;
    if (mDragHandle == CropHandle::Move) {
        const QRect moved = mDragStart.translated(delta);
        mRect = imageRect().contains(moved) ? moved : placeInside(moved, imageRect());
    } else if (!mAspectLocked) {
        mRect = resizedFree(mDragHandle, delta, result);
    } else if (isCorner(mDragHandle)) {
        mRect = resizedCornerLocked(mDragHandle, delta, result);
    } else {
        mRect = resizedEdgeLocked(mDragHandle, delta, result);
    }
    return result;
}

void CropSelection::endDrag() {
    if (mDragKind == DragKind::NewSelection && mDragHandle == CropHandle::None)
        clear();
    mDragKind = DragKind::None;
    mDragHandle = CropHandle::None;
}

bool CropSelection::isDragging() const {
    return mDragKind != DragKind::None;
}

//------------------------------------------------------------------------------
QRect CropSelection::placeInside(QRect inner, QRect outer) {
    if (inner.width() > outer.width()) {
        inner.setLeft(outer.left());
        inner.setRight(outer.right());
    } else {
        if (inner.left() < outer.left())
            inner.moveLeft(outer.left());
        if (inner.right() > outer.right())
            inner.moveRight(outer.right());
    }
    if (inner.height() > outer.height()) {
        inner.setTop(outer.top());
        inner.setBottom(outer.bottom());
    } else {
        if (inner.top() < outer.top())
            inner.moveTop(outer.top());
        if (inner.bottom() > outer.bottom())
            inner.moveBottom(outer.bottom());
    }
    return inner;
}

QRect CropSelection::imageRect() const {
    return QRect(QPoint(0, 0), mImageSize);
}

QRect CropSelection::resizedFree(CropHandle handle, QPoint delta, CropHandle &result) const {
    QRect rect = mDragStart;
    switch (handle) {
    case CropHandle::TopLeft: rect.setTopLeft(rect.topLeft() + delta); break;
    case CropHandle::TopRight: rect.setTopRight(rect.topRight() + delta); break;
    case CropHandle::BottomLeft: rect.setBottomLeft(rect.bottomLeft() + delta); break;
    case CropHandle::BottomRight: rect.setBottomRight(rect.bottomRight() + delta); break;
    case CropHandle::Left: rect.setLeft(rect.left() + delta.x()); break;
    case CropHandle::Right: rect.setRight(rect.right() + delta.x()); break;
    case CropHandle::Top: rect.setTop(rect.top() + delta.y()); break;
    case CropHandle::Bottom: rect.setBottom(rect.bottom() + delta.y()); break;
    default: break;
    }
    result = handle;
    if (rect.width() < 0) {
        const int left = rect.left();
        rect.setLeft(rect.right());
        rect.setRight(left);
        result = mirroredHorizontally(result);
    }
    if (rect.height() < 0) {
        const int top = rect.top();
        rect.setTop(rect.bottom());
        rect.setBottom(top);
        result = mirroredVertically(result);
    }
    return rect.intersected(imageRect());
}

QRect CropSelection::resizedCornerLocked(CropHandle handle, QPoint delta,
                                         CropHandle &result) const {
    const auto [corner, anchor] = cornerAndAnchor(mDragStart, handle);
    const QPoint span = corner + delta - anchor;
    if (span.x() < 0)
        result = span.y() < 0 ? CropHandle::TopLeft : CropHandle::BottomLeft;
    else
        result = span.y() < 0 ? CropHandle::TopRight : CropHandle::BottomRight;

    const QSizeF maxSize(isLeft(result) ? anchor.x() + 1 : mImageSize.width() - anchor.x(),
                         isTop(result) ? anchor.y() + 1 : mImageSize.height() - anchor.y());
    const qreal targetWidth = std::abs(span.x());
    QSize size;
    if (targetWidth > 0) {
        QSizeF scaled(targetWidth, targetWidth / mAspectRatio.x() * mAspectRatio.y());
        scaled.scale(qMin(targetWidth, maxSize.width()), maxSize.height(), Qt::KeepAspectRatio);
        size = scaled.toSize();
    }

    QRect rect(QPoint(0, 0), size);
    switch (result) {
    case CropHandle::TopLeft: rect.moveBottomRight(anchor); break;
    case CropHandle::TopRight: rect.moveBottomLeft(anchor); break;
    case CropHandle::BottomLeft: rect.moveTopRight(anchor); break;
    default: rect.moveTopLeft(anchor); break;
    }
    return rect;
}

QRect CropSelection::resizedEdgeLocked(CropHandle handle, QPoint delta,
                                       CropHandle &result) const {
    const QRect &start = mDragStart;
    const QSizeF ratio(mAspectRatio.x(), mAspectRatio.y());
    QRect rect = start;
    result = handle;

    if (handle == CropHandle::Left || handle == CropHandle::Right) {
        const int centerY = start.top() + start.height() / 2;
        const int anchorX = handle == CropHandle::Left ? start.right() : start.left();
        qreal targetWidth = handle == CropHandle::Left ? start.width() - delta.x()
                                                        : start.width() + delta.x();
        if (targetWidth < 0) {
            result = mirroredHorizontally(handle);
            targetWidth = -targetWidth;
        }
        const int maxHeight = 2 * qMin(centerY, mImageSize.height() - centerY);
        const int maxWidth = result == CropHandle::Left ? anchorX + 1
                                                        : mImageSize.width() - anchorX;
        const QSize size =
            ratio.scaled(qMin(targetWidth, qreal(maxWidth)), maxHeight, Qt::KeepAspectRatio)
                .toSize();
        rect.setSize(size);
        if (result == CropHandle::Left)
            rect.moveRight(anchorX);
        else
            rect.moveLeft(anchorX);
        rect.moveTop(centerY - size.height() / 2);
        return rect;
    }

    const int centerX = start.left() + start.width() / 2;
    const int anchorY = handle == CropHandle::Top ? start.bottom() : start.top();
    qreal targetHeight = handle == CropHandle::Top ? start.height() - delta.y()
                                                   : start.height() + delta.y();
    if (targetHeight < 0) {
        result = mirroredVertically(handle);
        targetHeight = -targetHeight;
    }
    const int maxWidth = 2 * qMin(centerX, mImageSize.width() - centerX);
    const int maxHeight = result == CropHandle::Top ? anchorY + 1
                                                    : mImageSize.height() - anchorY;
    const QSize size =
        ratio.scaled(maxWidth, qMin(targetHeight, qreal(maxHeight)), Qt::KeepAspectRatio)
            .toSize();
    rect.setSize(size);
    if (result == CropHandle::Top)
        rect.moveBottom(anchorY);
    else
        rect.moveTop(anchorY);
    rect.moveLeft(centerX - size.width() / 2);
    return rect;
}
