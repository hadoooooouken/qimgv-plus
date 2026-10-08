#pragma once

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

#include "gui/quick/bridges/settingsenums.h"

// UI-relevant settings, grouped by area, as QML value types. They are plain
// snapshots: the application side fills them from Settings
// (gui/quick/adapters/bridgesnapshots.h) and SettingsBridge publishes them.
// Members are value-initialized only so that a default-constructed snapshot
// is well-defined; published values always come from Settings.

// Image viewer: fit, zoom, scrolling, filtering and viewer chrome.
struct ViewerSettings {
  Q_GADGET
  QML_VALUE_TYPE(viewerSettings)
  QML_UNCREATABLE("Provided by AppSettings.viewer")
  Q_PROPERTY(SettingsEnums::FitMode fitMode MEMBER fitMode)
  Q_PROPERTY(bool keepFitMode MEMBER keepFitMode)
  Q_PROPERTY(bool expandImage MEMBER expandImage)
  Q_PROPERTY(int expandLimit MEMBER expandLimit)
  Q_PROPERTY(SettingsEnums::ScalingFilter scalingFilter MEMBER scalingFilter)
  Q_PROPERTY(double casSharpening MEMBER casSharpening)
  Q_PROPERTY(double casContrast MEMBER casContrast)
  Q_PROPERTY(bool smoothZoom MEMBER smoothZoom)
  Q_PROPERTY(bool smoothScroll MEMBER smoothScroll)
  Q_PROPERTY(double zoomStep MEMBER zoomStep)
  Q_PROPERTY(bool useFixedZoomLevels MEMBER useFixedZoomLevels)
  Q_PROPERTY(QString zoomLevels MEMBER zoomLevels)
  Q_PROPERTY(bool unlockMinZoom MEMBER unlockMinZoom)
  Q_PROPERTY(SettingsEnums::FocusPoint focusPointIn1to1Mode MEMBER focusPointIn1to1Mode)
  Q_PROPERTY(SettingsEnums::ImageScrolling imageScrolling MEMBER imageScrolling)
  Q_PROPERTY(double mouseScrollingSpeed MEMBER mouseScrollingSpeed)
  Q_PROPERTY(bool trackpadDetection MEMBER trackpadDetection)
  Q_PROPERTY(bool transparencyGrid MEMBER transparencyGrid)
  Q_PROPERTY(double backgroundOpacity MEMBER backgroundOpacity)
  Q_PROPERTY(bool cursorAutohide MEMBER cursorAutohide)
  Q_PROPERTY(bool clickableEdges MEMBER clickableEdges)
  Q_PROPERTY(bool clickableEdgesVisible MEMBER clickableEdgesVisible)

public:
  SettingsEnums::FitMode fitMode{};
  bool keepFitMode{};
  bool expandImage{};
  int expandLimit{};
  SettingsEnums::ScalingFilter scalingFilter{};
  double casSharpening{};
  double casContrast{};
  bool smoothZoom{};
  bool smoothScroll{};
  double zoomStep{};
  bool useFixedZoomLevels{};
  QString zoomLevels;
  bool unlockMinZoom{};
  SettingsEnums::FocusPoint focusPointIn1to1Mode{};
  SettingsEnums::ImageScrolling imageScrolling{};
  double mouseScrollingSpeed{};
  bool trackpadDetection{};
  bool transparencyGrid{};
  double backgroundOpacity{};
  bool cursorAutohide{};
  bool clickableEdges{};
  bool clickableEdgesVisible{};

  friend bool operator==(const ViewerSettings &,
                         const ViewerSettings &) = default;
};

// Thumbnail strip / main panel.
struct PanelSettings {
  Q_GADGET
  QML_VALUE_TYPE(panelSettings)
  QML_UNCREATABLE("Provided by AppSettings.panel")
  Q_PROPERTY(bool enabled MEMBER enabled)
  Q_PROPERTY(SettingsEnums::PanelPosition position MEMBER position)
  Q_PROPERTY(SettingsEnums::PanelStyle style MEMBER style)
  Q_PROPERTY(bool fullscreenOnly MEMBER fullscreenOnly)
  Q_PROPERTY(bool pinned MEMBER pinned)
  Q_PROPERTY(int hideDelayMs MEMBER hideDelayMs)
  Q_PROPERTY(int previewsSize MEMBER previewsSize)
  Q_PROPERTY(bool centerSelection MEMBER centerSelection)
  Q_PROPERTY(bool showSubfolders MEMBER showSubfolders)

public:
  bool enabled{};
  SettingsEnums::PanelPosition position{};
  SettingsEnums::PanelStyle style{};
  bool fullscreenOnly{};
  bool pinned{};
  int hideDelayMs{};
  int previewsSize{};
  bool centerSelection{};
  bool showSubfolders{};

  friend bool operator==(const PanelSettings &,
                         const PanelSettings &) = default;
};

// Folder view: thumbnail grid, sorting and the places panel.
struct FolderViewSettings {
  Q_GADGET
  QML_VALUE_TYPE(folderViewSettings)
  QML_UNCREATABLE("Provided by AppSettings.folderView")
  Q_PROPERTY(int iconSize MEMBER iconSize)
  Q_PROPERTY(bool squareThumbnails MEMBER squareThumbnails)
  Q_PROPERTY(SettingsEnums::SortingMode sortingMode MEMBER sortingMode)
  Q_PROPERTY(bool sortFolders MEMBER sortFolders)
  Q_PROPERTY(bool showHiddenFiles MEMBER showHiddenFiles)
  Q_PROPERTY(bool placesPanel MEMBER placesPanel)
  Q_PROPERTY(int placesPanelWidth MEMBER placesPanelWidth)
  Q_PROPERTY(bool bookmarksExpanded MEMBER bookmarksExpanded)
  Q_PROPERTY(bool treeExpanded MEMBER treeExpanded)

public:
  int iconSize{};
  bool squareThumbnails{};
  SettingsEnums::SortingMode sortingMode{};
  bool sortFolders{};
  bool showHiddenFiles{};
  bool placesPanel{};
  int placesPanelWidth{};
  bool bookmarksExpanded{};
  bool treeExpanded{};

  friend bool operator==(const FolderViewSettings &,
                         const FolderViewSettings &) = default;
};

// Overlays drawn over the viewer: info bar, zoom indicator, save and crop.
struct OverlaySettings {
  Q_GADGET
  QML_VALUE_TYPE(overlaySettings)
  QML_UNCREATABLE("Provided by AppSettings.overlays")
  Q_PROPERTY(bool infoBarFullscreen MEMBER infoBarFullscreen)
  Q_PROPERTY(bool infoBarWindowed MEMBER infoBarWindowed)
  Q_PROPERTY(SettingsEnums::ZoomIndicatorMode zoomIndicatorMode MEMBER zoomIndicatorMode)
  Q_PROPERTY(bool showSaveOverlay MEMBER showSaveOverlay)
  Q_PROPERTY(SettingsEnums::CropAction defaultCropAction MEMBER defaultCropAction)

public:
  bool infoBarFullscreen{};
  bool infoBarWindowed{};
  SettingsEnums::ZoomIndicatorMode zoomIndicatorMode{};
  bool showSaveOverlay{};
  SettingsEnums::CropAction defaultCropAction{};

  friend bool operator==(const OverlaySettings &,
                         const OverlaySettings &) = default;
};

// Complete set published by SettingsBridge in one step.
struct UiSettingsSnapshot {
  ViewerSettings viewer;
  PanelSettings panel;
  FolderViewSettings folderView;
  OverlaySettings overlays;

  friend bool operator==(const UiSettingsSnapshot &,
                         const UiSettingsSnapshot &) = default;
};
