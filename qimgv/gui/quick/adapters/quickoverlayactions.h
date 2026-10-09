#pragma once

#include <QObject>

class ActionManager;
class ImageViewportController;
class OverlayCoordinator;
class Settings;

// Connects the Qt Quick overlays to the application: runs the overlay
// actions of ActionManager (copy, move, image info, colour adjustments, CAS
// settings) on the OverlayCoordinator, and does the Settings work the
// Settings-free overlay models leave to the application:
// - fills the copy / move destinations when first needed and stores edits
//   (Settings::savedPaths);
// - stores CAS edits and applies them to the viewer directly, without a
//   settings notification (as the widget overlay does);
// - stores the fullscreen info bar toggle;
// - keeps the rename prompt's pass-through shortcuts (exit, rename) current.
// Colour adjustment previews go to the viewer; the displayed-image state
// reaches the coordinator.
//
// All referenced objects must outlive this object. GUI thread only.
class QuickOverlayActions final : public QObject {
    Q_OBJECT
public:
    QuickOverlayActions(ActionManager &actions, Settings &settings,
                        OverlayCoordinator &overlays, ImageViewportController &viewport,
                        QObject *parent = nullptr);

private:
    void connectActions();
    void connectModels();
    void loadCopyTargets();
    void refreshPassThroughShortcuts();

    ActionManager &actions;
    Settings &settings;
    OverlayCoordinator &overlays;
    ImageViewportController &viewport;
};
