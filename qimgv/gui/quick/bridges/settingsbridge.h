#pragma once

#include <QObject>
#include <QtQml/qqmlregistration.h>

#include "gui/quick/bridges/uisettings.h"

// Read-only view of the UI-relevant settings for QML (AppSettings singleton).
//
// The bridge does not read Settings itself: the composition root
// (QuickUiHost) builds a UiSettingsSnapshot whenever Settings::settingsChanged
// fires and passes it to apply(). Each area has its own notify signal, and
// apply() emits only for the areas whose values changed, so QML bindings on
// unrelated areas are not re-evaluated.
//
// GUI thread only.
class SettingsBridge : public QObject {
  Q_OBJECT
  QML_NAMED_ELEMENT(AppSettings)
  QML_SINGLETON
  QML_UNCREATABLE("Provided by QuickUiHost via setExternalSingletonInstance()")
  Q_PROPERTY(ViewerSettings viewer READ viewer NOTIFY viewerChanged FINAL)
  Q_PROPERTY(PanelSettings panel READ panel NOTIFY panelChanged FINAL)
  Q_PROPERTY(FolderViewSettings folderView READ folderView NOTIFY folderViewChanged FINAL)
  Q_PROPERTY(OverlaySettings overlays READ overlays NOTIFY overlaysChanged FINAL)

public:
  explicit SettingsBridge(const UiSettingsSnapshot &initial,
                          QObject *parent = nullptr);

  [[nodiscard]] const ViewerSettings &viewer() const;
  [[nodiscard]] const PanelSettings &panel() const;
  [[nodiscard]] const FolderViewSettings &folderView() const;
  [[nodiscard]] const OverlaySettings &overlays() const;

  // Publishes snapshot; emits the notify signal of every changed area.
  void apply(const UiSettingsSnapshot &snapshot);

signals:
  void viewerChanged();
  void panelChanged();
  void folderViewChanged();
  void overlaysChanged();

private:
  UiSettingsSnapshot mSnapshot;
};
