#include "viewportinteraction.h"

#include <cstdlib>

namespace {
// Right-button movement (logical px, scaled by DPR) before it counts as a
// zoom, and before a horizontal stroke counts as a next/prev gesture.
constexpr qreal kZoomThresholdPx = 4.0;
constexpr qreal kGestureThresholdPx = 40.0;
// Left-button movement (logical px) that starts dragging the file out.
constexpr int kDragOutThresholdPx = 10;
// A horizontal stroke is a gesture only while it is this many times longer
// than the vertical one.
constexpr int kGestureDominance = 2;
// Trackpad and wheel scrolling speed of the widget viewer.
constexpr qreal kTrackpadScrollMultiplier = 0.7;
constexpr qreal kWheelScrollMultiplier = 2.0;
// Image edge misalignment tolerated by the wheel "is scrollable" check.
constexpr int kScrollEdgeTolerancePx = 2;
} // namespace

InteractionThresholds InteractionThresholds::forDevicePixelRatio(qreal dpr) {
    return {
        .zoom = static_cast<int>(dpr * kZoomThresholdPx),
        .gesture = static_cast<int>(dpr * kGestureThresholdPx),
        .dragOut = kDragOutThresholdPx,
    };
}

void ViewportInteraction::setThresholds(const InteractionThresholds &thresholds) {
    mThresholds = thresholds;
}

const InteractionThresholds &ViewportInteraction::thresholds() const {
    return mThresholds;
}

ViewportInteraction::Mode ViewportInteraction::mode() const {
    return mMode;
}

bool ViewportInteraction::isBusy() const {
    return mMode != Mode::None && mMode != Mode::DragOut;
}

void ViewportInteraction::press(QPoint pos) {
    mPressPos = pos;
    mLastPos = pos;
}

InteractionStep ViewportInteraction::move(QPoint pos, Qt::MouseButtons buttons,
                                          const InteractionContext &context) {
    if (context.panorama && buttons.testFlag(Qt::LeftButton)) {
        const QPoint delta = pos - mLastPos;
        mLastPos = pos;
        return {.kind = InteractionStep::Kind::PanoramaDrag, .delta = delta};
    }
    if (!context.hasImage || mMode == Mode::DragOut || mMode == Mode::WheelZoom)
        return {};
    if (buttons.testFlag(Qt::LeftButton))
        return moveLeft(pos, context);
    if (buttons.testFlag(Qt::RightButton))
        return moveRight(pos, context);
    return {};
}

InteractionStep ViewportInteraction::moveLeft(QPoint pos, const InteractionContext &context) {
    if (mMode == Mode::None) {
        if (!context.imageFits)
            mMode = Mode::Pan;
        else if (context.dragsEnabled)
            mMode = Mode::DragBegin;
    }
    if (mMode == Mode::DragBegin) {
        const QPoint moved = pos - mPressPos;
        if (std::abs(moved.x()) > mThresholds.dragOut || std::abs(moved.y()) > mThresholds.dragOut) {
            mMode = Mode::DragOut;
            return {.kind = InteractionStep::Kind::DragOut};
        }
        return {};
    }
    if (mMode == Mode::Pan) {
        // The image may have shrunk to fit (zoomed out by a shortcut) while
        // panning; then there is nothing to pan, as in the widget viewer.
        if (context.imageFits)
            return {};
        const QPoint delta = mLastPos - pos;
        mLastPos = pos;
        return {.kind = InteractionStep::Kind::Pan, .delta = delta};
    }
    return {};
}

InteractionStep ViewportInteraction::moveRight(QPoint pos, const InteractionContext &) {
    if (mMode == Mode::None) {
        const int dx = pos.x() - mPressPos.x();
        const int dy = pos.y() - mPressPos.y();
        // Wait for some movement to decide the direction.
        if (std::abs(dx) <= mThresholds.zoom && std::abs(dy) <= mThresholds.zoom)
            return {};
        if (std::abs(dx) > std::abs(dy) * kGestureDominance) {
            if (std::abs(dx) > mThresholds.gesture) {
                mMode = Mode::Gesture;
                return {.kind = dx < 0 ? InteractionStep::Kind::NextImage
                                       : InteractionStep::Kind::PrevImage};
            }
        } else if (std::abs(dy) > mThresholds.zoom) {
            // The first zoom step covers the movement since the press.
            mMode = Mode::Zoom;
        }
        return {};
    }
    if (mMode == Mode::Zoom) {
        const int distance = mLastPos.y() - pos.y();
        mLastPos = pos;
        return {.kind = InteractionStep::Kind::GestureZoom, .distance = distance};
    }
    return {};
}

void ViewportInteraction::beginWheelZoom() {
    mMode = Mode::WheelZoom;
}

ViewportInteraction::Release ViewportInteraction::release(bool hasImage) {
    const Release result{
        .consumed = hasImage && mMode != Mode::None && mMode != Mode::DragOut,
        .rescale = mMode == Mode::Zoom || mMode == Mode::WheelZoom || mMode == Mode::Pan,
    };
    mMode = Mode::None;
    return result;
}

void ViewportInteraction::resetPressPosition(QPoint pos) {
    mPressPos = pos;
}

//------------------------------------------------------------------------------
bool WheelClassifier::isMouseWheel(QPoint angleDelta, bool detectTrackpad, qint64 nowMs) {
    if (!detectTrackpad)
        return true;
    const int dy = angleDelta.y();
    const bool cooledDown = !mTrackpadSeen || nowMs - mLastTrackpadMs > kTrackpadCooldownMs;
    const bool wheel = dy != 0 && std::abs(dy) >= kNotchAngleDelta &&
                       dy % kHalfNotchAngleDelta == 0 && cooledDown;
    if (!wheel) {
        mTrackpadSeen = true;
        mLastTrackpadMs = nowMs;
    }
    return wheel;
}

QPointF trackpadScrollDelta(QPoint angleDelta, QPoint pixelDelta) {
    // One of the deltas may be multiplied by some scale value; the larger
    // one is used.
    const int dx = std::abs(angleDelta.x()) > std::abs(pixelDelta.x()) ? angleDelta.x()
                                                                       : pixelDelta.x();
    const int dy = std::abs(angleDelta.y()) > std::abs(pixelDelta.y()) ? angleDelta.y()
                                                                       : pixelDelta.y();
    return {-dx * kTrackpadScrollMultiplier, -dy * kTrackpadScrollMultiplier};
}

int wheelScrollDistance(int angleDeltaY, qreal scrollingSpeed) {
    return static_cast<int>(-angleDeltaY * kWheelScrollMultiplier * scrollingSpeed);
}

bool wheelCanScroll(int angleDeltaY, const QRect &imageRect, int viewportHeight) {
    return (angleDeltaY < 0 && imageRect.bottom() > viewportHeight + kScrollEdgeTolerancePx) ||
           (angleDeltaY > 0 && imageRect.top() < -kScrollEdgeTolerancePx);
}
