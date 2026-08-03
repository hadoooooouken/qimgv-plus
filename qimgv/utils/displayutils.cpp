#include "displayutils.h"

#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsWidget>
#include <QList>
#include <QScreen>
#include <QWidget>
#include <QtGlobal>

namespace {

constexpr qreal kFallbackRefreshRateHz = 60.0;

} // namespace

namespace DisplayUtils {

int animationTimerIntervalMs(const QWidget *referenceWidget) {
    qreal refreshRate = kFallbackRefreshRateHz;
    if (referenceWidget) {
        if (const QScreen *screen = referenceWidget->screen())
            refreshRate = screen->refreshRate();
    }
    if (refreshRate <= 0.0)
        refreshRate = kFallbackRefreshRateHz;
    return qMax(1, qRound(1000.0 / refreshRate));
}

int animationTimerIntervalMs(const QGraphicsWidget *referenceItem) {
    if (referenceItem && referenceItem->scene()) {
        const QList<QGraphicsView *> views = referenceItem->scene()->views();
        if (!views.isEmpty())
            return animationTimerIntervalMs(static_cast<const QWidget *>(views.first()));
    }
    return animationTimerIntervalMs(static_cast<const QWidget *>(nullptr));
}

} // namespace DisplayUtils
