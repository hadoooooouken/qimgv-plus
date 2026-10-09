#include "bridgesnapshots.h"

#include <QGuiApplication>

#include "gui/quick/adapters/themesnapshotbuilder.h"
#include "settings.h"
#include "utils/colormanager.h"
#include "utils/iconfontmanager.h"

//------------------------------------------------------------------------------
UiSettingsSnapshot BridgeSnapshots::readUiSettings(Settings &settings) {
  namespace Enums = SettingsEnums;
  return {
      .viewer =
          {
              .fitMode = static_cast<Enums::FitMode>(settings.imageFitMode()),
              .keepFitMode = settings.keepFitMode(),
              .expandImage = settings.expandImage(),
              .expandLimit = settings.expandLimit(),
              .scalingFilter =
                  static_cast<Enums::ScalingFilter>(settings.scalingFilter()),
              .casSharpening = settings.casSharpening(),
              .casContrast = settings.casContrast(),
              .useUpscayl = settings.useUpscayl(),
              .smoothZoom = settings.enableSmoothZoom(),
              .smoothScroll = settings.enableSmoothScroll(),
              .zoomStep = settings.zoomStep(),
              .useFixedZoomLevels = settings.useFixedZoomLevels(),
              .zoomLevels = settings.zoomLevels(),
              .unlockMinZoom = settings.unlockMinZoom(),
              .focusPointIn1to1Mode =
                  static_cast<Enums::FocusPoint>(settings.focusPointIn1to1Mode()),
              .imageScrolling =
                  static_cast<Enums::ImageScrolling>(settings.imageScrolling()),
              .mouseScrollingSpeed = settings.mouseScrollingSpeed(),
              .trackpadDetection = settings.trackpadDetection(),
              .transparencyGrid = settings.transparencyGrid(),
              .backgroundOpacity = settings.backgroundOpacity(),
              .cursorAutohide = settings.cursorAutohide(),
              .clickableEdges = settings.clickableEdges(),
              .clickableEdgesVisible = settings.clickableEdgesVisible(),
          },
      .panel =
          {
              .enabled = settings.panelEnabled(),
              .position = static_cast<Enums::PanelPosition>(settings.panelPosition()),
              .style = static_cast<Enums::PanelStyle>(settings.thumbPanelStyle()),
              .fullscreenOnly = settings.panelFullscreenOnly(),
              .pinned = settings.panelPinned(),
              .hideDelayMs = settings.panelHideDelayMs(),
              .previewsSize = settings.panelPreviewsSize(),
              .centerSelection = settings.panelCenterSelection(),
              .showSubfolders = settings.showSubfoldersInPanel(),
          },
      .folderView =
          {
              .iconSize = settings.folderViewIconSize(),
              .squareThumbnails = settings.squareThumbnails(),
              .sortingMode = static_cast<Enums::SortingMode>(settings.sortingMode()),
              .sortFolders = settings.sortFolders(),
              .showHiddenFiles = settings.showHiddenFiles(),
              .placesPanel = settings.placesPanel(),
              .placesPanelWidth = settings.placesPanelWidth(),
              .bookmarksExpanded = settings.placesPanelBookmarksExpanded(),
              .treeExpanded = settings.placesPanelTreeExpanded(),
          },
      .overlays =
          {
              .infoBarFullscreen = settings.infoBarFullscreen(),
              .infoBarWindowed = settings.infoBarWindowed(),
              .zoomIndicatorMode =
                  static_cast<Enums::ZoomIndicatorMode>(settings.zoomIndicatorMode()),
              .showSaveOverlay = settings.showSaveOverlay(),
              .defaultCropAction =
                  static_cast<Enums::CropAction>(settings.defaultCropAction()),
          },
      .displayColor =
          {
              .toneMapping = settings.hdrToneMappingEnabled(),
              .toneMapOperator = settings.hdrToneMappingOperator(),
              .hdrWhiteLevel = settings.hdrTargetWhiteLevel(),
              .colorManagement = settings.colorManagementEnabled(),
              .target = settings.colorManagementEnabled()
                            ? ColorManager::getTargetColorSpace()
                            : QColorSpace(),
          },
  };
}

//------------------------------------------------------------------------------
ThemeSnapshot BridgeSnapshots::readTheme(Settings &settings) {
  return buildThemeSnapshot(settings.colorScheme(), QGuiApplication::font(),
                            IconFontManager::family());
}
