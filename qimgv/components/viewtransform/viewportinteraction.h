#pragma once

#include <QPoint>
#include <QPointF>
#include <QRect>
#include <Qt>
#include <QtGlobal>

// Mouse thresholds of the image viewer in logical pixels, as the widget
// viewer (ImageViewerV2) computes them: the right-button thresholds grow with
// the device pixel ratio (truncated), the drag-out threshold does not.
struct InteractionThresholds {
    // Right-button movement before it counts as a zoom or a gesture.
    int zoom = 0;
    // Horizontal right-button movement that triggers next/previous image.
    int gesture = 0;
    // Left-button movement that starts dragging the file out.
    int dragOut = 0;

    [[nodiscard]] static InteractionThresholds forDevicePixelRatio(qreal dpr);

    friend bool operator==(const InteractionThresholds &,
                           const InteractionThresholds &) = default;
};

// What the viewer looks like to the pointer at the time of an event.
struct InteractionContext {
    bool hasImage = false;
    bool panorama = false;
    // The image fits into the viewport at the current scale (nothing to pan).
    bool imageFits = false;
    // Dragging the file out is allowed (off while a click zone is pressed).
    bool dragsEnabled = true;
};

// One thing the viewer does in response to a pointer move.
struct InteractionStep {
    enum class Kind {
        None,
        // Scroll by `delta` (positive = towards the right/bottom of the image).
        Pan,
        // Right-button zoom by `distance` logical pixels moved up.
        GestureZoom,
        // Rotate the panorama camera by `delta` logical pixels.
        PanoramaDrag,
        // Start dragging the file out of the viewer.
        DragOut,
        NextImage,
        PrevImage,
    };

    Kind kind = Kind::None;
    QPoint delta;
    int distance = 0;

    friend bool operator==(const InteractionStep &, const InteractionStep &) = default;
};

// Mouse interaction state machine of the image viewer, shared logic of the
// widget viewer's mouse handlers in a UI-independent form:
//  - left button: pans an image larger than the viewport, or starts dragging
//    the file out once the pointer leaves the drag-out threshold over an
//    image that fits; in panorama mode it rotates the camera;
//  - right button: a dominant vertical stroke zooms (moving up zooms in), a
//    dominant horizontal stroke past the gesture threshold requests the
//    next (leftwards) or previous (rightwards) image;
//  - right button + wheel: zoom at the cursor (beginWheelZoom()).
// Positions are logical viewport pixels. GUI thread only.
class ViewportInteraction {
public:
    enum class Mode {
        None,
        // Left button down over an image that fits; drag-out pending.
        DragBegin,
        // The file is being dragged out; further moves are ignored until
        // the button is released.
        DragOut,
        Pan,
        Zoom,
        WheelZoom,
        Gesture,
    };

    // Result of a button release.
    struct Release {
        // The press-move-release sequence did something; the release must not
        // trigger a mouse shortcut (for example the right-click menu).
        bool consumed = false;
        // A zoom or pan ended; the view requests a high-quality rescale.
        bool rescale = false;
    };

    void setThresholds(const InteractionThresholds &thresholds);
    [[nodiscard]] const InteractionThresholds &thresholds() const;

    [[nodiscard]] Mode mode() const;
    // True from the first move of a press over the image (drag, pan, zoom or
    // gesture) until the release, except while the file is dragged out.
    [[nodiscard]] bool isBusy() const;

    // A button went down at pos.
    void press(QPoint pos);
    // The pointer moved to pos with buttons held.
    [[nodiscard]] InteractionStep move(QPoint pos, Qt::MouseButtons buttons,
                                       const InteractionContext &context);
    // The wheel turned with the right button held.
    void beginWheelZoom();
    // The last button was released.
    [[nodiscard]] Release release(bool hasImage);
    // Forgets the pressed position (the viewport was resized under it).
    void resetPressPosition(QPoint pos);

private:
    [[nodiscard]] InteractionStep moveLeft(QPoint pos, const InteractionContext &context);
    [[nodiscard]] InteractionStep moveRight(QPoint pos, const InteractionContext &context);

    InteractionThresholds mThresholds = InteractionThresholds::forDevicePixelRatio(1.0);
    Mode mMode = Mode::None;
    // Where the button went down, and the position of the previous move.
    QPoint mPressPos;
    QPoint mLastPos;
};

// Tells mouse wheels from trackpads, which Windows reports identically
// (angleDelta only): a wheel turns in whole notches (multiples of half a
// notch of at least one notch), and a trackpad stream suppresses wheel
// detection for a short cooldown.
class WheelClassifier {
public:
    static constexpr int kNotchAngleDelta = 120;
    static constexpr int kHalfNotchAngleDelta = 60;
    static constexpr qint64 kTrackpadCooldownMs = 250;

    // Classifies one wheel event at nowMs (any monotonic clock). With
    // detection off every event is a wheel.
    [[nodiscard]] bool isMouseWheel(QPoint angleDelta, bool detectTrackpad, qint64 nowMs);

private:
    bool mTrackpadSeen = false;
    qint64 mLastTrackpadMs = 0;
};

// Scroll offset for one trackpad event: per axis the larger of angleDelta
// and pixelDelta, scaled down to match the widget viewer's speed. Positive
// values scroll towards the right/bottom of the image.
[[nodiscard]] QPointF trackpadScrollDelta(QPoint angleDelta, QPoint pixelDelta);

// Scroll distance of one mouse-wheel event (vertical, logical pixels).
[[nodiscard]] int wheelScrollDistance(int angleDeltaY, qreal scrollingSpeed);

// True when a wheel turn of angleDeltaY can scroll imageRect (viewport
// coordinates) inside a viewport viewportHeight pixels high.
[[nodiscard]] bool wheelCanScroll(int angleDeltaY, const QRect &imageRect, int viewportHeight);
