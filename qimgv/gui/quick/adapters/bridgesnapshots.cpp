#include "bridgesnapshots.h"

#include <QGuiApplication>

#include "gui/uimetrics.h"
#include "settings.h"
#include "themestore.h"
#include "utils/iconfontmanager.h"

namespace {

ThemeColors themeColors(const ColorScheme &scheme) {
  return {
      .background = scheme.background,
      .backgroundFullscreen = scheme.background_fullscreen,
      .text = scheme.text,
      .icons = scheme.icons,
      .folderIcons = scheme.folder_icons,
      .thumbnailFolderIcons = scheme.thumbnail_folder_icons,
      .widget = scheme.widget,
      .widgetBorder = scheme.widget_border,
      .accent = scheme.accent,
      .folderView = scheme.folderview,
      .folderViewTopBar = scheme.folderview_topbar,
      .thumbPanel = scheme.thumbpanel,
      .scrollbar = scheme.scrollbar,
      .scrollbarHover = scheme.scrollbar_hover,
      .overlayText = scheme.overlay_text,
      .overlay = scheme.overlay,
      .statusPending = scheme.status_pending,
      .statusError = scheme.status_error,
      .statusProcessing = scheme.status_processing,
      .statusSuccess = scheme.status_success,
      .danger = scheme.danger,
      .trash = scheme.trash,
      .textHc2 = scheme.text_hc2,
      .textHc = scheme.text_hc,
      .textLc = scheme.text_lc,
      .textLc2 = scheme.text_lc2,
      .button = scheme.button,
      .buttonHover = scheme.button_hover,
      .buttonPressed = scheme.button_pressed,
      .panelButton = scheme.panel_button,
      .panelButtonHover = scheme.panel_button_hover,
      .panelButtonPressed = scheme.panel_button_pressed,
      .folderViewHc = scheme.folderview_hc,
      .folderViewHc2 = scheme.folderview_hc2,
      .thumbPanelHc = scheme.thumbpanel_hc,
      .thumbPanelHc2 = scheme.thumbpanel_hc2,
      .thumbPanelText = scheme.thumbpanel_text,
      .folderViewButtonHover = scheme.folderview_button_hover,
      .folderViewButtonPressed = scheme.folderview_button_pressed,
      .inputFieldFocus = scheme.input_field_focus,
      .comboField = scheme.combo_field,
      .comboFieldHover = scheme.combo_field_hover,
      .comboFieldPressed = scheme.combo_field_pressed,
      .comboFieldBorder = scheme.combo_field_border,
  };
}

QFont withPointSize(const QFont &base, int pointSize) {
  QFont font = base;
  font.setPointSize(pointSize);
  return font;
}

ThemeFonts themeFonts(const QFont &base) {
  const UiMetrics::Typography typography = UiMetrics::typographyFor(base);
  return {
      .base = base,
      .compact = withPointSize(base, typography.smallPointSize),
      .section = withPointSize(base, typography.sectionPointSize),
      .large = withPointSize(base, typography.largePointSize),
  };
}

} // namespace

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
  };
}

//------------------------------------------------------------------------------
ThemeSnapshot BridgeSnapshots::readTheme(Settings &settings) {
  const ColorScheme &scheme = settings.colorScheme();
  return {
      .colors = themeColors(scheme),
      .fonts = themeFonts(QGuiApplication::font()),
      .dark = scheme.tid == COLORS_DARK,
      .iconFontFamily = IconFontManager::family(),
  };
}
