#include "settingsbridge.h"

#include "gui/quick/bridges/publish.h"

//------------------------------------------------------------------------------
SettingsBridge::SettingsBridge(const UiSettingsSnapshot &initial,
                               QObject *parent)
    : QObject(parent), mSnapshot(initial) {}

//------------------------------------------------------------------------------
const ViewerSettings &SettingsBridge::viewer() const {
  return mSnapshot.viewer;
}

const PanelSettings &SettingsBridge::panel() const { return mSnapshot.panel; }

const FolderViewSettings &SettingsBridge::folderView() const {
  return mSnapshot.folderView;
}

const OverlaySettings &SettingsBridge::overlays() const {
  return mSnapshot.overlays;
}

//------------------------------------------------------------------------------
void SettingsBridge::apply(const UiSettingsSnapshot &snapshot) {
  publishIfChanged(mSnapshot.viewer, snapshot.viewer, *this,
                   &SettingsBridge::viewerChanged);
  publishIfChanged(mSnapshot.panel, snapshot.panel, *this,
                   &SettingsBridge::panelChanged);
  publishIfChanged(mSnapshot.folderView, snapshot.folderView, *this,
                   &SettingsBridge::folderViewChanged);
  publishIfChanged(mSnapshot.overlays, snapshot.overlays, *this,
                   &SettingsBridge::overlaysChanged);
}
