#pragma once

#include <QApplication>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QImageWriter>
#include <QMessageBox>
#include <QMimeData>
#include <QObject>
#include <QWidget>
#include <QWindow>




#include "components/actionmanager/actionmanager.h"
#include "gui/centralwidget.h"
#include "gui/customwidgets/floatingwidgetcontainer.h"
#include "gui/dialogs/filereplacedialog.h"
#include "gui/dialogs/resizedialog.h"
#include "gui/dialogs/batchconverterdialog.h"
#include "gui/dialogs/settingsdialog.h"
#include "gui/folderview/folderviewproxy.h"
#include "gui/overlays/cassettingsoverlay.h"
#include "gui/overlays/coloradjustmentsoverlayproxy.h"
#include "gui/overlays/controlsoverlay.h"
#include "gui/overlays/copyoverlay.h"
#include "gui/overlays/cropoverlay.h"
#include "gui/overlays/floatingmessageproxy.h"
#include "gui/overlays/fullscreeninfooverlayproxy.h"
#include "gui/overlays/imageinfooverlayproxy.h"
#include "gui/overlays/renameoverlay.h"
#include "gui/overlays/saveconfirmoverlay.h"
#include "gui/panels/croppanel/croppanel.h"
#include "gui/panels/mainpanel/thumbnailstrip.h"
#include "gui/panels/sidepanel/sidepanel.h"
#include "gui/viewers/documentwidget.h"
#include "gui/viewers/viewerwidget.h"
#include "settings_types.h"

struct CurrentInfo {
  int index;
  int fileCount;
  QString fileName;
  QString filePath;
  QString directoryName;
  QString directoryPath;
  QSize imageSize;
  qint64 fileSize;
  QString format;
  QString colorProfile;
  bool slideshow;
  bool shuffle;
  bool edited;
};

enum ActiveSidePanel { SIDEPANEL_CROP, SIDEPANEL_NONE };

class MW : public FloatingWidgetContainer {
  Q_OBJECT
public:
  explicit MW(QWidget *parent = nullptr);
  ~MW();
  bool isFullScreen() const;
  bool isCropPanelActive();
  void onScalingFinished(QImage scaled);
  void onUpscaleFinished(const QImage &cropImg, QRect origCrop);
  void hideUpscaledCrop();
  void showImage(std::shared_ptr<const QImage> image, QString filePath = "");
  void showAnimation(const QString &filePath, const QString &format, QSize size);

  QRect visibleImageRect() const;
  QRect visibleOriginalImageRect() const;
  QPixmap currentScaledPixmapCopy() const;
  float getDpr() const;
  float currentScale() const;
  bool panoramaMode() const;
  bool isBusyInteracting() const;

  void setCurrentInfo(int fileIndex, int fileCount, QString filePath,
                      QString fileName, QSize imageSize, qint64 fileSize,
                      QString format, QString colorProfile, bool slideshow, bool shuffle,
                      bool edited);
  void setExifInfo(QMap<QString, QString>);
  std::shared_ptr<FolderViewProxy> getFolderView();
  std::shared_ptr<ThumbnailStripProxy> getThumbnailPanel();

  ViewMode currentViewMode();

  bool showConfirmation(QString title, QString msg);
  DialogResult fileReplaceDialog(QString source, QString target,
                                 FileReplaceMode mode, bool multiple);

private:
  std::shared_ptr<ViewerWidget> viewerWidget;
  QHBoxLayout layout;
  QTimer windowGeometryChangeTimer;
  int currentDisplay;

  bool m_pseudoFullscreen;
  bool cropPanelActive, showInfoBarFullscreen, maximized;
  std::shared_ptr<DocumentWidget> docWidget;
  std::shared_ptr<FolderViewProxy> folderView;
  std::shared_ptr<CentralWidget> centralWidget;
  ActiveSidePanel activeSidePanel;
  SidePanel *sidePanel;
  CropPanel *cropPanel;
  CropOverlay *cropOverlay;
  SaveConfirmOverlay *saveOverlay;

  CopyOverlay *copyOverlay;

  RenameOverlay *renameOverlay;
  ColorAdjustmentsOverlayProxy *colorAdjustmentsOverlay = nullptr;
  CasSettingsOverlay *casSettingsOverlay = nullptr;

  ImageInfoOverlayProxy *imageInfoOverlay;

  ControlsOverlay *controlsOverlay;
  FullscreenInfoOverlayProxy *infoBarFullscreen;
  FloatingMessageProxy *floatingMessage;
  FloatingMessageProxy *floatingMessageFolderView;

  PanelPosition panelPosition;
  CurrentInfo info;
  int lastScalePercent = -1;


  void saveWindowGeometry();
  void restoreWindowGeometry();
  void saveCurrentDisplay();
  void setupUi();
  FloatingMessageProxy *activeFloatingMessage();

  void mouseDoubleClickEvent(QMouseEvent *event);

  void setupCropPanel();
  void setupCopyOverlay();
  void setupSaveOverlay();
  void setupRenameOverlay();
  void preShowResize(QSize sz);
  void setInteractionEnabled(bool mode);

private slots:
  void updateCurrentDisplay();
  void readSettings();
  void adaptToWindowState();
  void onWindowGeometryChanged();
  void onInfoUpdated();
  void onScaleChanged(qreal scale);
  void showScriptSettings();

protected:
  void mouseMoveEvent(QMouseEvent *event);
  bool event(QEvent *event);
  bool eventFilter(QObject *obj, QEvent *event) override;
  void paintEvent(QPaintEvent *event);
  void closeEvent(QCloseEvent *event);
  void dragEnterEvent(QDragEnterEvent *e);
  void dropEvent(QDropEvent *event);
  void resizeEvent(QResizeEvent *event);

  void mousePressEvent(QMouseEvent *event);
  void keyPressEvent(QKeyEvent *event);
  void wheelEvent(QWheelEvent *event);
  void mouseReleaseEvent(QMouseEvent *event);
  void leaveEvent(QEvent *event);

  // bool focusNextPrevChild(bool);
signals:
  void opened(QString);
  void fullscreenStateChanged(bool);
  void copyRequested(QString);
  void moveRequested(QString);
  void copyUrlsRequested(QList<QString>, QString);
  void moveUrlsRequested(QList<QString>, QString);
  void showFoldersChanged(bool);
  void resizeRequested(QSize, ScalingFilter, bool, QString);
  void renameRequested(QString);
  void cropRequested(QRect);
  void cropAndSaveRequested(QRect);
  void discardEditsRequested();
  void saveAsClicked();
  void saveRequested();
  void saveAsRequested(QString);
  void sortingSelected(SortingMode);
  void folderSortingSelected(SortingMode);
  void colorAdjustmentsApplyRequested(float exposure, float contrast, float brightness, float temperature, float tint, float saturation, float hue);
  void batchRequested();
  void suspendRequested();

  // viewerWidget
  void scalingRequested(QSize, ScalingFilter);
  void zoomIn();
  void zoomOut();
  void zoomInCursor();
  void zoomOutCursor();
  void scrollUp();
  void scrollDown();
  void scrollLeft();
  void scrollRight();
  void toggleTransparencyGrid();
  void droppedIn(const QMimeData *, QObject *);
  void draggedOut();
  void nextImageRequested();
  void prevImageRequested();

public slots:
  void setupFullUi();
  void showDefault();
  void showCropPanel();
  void hideCropPanel();
  void toggleFolderView();
  void enableFolderView();
  void enableDocumentView();
  void showSaveDialog(QString filePath);
  QString getSaveFileName(QString fileName);
  void showResizeDialog(QSize initialSize);
  void showSettings();
  void triggerFullScreen();
  void showMessageDirectory(QString dirName);
  void showMessageDirectoryEnd();
  void showMessageDirectoryStart();
  void showMessageFitWindow();
  void showMessageFitWidth();
  void showMessageFitOriginal();
  void showFullScreen();
  void showWindowed();
  void triggerCopyOverlay();
  void copyViewportToClipboard();
  void showMessage(QString text);
  void showMessage(QString text, int duration);
  void hideMessage();
  void showMessageSuccess(QString text);
  void showWarning(QString text);
  void showError(QString text);
  void triggerMoveOverlay();
  void closeFullScreenOrExit();
  void close();
  void triggerCropPanel();
  void updateCropPanelData();
  void showSaveOverlay();
  void hideSaveOverlay();
  void fitWindow();
  void fitWidth();
  void fitOriginal();
  void fitWindowStretch();
  void switchFitMode();
  void closeImage();
  void showContextMenu();
  void onSortingChanged(SortingMode);
  void onFolderSortingChanged(SortingMode);
  void toggleImageInfoOverlay();
  void toggleRenameOverlay(QString currentName);
  void toggleColorAdjustments();
  void toggleCasSettings();
  void setFilterNearest();
  void setFilterBilinear();
  void setFilter(ScalingFilter filter);
  void toggleScalingFilter();
  void cycleScalingFilter();
  void setDirectoryPath(QString path);
  void toggleLockZoom();
  void toggleLockView();
  void toggleFullscreenInfoBar();
  void togglePanorama();
#ifdef USE_UPSCAYL
  void toggleUpscayl();
#endif
  void showBatchConverter(const QList<QString> &paths);
};
