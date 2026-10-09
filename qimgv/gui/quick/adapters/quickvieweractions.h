#pragma once

#include <QObject>
#include <QString>

#include "gui/ports/notificationport.h"
#include "settings_types.h"

class ActionManager;
class ImageViewportController;
class Settings;

// Runs the viewer actions of ActionManager (fit, zoom, scroll, locks,
// transparency grid, scaling filter, panorama) on the Qt Quick image
// viewport, with the confirmation messages of the widget UI. The messages are
// published through notificationRequested(); the Quick UI shows them once it
// has its notification overlay (S2.3).
//
// All three referenced objects must outlive this object. GUI thread only.
class QuickViewerActions final : public QObject {
    Q_OBJECT
public:
    QuickViewerActions(ActionManager &actions, Settings &settings,
                       ImageViewportController &viewport, QObject *parent = nullptr);

signals:
    void notificationRequested(const NotificationRequest &request);

private:
    void toggleLockZoom();
    void toggleLockView();
    void toggleScalingFilter();
    void cycleScalingFilter();
    // Shows filter in the viewer; persist also stores it as the configured
    // filter (cycling does, toggling to nearest does not).
    void selectScalingFilter(ScalingFilter filter, bool persist);
    void notify(const QString &text);
    void notify(const QString &text, int durationMs);

    Settings &settings;
    ImageViewportController &viewport;
};
