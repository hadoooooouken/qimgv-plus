#pragma once

#include <memory>
#include "themestore.h"
#include "utils/script.h"
#include <QApplication>
#include <QColor>
#include <QDebug>
#include <QDir>
#include <QFont>
#include <QFontMetrics>
#include <QImageReader>
#include <QKeySequence>
#include <QMap>
#include <QObject>
#include <QPalette>
#include <QSettings>
#include <QStandardPaths>
#include <QStringList>
#include <QThread>
#include <QVersionNumber>

#include "settings_types.h"

class Settings : public QObject {
  Q_OBJECT
public:
  static Settings *getInstance();
  ~Settings();
  QStringList supportedMimeTypes();
  QList<QByteArray> supportedFormats();
  QString supportedFormatsFilter();
  QString supportedFormatsRegex();
  int panelPreviewsSize();
  void setPanelPreviewsSize(int size);
  bool usePreloader();
  void setUsePreloader(bool mode);
#ifdef USE_UPSCAYL
  bool useUpscayl();
  void setUseUpscayl(bool mode);
  bool preloadUpscayl();
  void setPreloadUpscayl(bool mode);
  QString upscaylModel();
  void setUpscaylModel(const QString &model);
  bool upscaylLimitEnabled();
  void setUpscaylLimitEnabled(bool enabled);
  int upscaylLimitValue();
  void setUpscaylLimitValue(int value);
  bool resizeUseUpscayl();
  void setResizeUseUpscayl(bool enabled);
  bool hasUpscaylModels();
#endif
  bool fullscreenMode();
  void setFullscreenMode(bool mode);
  ImageFitMode imageFitMode();
  void setImageFitMode(ImageFitMode mode);
  QRect windowGeometry();
  void setWindowGeometry(QRect geometry);
  PanelPosition panelPosition();
  void setPanelPosition(PanelPosition);
  bool loopSlideshow();
  void setLoopSlideshow(bool mode);
  void readShortcuts(QMap<QString, QString> &shortcuts);
  void saveShortcuts(const QMap<QString, QString> &shortcuts);
  bool panelEnabled();
  void setPanelEnabled(bool mode);
  int lastDisplay();
  void setLastDisplay(int display);
  bool squareThumbnails();
  void setSquareThumbnails(bool mode);
  bool transparencyGrid();
  void setTransparencyGrid(bool mode);
  bool enableSmoothScroll();
  void setEnableSmoothScroll(bool mode);
  bool enableSmoothZoom();
  void setEnableSmoothZoom(bool mode);
  bool useThumbnailCache();
  void setUseThumbnailCache(bool mode);
  QStringList savedPaths();
  void setSavedPaths(QStringList paths);
  QString tmpDir();
  QString thumbnailCacheDir();
  int thumbnailResolution();
  void setThumbnailResolution(int size);
  int thumbnailerThreadCount();
  void setThumbnailerThreadCount(int count);

  void setExpandImage(bool mode);
  bool expandImage();
  ScalingFilter scalingFilter();
  void setScalingFilter(ScalingFilter mode);
  float casSharpening();
  void setCasSharpening(float value);
  float casContrast();
  void setCasContrast(float value);

  bool panelFullscreenOnly();
  void setPanelFullscreenOnly(bool mode);
  QVersionNumber lastVersion();
  void setLastVersion(QVersionNumber &ver);
  qreal backgroundOpacity();
  void setBackgroundOpacity(qreal value);
  void setSortingMode(SortingMode mode);
  SortingMode sortingMode();
  void readScripts(QMap<QString, Script> &scripts);
  void saveScripts(const QMap<QString, Script> &scripts);
  int folderViewIconSize();
  void setFolderViewIconSize(int value);

  bool firstRun();
  void setFirstRun(bool mode);

  void sync();
  bool cursorAutohide();
  void setCursorAutohide(bool mode);

  bool infoBarFullscreen();
  void setInfoBarFullscreen(bool mode);
  bool infoBarWindowed();
  void setInfoBarWindowed(bool mode);

  bool windowTitleExtendedInfo();
  void setWindowTitleExtendedInfo(bool mode);

  bool maximizedWindow();
  void setMaximizedWindow(bool mode);

  bool keepFitMode();
  void setKeepFitMode(bool mode);

  int expandLimit();
  void setExpandLimit(int value);

  float zoomStep();
  void setZoomStep(float value);
  int JPEGSaveQuality();
  void setJPEGSaveQuality(int value);
  int pngSaveQuality();
  void setPngSaveQuality(int value);
  int modernSaveQuality();
  void setModernSaveQuality(int value);
  void setZoomIndicatorMode(ZoomIndicatorMode mode);
  ZoomIndicatorMode zoomIndicatorMode();
  void setFocusPointIn1to1Mode(ImageFocusPoint mode);
  ImageFocusPoint focusPointIn1to1Mode();
  void setDefaultCropAction(DefaultCropAction mode);
  DefaultCropAction defaultCropAction();
  bool placesPanel();
  void setPlacesPanel(bool mode);

  QStringList bookmarks();
  void setBookmarks(QStringList paths);
  bool placesPanelBookmarksExpanded();
  void setPlacesPanelBookmarksExpanded(bool mode);
  bool placesPanelTreeExpanded();
  void setPlacesPanelTreeExpanded(bool mode);

  void setSlideshowInterval(int ms);
  int slideshowInterval();

  ImageScrolling imageScrolling();
  void setImageScrolling(ImageScrolling mode);

  int placesPanelWidth();
  void setPlacesPanelWidth(int width);

  ViewMode defaultViewMode();
  void setDefaultViewMode(ViewMode mode);

  FolderEndAction folderEndAction();
  void setFolderEndAction(FolderEndAction mode);

  const ColorScheme &colorScheme();
  void setColorScheme(ColorScheme scheme);
  void setColorTid(int tid);

  bool useSystemColorScheme();
  void setUseSystemColorScheme(bool mode);

  ThemeMode themeMode();
  void setThemeMode(ThemeMode mode);

  qreal thumbnailOpacity();
  void setThumbnailOpacity(qreal value);

  bool useBlackBackground();
  void setUseBlackBackground(bool mode);

  void loadTheme();
  void saveTheme();
  bool hasCustomAccent() const { return mHasCustomAccent; }
  void setHasCustomAccent(bool custom) { mHasCustomAccent = custom; }
  void clearCustomAccent();

  void loadStylesheet();

  bool showSaveOverlay();
  void setShowSaveOverlay(bool mode);
  bool confirmDelete();
  void setConfirmDelete(bool mode);
  bool confirmTrash();
  void setConfirmTrash(bool mode);

  bool colorManagementEnabled();
  void setColorManagementEnabled(bool enabled);
  QString monitorColorProfileType();
  void setMonitorColorProfileType(const QString &type);
  QString monitorColorProfilePath();
  void setMonitorColorProfilePath(const QString &path);



  bool printLandscape();
  void setPrintLandscape(bool mode);
  bool printPdfDefault();
  void setPrintPdfDefault(bool mode);
  bool printColor();
  void setPrintColor(bool mode);
  bool printFitToPage();
  void setPrintFitToPage(bool mode);
  QString lastPrinter();
  void setLastPrinter(QString name);
  bool unloadThumbs();
  void setUnloadThumbs(bool mode);
  ThumbPanelStyle thumbPanelStyle();
  void setThumbPanelStyle(ThumbPanelStyle mode);

  bool jxlAnimation();
  void setJxlAnimation(bool mode);
  bool absoluteZoomStep();
  void setAbsoluteZoomStep(bool mode);
  bool autoResizeWindow();
  void setAutoResizeWindow(bool mode);
  int autoResizeLimit();
  void setAutoResizeLimit(int percent);

  bool panelPinned();
  void setPanelPinned(bool mode);
  int memoryAllocationLimit();
  void setMemoryAllocationLimit(int limitMB);
  bool panelCenterSelection();
  void setPanelCenterSelection(bool mode);
  QString language();
  void setLanguage(QString lang);
  bool showSubfoldersInPanel();
  void setShowSubfoldersInPanel(bool mode);

  QString defaultZoomLevels();
  QString zoomLevels();
  void setZoomLevels(QString levels);
  bool useFixedZoomLevels();
  void setUseFixedZoomLevels(bool mode);
  bool unlockMinZoom();
  void setUnlockMinZoom(bool mode);
  bool applyFilterAt100();
  void setApplyFilterAt100(bool mode);
  bool sortFolders();
  void setSortFolders(bool mode);
  bool trackpadDetection();
  void setTrackpadDetection(bool mode);

  void setFolderIconSortingMode(SortingMode mode);
  SortingMode folderIconSortingMode();

  bool clickableEdges();
  void setClickableEdges(bool mode);
  bool clickableEdgesVisible();
  void setClickableEdgesVisible(bool mode);

  float mouseScrollingSpeed();
  void setMouseScrollingSpeed(float value);

  bool showHiddenFiles();
  void setShowHiddenFiles(bool mode);

  bool multiInstance();
  void setMultiInstance(bool mode);

  QString excludedCachePaths();
  void setExcludedCachePaths(QString paths);
  bool isPathExcludedFromCache(const QString &path);


private:
  explicit Settings(QObject *parent = nullptr);
  std::unique_ptr<QSettings> settingsConf, stateConf, themeConf;
  std::unique_ptr<QDir> mTmpDir, mThumbCacheDir, mConfDir;
  ColorScheme mColorScheme;
  bool mHasCustomAccent = false;
  void createColorVariants();

  void setupCache();

signals:
  void settingsChanged();

public slots:
  void sendChangeNotification();
};

extern Settings *settings;
