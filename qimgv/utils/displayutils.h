#pragma once

class QWidget;
class QGraphicsWidget;

// Computes QTimeLine/QTimer update intervals that match the actual
// refresh rate of the screen a widget currently resides on, instead of
// a hardcoded interval. A hardcoded interval that does not evenly
// divide the monitor's refresh period causes a beat frequency between
// the animation timer and the compositor, which is perceived as
// stutter -- this is independent of adaptive sync (G-Sync/FreeSync),
// since QTimeLine/QTimer are software timers, not tied to the
// presentation pipeline.
namespace DisplayUtils {

// referenceWidget may be nullptr; falls back to the fallback refresh
// rate below.
int animationTimerIntervalMs(const QWidget *referenceWidget);

// Overload for QGraphicsWidget items (e.g. inside a QGraphicsScene),
// resolved via the QGraphicsView currently displaying them.
int animationTimerIntervalMs(const QGraphicsWidget *referenceItem);

} // namespace DisplayUtils
