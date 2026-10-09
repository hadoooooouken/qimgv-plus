#include "quickvieweractions.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QDebug>
#include <QGuiApplication>

#include "components/actionmanager/actionmanager.h"
#include "components/scalingfilter/scalingfilterselection.h"
#include "gui/quick/ui/imageviewportcontroller.h"
#include "settings.h"

namespace {
// Display time of the scaling filter message (MW::setFilter()).
constexpr int kFilterNotificationMs = 600;

// The messages reuse the widget UI's translations, which lupdate files under
// the main window's context ("MW"); lupdate needs the literal context in
// every QCoreApplication::translate() call.
QString filterName(ScalingFilter filter) {
    switch (filter) {
    case QI_FILTER_NEAREST:
        return QCoreApplication::translate("MW", "Nearest");
    case QI_FILTER_BILINEAR:
        return QCoreApplication::translate("MW", "Bilinear");
    case QI_FILTER_SMART:
        return QCoreApplication::translate("MW", "Smart sharpen");
    case QI_FILTER_CAS:
        return QStringLiteral("FidelityFX-CAS (GPU)");
    case QI_FILTER_SMART_GPU:
        return QCoreApplication::translate("MW", "Smart sharpen (GPU)");
    case QI_FILTER_MKS2021:
        return QCoreApplication::translate("MW", "Magic Kernel Sharp 2021");
    case QI_FILTER_MKS2021_GPU:
        return QCoreApplication::translate("MW", "Magic Kernel Sharp 2021 (GPU)");
    }
    return QCoreApplication::translate("MW", "Configured ") + QString::number(static_cast<int>(filter));
}
} // namespace

QuickViewerActions::QuickViewerActions(ActionManager &actions, Settings &settings,
                                       ImageViewportController &viewport, QObject *parent)
    : QObject(parent),
      settings(settings),
      viewport(viewport) {
    ImageViewportController *view = &viewport;
    connect(&actions, &ActionManager::fitWindow, view, &ImageViewportController::fitWindow);
    connect(&actions, &ActionManager::fitWidth, view, &ImageViewportController::fitWidth);
    connect(&actions, &ActionManager::fitNormal, view, &ImageViewportController::fitOriginal);
    connect(&actions, &ActionManager::fitHeight, view, &ImageViewportController::fitHeight);
    connect(&actions, &ActionManager::toggleFitMode, view, &ImageViewportController::switchFitMode);
    connect(&actions, &ActionManager::zoomIn, view, &ImageViewportController::zoomIn);
    connect(&actions, &ActionManager::zoomOut, view, &ImageViewportController::zoomOut);
    connect(&actions, &ActionManager::zoomInCursor, view, &ImageViewportController::zoomInCursor);
    connect(&actions, &ActionManager::zoomOutCursor, view, &ImageViewportController::zoomOutCursor);
    connect(&actions, &ActionManager::scrollUp, view, &ImageViewportController::scrollUp);
    connect(&actions, &ActionManager::scrollDown, view, &ImageViewportController::scrollDown);
    connect(&actions, &ActionManager::scrollLeft, view, &ImageViewportController::scrollLeft);
    connect(&actions, &ActionManager::scrollRight, view, &ImageViewportController::scrollRight);
    connect(&actions, &ActionManager::toggleTransparencyGrid, view,
            &ImageViewportController::toggleTransparencyGrid);
    connect(&actions, &ActionManager::togglePanorama, view, &ImageViewportController::togglePanorama);
    connect(&actions, &ActionManager::copyViewportClipboard, view,
            &ImageViewportController::grabVisibleImage);
    connect(view, &ImageViewportController::visibleImageGrabbed, this,
            &QuickViewerActions::copyToClipboard);
    connect(view, &ImageViewportController::visibleImageGrabFailed, this, [this]() {
        emit notificationRequested({QCoreApplication::translate(
                                        "MW", "No viewport image available to copy."),
                                    NotificationKind::Warning, std::nullopt});
    });
    connect(&actions, &ActionManager::lockZoom, this, &QuickViewerActions::toggleLockZoom);
    connect(&actions, &ActionManager::lockView, this, &QuickViewerActions::toggleLockView);
    connect(&actions, &ActionManager::toggleScalingFilter, this,
            &QuickViewerActions::toggleScalingFilter);
    connect(&actions, &ActionManager::cycleScalingFilter, this,
            &QuickViewerActions::cycleScalingFilter);
}

void QuickViewerActions::toggleLockZoom() {
    viewport.toggleLockZoom();
    notify(viewport.isZoomLocked() ? QCoreApplication::translate("MW", "Zoom lock: ON")
                                   : QCoreApplication::translate("MW", "Zoom lock: OFF"));
}

void QuickViewerActions::toggleLockView() {
    viewport.toggleLockView();
    notify(viewport.isViewLocked() ? QCoreApplication::translate("MW", "View lock: ON")
                                   : QCoreApplication::translate("MW", "View lock: OFF"));
}

void QuickViewerActions::toggleScalingFilter() {
    const ScalingFilter configured = settings.scalingFilter();
    const ScalingFilter filter =
        ScalingFilterSelection::toggled(viewport.scalingFilter(), configured);
    selectScalingFilter(filter, filter == configured);
}

void QuickViewerActions::cycleScalingFilter() {
    selectScalingFilter(ScalingFilterSelection::next(viewport.scalingFilter()), true);
}

void QuickViewerActions::selectScalingFilter(ScalingFilter filter, bool persist) {
    notify(QCoreApplication::translate("MW", "Filter: ") + filterName(filter),
           kFilterNotificationMs);
    if (persist)
        settings.setScalingFilter(filter);
    viewport.setScalingFilter(filter);
}

void QuickViewerActions::copyToClipboard(const QImage &image) {
    QClipboard *clipboard = QGuiApplication::clipboard();
    if (!clipboard) {
        qWarning() << "QuickViewerActions: no clipboard";
        return;
    }
    clipboard->setImage(image);
    emit notificationRequested({QCoreApplication::translate(
                                    "MW", "Viewport image copied to clipboard"),
                                NotificationKind::Success, std::nullopt});
}

void QuickViewerActions::notify(const QString &text) {
    emit notificationRequested({text, NotificationKind::Info, std::nullopt});
}

void QuickViewerActions::notify(const QString &text, int durationMs) {
    emit notificationRequested({text, NotificationKind::Info, durationMs});
}
