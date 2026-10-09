pragma ComponentBehavior: Bound

import QtQuick

// The overlays above the viewer and the folder page. Each overlay is created
// asynchronously the first time it is needed (its `created` state) and kept
// for its fade-out and later use; none exists at startup unless the settings
// show it (the fullscreen chrome when starting in fullscreen). The overlay
// that takes the keyboard focus gets it while open.
FocusScope {
    id: overlays

    required property OverlayCoordinator coordinator

    Loader {
        anchors.fill: parent
        active: overlays.coordinator.fullscreenChrome.infoBarActive
        asynchronous: true
        sourceComponent: FullscreenInfoOverlay {
            chrome: overlays.coordinator.fullscreenChrome
        }
    }

    Loader {
        anchors.fill: parent
        active: overlays.coordinator.fullscreenChrome.controlsActive
        asynchronous: true
        sourceComponent: ControlsOverlay {
            chrome: overlays.coordinator.fullscreenChrome
        }
    }

    Loader {
        anchors.fill: parent
        active: overlays.coordinator.imageInfo.created
        asynchronous: true
        sourceComponent: ImageInfoOverlay {
            overlay: overlays.coordinator.imageInfo
            entries: overlays.coordinator.imageInfoModel
        }
    }

    Loader {
        anchors.fill: parent
        active: overlays.coordinator.colorAdjustments.created
        asynchronous: true
        sourceComponent: ColorAdjustmentsOverlay {
            coordinator: overlays.coordinator
        }
    }

    Loader {
        anchors.fill: parent
        active: overlays.coordinator.casSettings.created
        asynchronous: true
        sourceComponent: CasSettingsOverlay {
            coordinator: overlays.coordinator
        }
    }

    Loader {
        anchors.fill: parent
        active: overlays.coordinator.saveConfirm.created
        asynchronous: true
        sourceComponent: SaveConfirmOverlay {
            coordinator: overlays.coordinator
        }
    }

    Loader {
        anchors.fill: parent
        active: overlays.coordinator.copy.created
        asynchronous: true
        focus: overlays.coordinator.copy.open
        sourceComponent: CopyOverlay {
            coordinator: overlays.coordinator
            focus: true
        }
    }

    Loader {
        anchors.fill: parent
        active: overlays.coordinator.rename.created
        asynchronous: true
        focus: overlays.coordinator.rename.open
        sourceComponent: RenameOverlay {
            coordinator: overlays.coordinator
            focus: true
        }
    }

    Loader {
        anchors.fill: parent
        active: overlays.coordinator.messages.created
        asynchronous: true
        sourceComponent: FloatingMessage {
            message: overlays.coordinator.messages
        }
    }
}
