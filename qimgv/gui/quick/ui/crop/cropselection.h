#pragma once

#include <QPoint>
#include <QPointF>
#include <QRect>
#include <QSize>

// The part of the crop selection a pointer drag acts on.
enum class CropHandle {
    None,
    Move,
    Left,
    Right,
    Top,
    Bottom,
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight,
};

// The crop selection in source image pixels, with the selection rules of the
// widget CropOverlay:
// - the selection always lies inside the image; edits that would leave it
//   are moved (and shrunk) back in;
// - while the aspect ratio is locked, resizing keeps it: corners resize
//   from the opposite corner, edges from the opposite edge around the
//   centre line, both limited by the image;
// - a resize that crosses its anchor flips to the opposite handle;
// - a new selection starts at a point and takes its direction from the
//   first pointer travel that moves along both axes; one without such
//   travel is dropped when the drag ends.
//
// A drag applies the whole pointer travel since it began to the selection it
// started from, so no precision is lost between pointer moves (the widget
// overlay applied each move's rounded delta).
//
// Value type without Qt object dependencies; the CropController owns it.
class CropSelection {
public:
    // Aspect ratio used until one is set (the widget overlay's default).
    static constexpr QPointF kDefaultAspectRatio{16.0, 9.0};

    [[nodiscard]] QSize imageSize() const;
    // Sets the image and fits the selection to the aspect ratio (the whole
    // image while the ratio is unlocked).
    void setImageSize(QSize size);

    [[nodiscard]] QRect rect() const;
    [[nodiscard]] bool hasSelection() const;

    [[nodiscard]] bool isAspectLocked() const;
    [[nodiscard]] QPointF aspectRatio() const;
    // Locks the selection to ratio and fits it. A ratio with a zero
    // component is ignored; returns whether it was accepted.
    bool setAspectRatio(QPointF ratio);
    void setAspectLocked(bool locked);
    // The largest centred selection with the locked ratio, or the whole
    // image while unlocked.
    void fitToAspectRatio();
    void selectAll();
    void clear();
    // Replaces the selection with requested, moved and shrunk into the
    // image. An empty request is ignored. Returns true when the selection
    // differs from requested (the inputs that asked for it must follow).
    bool place(QRect requested);

    // --- pointer drags -------------------------------------------------
    // Starts moving (Move) or resizing (an edge or corner) the current
    // selection.
    void beginDrag(CropHandle handle);
    // Starts a new selection at imagePoint (image pixels).
    void beginNewSelection(QPoint imagePoint);
    // Applies the pointer travel since the drag began, in image pixels.
    // Returns the handle dragged now (None while a new selection has no
    // direction yet).
    CropHandle dragBy(QPointF travel);
    // Ends the drag; a new selection that never got a direction is cleared.
    void endDrag();
    [[nodiscard]] bool isDragging() const;

    // Moves and, where needed, shrinks inner into outer.
    [[nodiscard]] static QRect placeInside(QRect inner, QRect outer);

private:
    enum class DragKind { None, Existing, NewSelection };

    [[nodiscard]] QRect imageRect() const;
    [[nodiscard]] QRect resizedFree(CropHandle handle, QPoint delta, CropHandle &result) const;
    [[nodiscard]] QRect resizedCornerLocked(CropHandle handle, QPoint delta,
                                            CropHandle &result) const;
    [[nodiscard]] QRect resizedEdgeLocked(CropHandle handle, QPoint delta,
                                          CropHandle &result) const;

    QSize mImageSize;
    QRect mRect;
    bool mAspectLocked = false;
    QPointF mAspectRatio = kDefaultAspectRatio;

    DragKind mDragKind = DragKind::None;
    // The selection and handle the drag started from.
    QRect mDragStart;
    CropHandle mDragHandle = CropHandle::None;
};
