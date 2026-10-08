#pragma once

#include <QObject>
#include <memory>

#include "components/viewmode/viewmodecontroller.h"
#include "gui/ports/uievents.h"
#include "gui/ports/uiports.h"
#include "gui/widgetui/widgetportadapters.h"

class MW;

// Composition root of the Qt Widgets user interface. Owns the main window,
// the view mode state, the inbound UiEvents and the port adapters, and
// wires the window to them and to the global ActionManager. Core talks to
// this UI only through ports(). Must outlive every Core built from ports().
class WidgetUi final : public QObject {
    Q_OBJECT
public:
    WidgetUi();
    ~WidgetUi() override;

    WidgetUi(const WidgetUi &) = delete;
    WidgetUi &operator=(const WidgetUi &) = delete;

    [[nodiscard]] UiPorts ports();

private:
    void forwardWindowEvents();
    void followViewMode();
    void connectWindowActions();

    // Declaration order is construction order; the window is destroyed
    // before the controller and events it forwards to.
    UiEvents events;
    ViewModeController viewModeController;
    std::unique_ptr<MW> window;
    WidgetNotificationAdapter notificationAdapter;
    WidgetDialogAdapter dialogAdapter;
    WidgetViewerAdapter viewerAdapter;
    WidgetShellAdapter shellAdapter;
    WidgetWindowAdapter windowAdapter;
};
