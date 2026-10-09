/*
 * This is sort-of a main controller of application.
 * It creates and initializes all components, then sets up gui and actions.
 * Most of communication between components go through here.
 *
 */

#include "core.h"
#include "settings.h"
#include <QDir>
#include <QGuiApplication>
#include <QMimeDatabase>
#include <QProcess>
#include <algorithm>
#include <QRegularExpression>
#include <QCoreApplication>
#include <QThreadPool>
#include <QTimer>
#include "components/actionmanager/actionmanager.h"
#include "components/fileoptask/fileopcontroller.h"
#include "components/upscaler/upscaledecision.h"
#include "components/upscaler/upscaler.h"
#include "components/upscaler/upscaylresizerunnable.h"
#include "gui/controllers/coldstartwindowcontroller.h"
#include "components/wallpaper/wallpapercontroller.h"
#include "components/mimepayload/mimepayloadmanager.h"
#include <QColorSpace>
#include <tchar.h>
#include <windows.h>
#include <psapi.h>
#include "utils/colormanager.h"
#include "utils/hdrtonemapper.h"
#include "utils/imagelib.h"
#include <QScreen>
#include <QThread>
#include "sourcecontainers/imagestatic.h"

namespace {
constexpr int PreloadDebounceDelayMs = 200;
constexpr int PageChangeMessageDurationMs = 900;
constexpr int FolderViewRevealDelayMs = 100;
constexpr int DocumentReadyFallbackMs = 1000;
constexpr int SettledDocumentRevealDelayMs = 0;
constexpr Qt::DropActions SupportedFileDragActions =
    Qt::CopyAction | Qt::MoveAction;

QString wallpaperApplyFailureMessage(const WallpaperApplyResult &result) {
  const QString nativeError = QString::number(result.nativeError);
  switch (result.error) {
  case WallpaperApplyError::StorageDirectoryCreationFailed:
    return QCoreApplication::translate(
        "Core", "Set wallpaper: failed to create application data directory");
  case WallpaperApplyError::RegistryOpenFailed:
    return QCoreApplication::translate(
               "Core",
               "Set wallpaper: failed to open desktop settings (Windows error %1)")
        .arg(nativeError);
  case WallpaperApplyError::WallpaperStyleWriteFailed:
    return QCoreApplication::translate(
               "Core",
               "Set wallpaper: failed to write wallpaper style (Windows error %1)")
        .arg(nativeError);
  case WallpaperApplyError::TileWallpaperWriteFailed:
    return QCoreApplication::translate(
               "Core",
               "Set wallpaper: failed to write wallpaper tiling (Windows error %1)")
        .arg(nativeError);
  case WallpaperApplyError::RegistryCloseFailed:
    return QCoreApplication::translate(
               "Core",
               "Set wallpaper: failed to close desktop settings (Windows error %1)")
        .arg(nativeError);
  case WallpaperApplyError::SystemParametersInfoFailed:
    return QCoreApplication::translate(
               "Core",
               "Set wallpaper: Windows wallpaper update failed (Windows error %1)")
        .arg(nativeError);
  case WallpaperApplyError::None:
    return {};
  }

  return QCoreApplication::translate("Core", "Set wallpaper: unknown error");
}

QString imageSaveFailureMessage(const ImageSaveResult &result) {
  if (!result.retainedBackupPath.isEmpty() &&
      !result.retainedTemporaryPath.isEmpty()) {
    return QCoreApplication::translate(
               "Core",
               "Could not save file. Recovery files were retained:\n"
               "Original backup:\n%1\n\nStaged copy:\n%2")
        .arg(QDir::toNativeSeparators(result.retainedBackupPath),
             QDir::toNativeSeparators(result.retainedTemporaryPath));
  }

  if (!result.retainedTemporaryPath.isEmpty()) {
    return QCoreApplication::translate(
               "Core",
               "Could not save file. A staged recovery copy was retained at:\n%1")
        .arg(QDir::toNativeSeparators(result.retainedTemporaryPath));
  }

  if (!result.retainedBackupPath.isEmpty()) {
    return QCoreApplication::translate(
               "Core",
               "Could not save file. A recovery backup was retained at:\n%1")
        .arg(QDir::toNativeSeparators(result.retainedBackupPath));
  }

  switch (result.error) {
  case ImageSaveError::InvalidSourceImage:
    return QCoreApplication::translate("Core",
                                       "Could not save file: the image is invalid");
  case ImageSaveError::InvalidDestinationPath:
    return QCoreApplication::translate(
        "Core", "Could not save file: the destination path is invalid");
  case ImageSaveError::SourceUnavailable:
    return QCoreApplication::translate(
        "Core", "Could not save file: the source is unavailable");
  case ImageSaveError::TemporaryFileCreationFailed:
    return QCoreApplication::translate(
        "Core", "Could not save file: a temporary file could not be created");
  case ImageSaveError::ImageEncodingFailed:
    return QCoreApplication::translate(
        "Core", "Could not save file: the image could not be encoded");
  case ImageSaveError::TemporaryFileFlushFailed:
    return QCoreApplication::translate(
        "Core", "Could not save file: the temporary file could not be written");
  case ImageSaveError::FileCopyFailed:
    return QCoreApplication::translate("Core",
                                       "Could not save file: the file copy failed");
  case ImageSaveError::CommitFailed:
    return QCoreApplication::translate(
               "Core",
               "Could not save file: the destination could not be replaced (Windows error %1)")
        .arg(result.nativeError);
  case ImageSaveError::RecoveryFailed:
    return QCoreApplication::translate(
               "Core",
               "Could not save file: automatic recovery failed (Windows error %1)")
        .arg(result.nativeError);
  case ImageSaveError::None:
    return {};
  }

  return QCoreApplication::translate("Core", "Could not save file");
}

QString retainedBackupWarningMessage(const ImageSaveResult &result) {
  return QCoreApplication::translate(
             "Core", "File saved, but a backup could not be removed:\n%1")
      .arg(QDir::toNativeSeparators(result.retainedBackupPath));
}

QString mimePayloadFailureMessage(MimePayloadError error) {
  switch (error) {
  case MimePayloadError::ImageUnavailable:
    return QCoreApplication::translate(
        "Core", "Could not prepare image transfer: the image is unavailable");
  case MimePayloadError::SourcePathUnavailable:
    return QCoreApplication::translate(
        "Core",
        "Could not prepare image transfer: the source path is unavailable");
  case MimePayloadError::StorageRootUnavailable:
    return QCoreApplication::translate(
        "Core",
        "Could not prepare image transfer: temporary storage is unavailable");
  case MimePayloadError::PayloadDirectoryCreationFailed:
    return QCoreApplication::translate(
        "Core",
        "Could not prepare image transfer: the temporary directory could not "
        "be created");
  case MimePayloadError::PayloadFileCreationFailed:
    return QCoreApplication::translate(
        "Core",
        "Could not prepare image transfer: the temporary file could not be "
        "created");
  case MimePayloadError::ImageEncodingFailed:
    return QCoreApplication::translate(
        "Core", "Could not prepare image transfer: the image could not be encoded");
  case MimePayloadError::PayloadCommitFailed:
    return QCoreApplication::translate(
        "Core",
        "Could not prepare image transfer: the temporary file could not be "
        "written");
  case MimePayloadError::None:
    return {};
  }

  return QCoreApplication::translate(
      "Core", "Could not prepare image transfer");
}
}

Core::Core(const UiPorts &ports)
    : QObject(), ui(ports), folderEndAction(FOLDER_END_NO_ACTION),
      loopSlideshow(false), mDrag(nullptr), slideshow(false), shuffle(false) {
  initComponents();
  connectComponents();
  connectUiEvents();
  initActions();
  readSettings();
  lastCMEnabled = settings->colorManagementEnabled();
  lastCMType = settings->monitorColorProfileType();
  lastCMPath = settings->monitorColorProfilePath();
  lastHdrEnabled = settings->hdrToneMappingEnabled();
  lastHdrOp = settings->hdrToneMappingOperator();
  lastHdrWhite = settings->hdrTargetWhiteLevel();
  lastThumbnailResolution = settings->thumbnailResolution();
  lastShowSubfoldersInPanel = settings->showSubfoldersInPanel();
  lastSquareThumbnails = settings->squareThumbnails();
  lastShowHiddenFiles = settings->showHiddenFiles();
  lastPanelPreviewsSize = settings->panelPreviewsSize();
  lastSortFolders = settings->sortFolders();
  lastFolderIconSortingMode = settings->folderIconSortingMode();
  lastThumbPanelStyle = settings->thumbPanelStyle();
  slideshowTimer.setSingleShot(true);
  preloadTimer.setSingleShot(true);
  m_raiseWindowRevealTimer.setSingleShot(true);
  connect(&m_raiseWindowRevealTimer, &QTimer::timeout, this, [this]() {
    if (m_raiseWindowDocumentRenderingSettled &&
        !ui.viewer.isRenderingSettled()) {
      m_raiseWindowDocumentRenderingSettled = false;
      m_raiseWindowAwaitingDocumentRendering = true;
      m_raiseWindowRevealTimer.start(DocumentReadyFallbackMs);
      return;
    }
    if (m_raiseWindowAwaitingDocumentRendering) {
      qWarning() << "Raised document rendering did not settle within"
                 << DocumentReadyFallbackMs << "ms; revealing the window";
    }
    m_raiseWindowAwaitingDocumentRendering = false;
    m_raiseWindowDocumentRenderingSettled = false;
    m_raiseWindowConcealed = false;
    ui.window.setWindowConcealed(false);
  });
  connect(&ui.events, &UiEvents::documentRenderingSettled, this, [this]() {
    if (!m_raiseWindowConcealed ||
        !m_raiseWindowAwaitingDocumentRendering) {
      return;
    }

    m_raiseWindowDocumentRenderingSettled = true;
    if (!m_raiseWindowActive) {
      m_raiseWindowAwaitingDocumentRendering = false;
      m_raiseWindowRevealTimer.start(SettledDocumentRevealDelayMs);
    }
  });
  connect(settings, &Settings::settingsChanged, this, &Core::readSettings);

  upscaler = std::make_unique<Upscaler>(this);
  // Initial load: no active image/UI state yet to refresh, so the
  // modelChanged result is not actionable here (unlike in Core::readSettings()).
  (void)upscaler->readSettings();
  wallpaperController = std::make_unique<WallpaperController>(this);
  connect(wallpaperController.get(),
          &WallpaperController::notificationRequested,
          this,
          [this](const NotificationRequest &request) {
            ui.notifications.showNotification(request);
          });
  mimePayloadManager =
      std::make_unique<MimePayloadManager>(settings->tmpDir(), this);
  connect(mimePayloadManager.get(),
          &MimePayloadManager::payloadCleanupFailed,
          this,
          [this](const QString &path) {
            ui.notifications.showError(
                tr("Failed to clean up temporary image-transfer data: %1")
                    .arg(QDir::toNativeSeparators(path)));
          });
  connect(wallpaperController.get(),
          &WallpaperController::wallpaperApplyFinished,
          this,
          [this](const WallpaperApplyResult &result) {
            if (result.succeeded()) {
              ui.notifications.showSuccess(tr("Wallpaper set"));
              return;
            }

            ui.notifications.showError(wallpaperApplyFailureMessage(result));
          },
          Qt::QueuedConnection);
  connect(wallpaperController.get(),
          &WallpaperController::wallpaperFileCleanupFailed,
          this,
          [this](const QString &path) {
            ui.notifications.showError(
                tr("Set wallpaper: failed to clean up wallpaper file: %1")
                    .arg(QDir::toNativeSeparators(path)));
          },
          Qt::QueuedConnection);
  connect(upscaler.get(), &Upscaler::upscaleStarted, this, [this]() {
      ui.notifications.showAiUpscale(tr("AI Upscaling..."), 3600000);
  });
  connect(upscaler.get(), &Upscaler::upscaleFinished, this,
      [this](QImage cropImg, QRect origCrop, QString path, QSize) {
          ui.notifications.hideNotifications();
          if (ui.viewer.panoramaMode()) {
              ui.viewer.hideUpscaledCrop();
              return;
          }
          if (state.hasActiveImage && path == state.currentFilePath)
              ui.viewer.showUpscaledCrop(cropImg, origCrop);
      });
  connect(upscaler.get(), &Upscaler::upscaleAborted, this,
          [this]() { ui.notifications.hideNotifications(); });
  connect(upscaler.get(), &Upscaler::upscaleFailed, this, [this](const QString &error) {
      ui.notifications.hideNotifications();
      ui.notifications.showError(error);
  });
  connect(upscaler.get(), &Upscaler::previewInvalidated, this,
          [this]() { ui.viewer.hideUpscaledCrop(); });

  connect(upscaler.get(), &Upscaler::requestUpscaleParams, this, [this](const QString &path, bool *ok, QRect *visibleRect, double *currentScale, double *dpr) {
      if (!state.hasActiveImage || path != state.currentFilePath || ui.viewer.panoramaMode() || ui.viewer.isBusyInteracting()) {
          *ok = false;
          return;
      }
      *ok = true;
      *visibleRect = ui.viewer.visibleOriginalImageRect();
      *currentScale = ui.viewer.currentScale();
      *dpr = ui.viewer.devicePixelRatio();
  });

  // Off-GUI-thread copy/move (see components/fileoptask/). model is a
  // stable DirectoryModel instance for Core's whole lifetime (attachModel()
  // is only ever called once, from initComponents()), so handing its raw
  // pointer to FileOpController here is safe.
  fileOpController =
      std::make_unique<FileOpController>(model.get(), ui.dialogs, this);
  connect(fileOpController.get(), &FileOpController::progress, this,
          &Core::onFileOpProgress);
  connect(fileOpController.get(), &FileOpController::finished, this,
          &Core::onFileOpFinished);
  connect(fileOpController.get(), &FileOpController::operationFailed, this,
          [this](const QString &message) { ui.notifications.showError(message); });

  QVersionNumber lastVersion = settings->lastVersion();
  if (settings->firstRun())
    onFirstRun();
  else if (appVersion > lastVersion)
    onUpdate();
}

Core::~Core() = default;

void Core::readSettings() {
  if (upscaler) {
      const bool upscaylModelChanged = upscaler->readSettings();
      if (!settings->useUpscayl()) {
          upscaler->reset();
          ui.viewer.hideUpscaledCrop();
      } else if (upscaylModelChanged) {
          ui.viewer.refreshScaling();
      }
  }
  loopSlideshow = settings->loopSlideshow();
  folderEndAction = settings->folderEndAction();
  slideshowTimer.setInterval(settings->slideshowInterval());
  bool showDirs = true;
  if (folderViewPresenter.showDirs() != showDirs)
    folderViewPresenter.setShowDirs(showDirs);

  if (thumbPanelPresenter.showDirs() != settings->showSubfoldersInPanel())
    thumbPanelPresenter.setShowDirs(settings->showSubfoldersInPanel());

  if (shuffle)
    syncRandomizer();
}

void Core::showGui() {
    if (coldStartWindowController) {
      coldStartWindowController->show();
      return;
    }

    qWarning() << "Cold-start window controller is unavailable; showing the"
                  " window immediately";
    ui.window.showWindow();
}

void Core::raiseWindow(const QString &pathReceived) {
  m_pendingRaiseWindowRequests.enqueue(pathReceived);
  if (m_raiseWindowActive) return;

  RaiseWindowGuard raiseWindowGuard(m_raiseWindowActive);

  // The window may currently be hidden (standby) or minimized. Bringing
  // it back involves several native Win32 calls further down (SW_RESTORE,
  // a TOPMOST/NOTOPMOST toggle to steal focus, SetForegroundWindow...).
  // On Windows, SetWindowPos(..., SWP_SHOWWINDOW, ...) right after a
  // hide/minimize can make the OS present the window's raw backing
  // surface for a frame before Qt/DWM has painted real content over it,
  // which is what shows up as a brief white flash. Keeping the window
  // fully transparent for the duration of that dance and only revealing
  // it once everything has settled hides that frame regardless of its
  // cause (same technique ColdStartWindowController uses on first launch).
  const bool needsDelayedReveal = !ui.window.isWindowVisible() || m_raiseWindowConcealed;
  if (needsDelayedReveal) {
    m_raiseWindowRevealTimer.stop();
    m_raiseWindowAwaitingDocumentRendering = false;
    m_raiseWindowDocumentRenderingSettled = false;
    m_raiseWindowConcealed = true;
    ui.window.setWindowConcealed(true);
  }

  drainRaiseWindowRequests();

  showGui();

  HWND hwnd = reinterpret_cast<HWND>(ui.window.nativeWindowHandle());
  if (IsIconic(hwnd)) {
    ShowWindow(hwnd, SW_RESTORE);
  }

  SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
  SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0,
               SWP_SHOWWINDOW | SWP_NOMOVE | SWP_NOSIZE);

  SetForegroundWindow(hwnd);
  SetActiveWindow(hwnd);
  ui.window.raiseAndActivateWindow();

  if (needsDelayedReveal) {
    // The native restore/topmost-toggle sequence above can itself trigger
    // a resize on some setups (e.g. DWM finishing the restore animation a
    // frame late after resuming from standby). Keep draining second-instance
    // requests while transparent so the reveal timer cannot expose an
    // intermediate image or layout.
    m_raiseWindowAwaitingDocumentRendering = true;
    m_raiseWindowDocumentRenderingSettled = false;
    drainRaiseWindowRequests();

    const bool waitsForDocumentRendering =
        ui.viewMode.currentViewMode() == MODE_DOCUMENT;
    m_raiseWindowAwaitingDocumentRendering = waitsForDocumentRendering;
    if (waitsForDocumentRendering && ui.viewer.isRenderingSettled())
      m_raiseWindowDocumentRenderingSettled = true;
    if (!waitsForDocumentRendering) {
      m_raiseWindowDocumentRenderingSettled = false;
      m_raiseWindowRevealTimer.start(FolderViewRevealDelayMs);
    } else if (m_raiseWindowDocumentRenderingSettled) {
      m_raiseWindowAwaitingDocumentRendering = false;
      m_raiseWindowRevealTimer.start(SettledDocumentRevealDelayMs);
    } else {
      m_raiseWindowRevealTimer.start(DocumentReadyFallbackMs);
    }
  }
}

void Core::processRaiseWindowRequest(const QString &pathReceived) {

  if (m_resumeFromStandby) {
      m_resumeFromStandby = false;
      if (!pathReceived.isEmpty()) {
          loadPath(pathReceived);
      } else if (!hasActiveState()) {
          loadDefaultPath();
      } else {
          if (m_lastViewMode == MODE_FOLDERVIEW) {
              ui.viewMode.enableFolderView();
          } else {
              ui.viewMode.enableDocumentView();
          }
          if (m_lastViewMode == MODE_DOCUMENT && !m_lastFilePath.isEmpty()) {
              loadPath(m_lastFilePath);
          }
      }
  } else {
      if (!pathReceived.isEmpty()) {
          loadPath(pathReceived);
      } else if (!hasActiveState()) {
          loadDefaultPath();
      }
  }
}

void Core::drainRaiseWindowRequests() {
  // Let the event queue catch up before showing the window, same as the
  // cold-start path in main.cpp: without this, resuming from standby (or
  // a second instance handing off a path) paints the bare window for one
  // frame before pending layout/style events are processed. If processing
  // events delivers another request, load it in the same hidden transaction
  // and settle its events before returning.
  do {
    while (!m_pendingRaiseWindowRequests.isEmpty())
      processRaiseWindowRequest(m_pendingRaiseWindowRequests.dequeue());
    QCoreApplication::processEvents();
  } while (!m_pendingRaiseWindowRequests.isEmpty());
}

void Core::attachModel(DirectoryModel *_model) {
  model.reset(_model);
  thumbPanelPresenter.setModel(model);
  folderViewPresenter.setModel(model);
  bool showDirs = true;
  folderViewPresenter.setShowDirs(showDirs);
  if (shuffle)
    syncRandomizer();
}

void Core::initComponents() {
  // One Thumbnailer shared by both presenters so opening a folder queues
  // each thumbnail request once instead of racing two separate decodes -
  // must happen before attachModel() triggers the first populateView().
  thumbnailer = std::make_shared<Thumbnailer>();
  thumbPanelPresenter.setThumbnailer(thumbnailer);
  folderViewPresenter.setThumbnailer(thumbnailer);
  auto *directoryModel = new DirectoryModel();
  directoryModel->setDisplayPipeline(ui.viewer.displayPipeline());
  attachModel(directoryModel);
  coldStartWindowController = std::make_unique<ColdStartWindowController>(
      ui.window, ui.viewMode, ui.viewer, ui.events);
}

void Core::connectComponents() {
  thumbPanelPresenter.setView(ui.thumbnailPanelView);
  connect(&thumbPanelPresenter, &DirectoryPresenter::fileActivated, this,
          &Core::onDirectoryViewFileActivated);
  connect(&thumbPanelPresenter, &DirectoryPresenter::filesActivated, this,
          &Core::onDirectoryViewFilesActivated);
  connect(&thumbPanelPresenter, &DirectoryPresenter::dirActivated, this,
          &Core::onThumbPanelDirActivated);
  connect(&thumbPanelPresenter, &DirectoryPresenter::backRequested, this,
          &Core::historyBack);
  connect(&thumbPanelPresenter, &DirectoryPresenter::forwardRequested, this,
          &Core::historyForward);
  connect(&thumbPanelPresenter, &DirectoryPresenter::selectionExpansionFailed,
          this, [this](const QString &error) { ui.notifications.showError(error); });

  folderViewPresenter.setView(ui.folderView);
  connect(&folderViewPresenter, &DirectoryPresenter::fileActivated, this,
          &Core::onDirectoryViewFileActivated);
  connect(&folderViewPresenter, &DirectoryPresenter::filesActivated, this,
          &Core::onDirectoryViewFilesActivated);
  connect(&folderViewPresenter, &DirectoryPresenter::dirActivated, this,
          &Core::loadPath);
  connect(&folderViewPresenter, &DirectoryPresenter::backRequested, this,
          &Core::historyBack);
  connect(&folderViewPresenter, &DirectoryPresenter::forwardRequested, this,
          &Core::historyForward);

  connect(&folderViewPresenter, &DirectoryPresenter::draggedOut, this,
          qOverload<QList<QString>>(&Core::onDraggedOut));

  connect(&folderViewPresenter, &DirectoryPresenter::droppedInto, this,
          &Core::onDirectoryPresenterDroppedInto);

  connect(&folderViewPresenter, &DirectoryPresenter::expandedSelectedPathsReady, this,
          &Core::onBatchConverterPathsReady);
  connect(&folderViewPresenter, &DirectoryPresenter::selectionExpansionFailed,
          this, [this](const QString &error) { ui.notifications.showError(error); });

  connect(scriptManager, &ScriptManager::error, this,
          [this](const QString &error) { ui.notifications.showError(error); });

  connect(model.get(), &DirectoryModel::scalingFinished, this,
          &Core::onScalingFinished);

  connect(settings, &Settings::settingsChanged, this, [this]() {
      bool cmEnabled = settings->colorManagementEnabled();
      QString cmType = settings->monitorColorProfileType();
      QString cmPath = settings->monitorColorProfilePath();
      bool hdrEnabled = settings->hdrToneMappingEnabled();
      int hdrOp = settings->hdrToneMappingOperator();
      int hdrWhite = settings->hdrTargetWhiteLevel();

      bool cmChanged = (cmEnabled != lastCMEnabled) ||
                       (cmType != lastCMType) ||
                       (cmPath != lastCMPath);
      bool hdrChanged = (hdrEnabled != lastHdrEnabled) ||
                        (hdrOp != lastHdrOp) ||
                        (hdrWhite != lastHdrWhite);

      if (cmChanged || hdrChanged) {
          lastCMEnabled = cmEnabled;
          lastCMType = cmType;
          lastCMPath = cmPath;
          lastHdrEnabled = hdrEnabled;
          lastHdrOp = hdrOp;
          lastHdrWhite = hdrWhite;

          ColorManager::invalidateCache();
          if (state.hasActiveImage && state.currentImg) {
              model->clearScaler();
              if (hdrChanged) {
                  model->reload(state.currentFilePath);
                  state.currentImg = model->getImage(state.currentFilePath);
              }
              guiSetImage(state.currentImg);
              updateInfoString();
          }
      }

      if (hdrChanged) {
          // Re-request thumbnails for whatever is currently on screen so
          // already-rendered folder view / thumbnail panel icons pick up
          // the new tone-map settings without requiring the folder to be
          // re-entered. This does not touch the on-disk cache itself -
          // ThumbnailerRunnable's own staleness check (see
          // isToneMapCacheStale) decides per item, on this re-request,
          // whether a fresh decode is actually needed.
          thumbPanelPresenter.reloadModel();
          folderViewPresenter.reloadModel();
      }

      int newThumbnailResolution = settings->thumbnailResolution();
      bool newShowSubfoldersInPanel = settings->showSubfoldersInPanel();
      bool newSquareThumbnails = settings->squareThumbnails();
      bool newShowHiddenFiles = settings->showHiddenFiles();
      int newPanelPreviewsSize = settings->panelPreviewsSize();
      bool newSortFolders = settings->sortFolders();
      SortingMode newFolderIconSortingMode = settings->folderIconSortingMode();
      ThumbPanelStyle newThumbPanelStyle = settings->thumbPanelStyle();

      const bool thumbnailResolutionChanged =
          newThumbnailResolution != lastThumbnailResolution;
      bool layoutChanged = thumbnailResolutionChanged ||
                           (newShowSubfoldersInPanel != lastShowSubfoldersInPanel) ||
                           (newSquareThumbnails != lastSquareThumbnails) ||
                           (newShowHiddenFiles != lastShowHiddenFiles) ||
                           (newPanelPreviewsSize != lastPanelPreviewsSize) ||
                           (newSortFolders != lastSortFolders) ||
                           (newFolderIconSortingMode != lastFolderIconSortingMode) ||
                           (newThumbPanelStyle != lastThumbPanelStyle);

      if (thumbnailResolutionChanged && !thumbnailer->clearCache()) {
          ui.notifications.showError(tr("Failed to clear thumbnail cache"));
      }

      if (layoutChanged) {
          bool folderSortingChanged = (newFolderIconSortingMode != lastFolderIconSortingMode);

          lastThumbnailResolution = newThumbnailResolution;
          lastShowSubfoldersInPanel = newShowSubfoldersInPanel;
          lastSquareThumbnails = newSquareThumbnails;
          lastShowHiddenFiles = newShowHiddenFiles;
          lastPanelPreviewsSize = newPanelPreviewsSize;
          lastSortFolders = newSortFolders;
          lastFolderIconSortingMode = newFolderIconSortingMode;
          lastThumbPanelStyle = newThumbPanelStyle;

          if (folderSortingChanged) {
              ui.shell.notifyFolderSortingChanged(newFolderIconSortingMode);
          }

          thumbPanelPresenter.reloadModel();
          folderViewPresenter.reloadModel();
      }
  });

  connect(model.get(), &DirectoryModel::fileAdded, this, &Core::onFileAdded);
  connect(model.get(), &DirectoryModel::fileRemoved, this,
          &Core::onFileRemoved);
  connect(model.get(), &DirectoryModel::fileRenamed, this,
          &Core::onFileRenamed);
  connect(model.get(), &DirectoryModel::fileModified, this,
          &Core::onFileModified);
  connect(model.get(), &DirectoryModel::loaded, this, &Core::onModelLoaded);
  connect(model.get(), &DirectoryModel::imageReady, this,
          &Core::onModelItemReady);
  connect(model.get(), &DirectoryModel::imageUpdated, this,
          &Core::onModelItemUpdated);
  connect(model.get(), &DirectoryModel::sortingChanged, this,
          &Core::onModelSortingChanged);
  connect(model.get(), &DirectoryModel::loadFailed, this, &Core::onLoadFailed);

  connect(&slideshowTimer, &QTimer::timeout, this, &Core::nextImageSlideshow);
  connect(&preloadTimer, &QTimer::timeout, this, &Core::preloadNeighbors);
}

void Core::connectUiEvents() {
  UiEvents *events = &ui.events;
  connect(events, &UiEvents::pathOpened, this, &Core::loadPath);
  connect(events, &UiEvents::droppedIn, this, &Core::onDropIn);
  connect(events, &UiEvents::copyRequested, this, &Core::copyCurrentFile);
  connect(events, &UiEvents::moveRequested, this, &Core::moveCurrentFile);
  connect(events, &UiEvents::copyUrlsRequested, this,
          qOverload<QList<QString>, QString>(&Core::copyPathsTo));
  connect(events, &UiEvents::moveUrlsRequested, this, &Core::movePathsTo);
  connect(events, &UiEvents::cropRequested, this, &Core::crop);
  connect(events, &UiEvents::cropAndSaveRequested, this, &Core::cropAndSave);
  connect(events, &UiEvents::colorAdjustmentsApplyRequested, this,
          &Core::applyColorAdjustments);
  connect(events, &UiEvents::saveAsRequested, this, &Core::requestSavePath);
  connect(events, &UiEvents::saveRequested, this, &Core::saveCurrentFile);
  connect(events, &UiEvents::clearThumbnailCacheRequested, this,
          &Core::onClearThumbnailCacheRequested);
  connect(events, &UiEvents::batchConversionRequested, this,
          &Core::showBatchConverter);
  connect(events, &UiEvents::renameRequested, this,
          &Core::renameCurrentSelection);
  connect(events, &UiEvents::sortingSelected, this, &Core::sortBy);
  connect(events, &UiEvents::folderSortingSelected, this,
          &Core::onFolderSortingSelected);
  connect(events, &UiEvents::formatFilterSelected, this,
          &Core::onFormatFilterSelected);
  connect(events, &UiEvents::nameFilterSelected, this,
          &Core::onNameFilterSelected);
  connect(events, &UiEvents::showFoldersChanged, this,
          &Core::setFoldersDisplay);
  connect(events, &UiEvents::discardEditsRequested, this, &Core::discardEdits);
  connect(events, &UiEvents::draggedOut, this,
          qOverload<>(&Core::onDraggedOut));
  connect(events, &UiEvents::nextImageRequested, this, &Core::nextImage);
  connect(events, &UiEvents::prevImageRequested, this, &Core::prevImage);
  connect(events, &UiEvents::scalingRequested, this, &Core::scalingRequest);
  connect(events, &UiEvents::upscaleRequested, this, &Core::onUpscaleRequested);
  connect(events, &UiEvents::suspendRequested, this, &Core::suspendToStandby);
}

void Core::initActions() {
  connect(actionManager, &ActionManager::nextImage, this, &Core::nextImage);
  connect(actionManager, &ActionManager::prevImage, this, &Core::prevImage);
  connect(actionManager, &ActionManager::resize, this, &Core::showResizeDialog);
  connect(actionManager, &ActionManager::flipH, this, &Core::flipH);
  connect(actionManager, &ActionManager::flipV, this, &Core::flipV);
  connect(actionManager, &ActionManager::rotateLeft, this, &Core::rotateLeft);
  connect(actionManager, &ActionManager::rotateRight, this, &Core::rotateRight);
  connect(actionManager, &ActionManager::nextPage, this, &Core::nextPage);
  connect(actionManager, &ActionManager::prevPage, this, &Core::prevPage);
  connect(actionManager, &ActionManager::crop, this, &Core::toggleCropPanel);
  connect(actionManager, &ActionManager::setWallpaper, this,
          &Core::setWallpaper);
  connect(actionManager, &ActionManager::save, this, &Core::saveCurrentFile);
  connect(actionManager, &ActionManager::saveAs, this, &Core::requestSavePath);
  connect(actionManager, &ActionManager::exit, this, &Core::forceExit);
  connect(actionManager, &ActionManager::removeFile, this,
          &Core::removePermanent);
  connect(actionManager, &ActionManager::moveToTrash, this, &Core::moveToTrash);
  connect(actionManager, &ActionManager::jumpToFirst, this, &Core::jumpToFirst);
  connect(actionManager, &ActionManager::jumpToLast, this, &Core::jumpToLast);
  connect(actionManager, &ActionManager::runScript, this, &Core::runScript);
  connect(actionManager, &ActionManager::folderView, this,
          &Core::enableFolderView);
  connect(actionManager, &ActionManager::documentView, this,
          &Core::enableDocumentView);
  connect(actionManager, &ActionManager::toggleFolderView, this,
          &Core::toggleFolderView);
  connect(actionManager, &ActionManager::reloadImage, this,
          qOverload<>(&Core::reloadImage));
  connect(actionManager, &ActionManager::copyFileClipboard, this,
          &Core::copyFileClipboard);
  connect(actionManager, &ActionManager::copyPathClipboard, this,
          &Core::copyPathClipboard);
  connect(actionManager, &ActionManager::renameFile, this,
          &Core::showRenameDialog);
  connect(actionManager, &ActionManager::sortByName, this, &Core::sortByName);
  connect(actionManager, &ActionManager::sortByTime, this, &Core::sortByTime);
  connect(actionManager, &ActionManager::sortBySize, this, &Core::sortBySize);
  connect(actionManager, &ActionManager::toggleShuffle, this,
          &Core::toggleShuffle);
  connect(actionManager, &ActionManager::showInDirectory, this,
          &Core::showInDirectory);
  connect(actionManager, &ActionManager::createDirectory, this,
          &Core::createDirectory);
  connect(actionManager, &ActionManager::toggleSlideshow, this,
          &Core::toggleSlideshow);
  connect(actionManager, &ActionManager::goUp, this, &Core::loadParentDir);
  connect(actionManager, &ActionManager::discardEdits, this,
          &Core::discardEdits);
  connect(actionManager, &ActionManager::nextDirectory, this,
          &Core::nextDirectory);
  connect(actionManager, &ActionManager::prevDirectory, this,
          qOverload<>(&Core::prevDirectory));
  connect(actionManager, &ActionManager::print, this, &Core::print);
  connect(actionManager, &ActionManager::toggleFullscreenInfoBar, this,
          &Core::toggleFullscreenInfoBar);
  connect(actionManager, &ActionManager::pasteFile, this,
          &Core::openFromClipboard);
}

void Core::onUpdate() {
  QVersionNumber lastVer = settings->lastVersion();

  actionManager->adjustFromVersion(lastVer);

  qDebug() << "Updated: " << settings->lastVersion().toString() << ">"
           << appVersion.toString();
  ui.notifications.showMessage(tr("Updated: ") + settings->lastVersion().toString() + " > " +
                      appVersion.toString(),
                  4000);
  settings->setLastVersion(appVersion);
}

void Core::onFirstRun() {
  ui.notifications.showMessage(tr("Welcome to ") + QCoreApplication::applicationName() +
                      tr(" version ") + appVersion.toString() + "!",
                  4000);

  settings->setScalingFilter(QI_FILTER_MKS2021_GPU);
  settings->setImageFitMode(FIT_WINDOW);
  settings->setBackgroundOpacity(0.8);
  settings->setThumbnailOpacity(0.6);

  // Set default thumbnailer threads on first run
  int defaultCount = std::clamp(QThread::idealThreadCount() / 2, Settings::MinThumbnailerThreads, Settings::MaxThumbnailerThreads);
  settings->setThumbnailerThreadCount(defaultCount);

  settings->setFirstRun(false);
  settings->setLastVersion(appVersion);
}

void Core::toggleShuffle() {
  if (shuffle) {
    ui.notifications.showMessage(tr("Shuffle mode: OFF"));
  } else {
    syncRandomizer();
    ui.notifications.showMessage(tr("Shuffle mode: ON"));
  }
  shuffle = !shuffle;
  updateInfoString();
}

void Core::toggleSlideshow() {
  if (slideshow) {
    stopSlideshow();
    ui.notifications.showMessage(tr("Slideshow: OFF"));

  } else {
    startSlideshow();
    ui.notifications.showMessage(tr("Slideshow: ON"));
  }
}

void Core::startSlideshow() {
  if (!slideshow) {
    slideshow = true;
    enableDocumentView();
    startSlideshowTimer();
    updateInfoString();
  }
}

void Core::stopSlideshow() {
  if (slideshow) {
    slideshow = false;
    slideshowTimer.stop();
    updateInfoString();
  }
}

void Core::onPlaybackFinished() {
  if (slideshow) {
    nextImageSlideshow();
  }
}

void Core::syncRandomizer() {
  if (model) {
    randomizer.setCount(model->fileCount());
    randomizer.shuffle();
    randomizer.setCurrent(model->indexOfFile(state.currentFilePath));
  }
}

void Core::onModelLoaded() {
  if (coldStartWindowController)
    coldStartWindowController->onDirectoryModelLoaded();
  thumbPanelPresenter.reloadModel();
  folderViewPresenter.reloadModel();
  thumbPanelPresenter.selectAndFocus(state.currentFilePath);
  folderViewPresenter.selectAndFocus(state.currentFilePath);
  if (!pendingFolderViewSelectPath.isEmpty()) {
    // Now that the directory has actually finished loading, restore
    // selection/scroll to the folder we navigated up/back from instead of
    // leaving the view on the first item.
    folderViewPresenter.selectAndFocus(pendingFolderViewSelectPath);
    pendingFolderViewSelectPath.clear();
  }
  // A subfolder or adjacent folder was activated in document mode: the async
  // directory scan has finished, so we can now load the first or last image
  // (if any) while staying in document mode. If the folder contains no images
  // the strip will show its subfolders (when the setting is on) and the main
  // view simply keeps whatever was displayed before.
  // Guard with a view-mode check: if the user manually switched to folder
  // view during the scan, honour that choice instead of forcing an image.
  if (m_pendingDocumentLoad != PendingDocumentLoad::None) {
    auto target = m_pendingDocumentLoad;
    m_pendingDocumentLoad = PendingDocumentLoad::None;
    if (ui.viewMode.currentViewMode() == MODE_DOCUMENT && model->fileCount() > 0) {
      int targetIndex = (target == PendingDocumentLoad::LastImage)
                            ? (model->fileCount() - 1)
                            : 0;
      loadFileIndex(targetIndex, false, settings->usePreloader());
    }
  }
  if (pendingModelImageSync) {
    model->updateImage(state.currentFilePath, state.currentImg);
    pendingModelImageSync = false;
  }
  if (shuffle)
    syncRandomizer();
  updateInfoString();
}

void Core::onThumbPanelDirActivated(QString dirPath) {
  if (dirPath.isEmpty())
    return;
  stopSlideshow();
  QString absolutePath = QDir(dirPath).absolutePath();
  if (!setDirectory(absolutePath))
    return;
  // Stay in document mode; the thumbnail strip will refresh with the new
  // directory's contents once the async scan finishes (onModelLoaded).
  // We cannot call loadFileIndex() yet because fileCount() is still 0;
  // the deferred load is handled via m_pendingDocumentLoad.
  m_pendingDocumentLoad = PendingDocumentLoad::FirstImage;
  if (settings->rememberLastFolder())
    settings->setLastFolder(absolutePath);
}

void Core::onDirectoryViewFileActivated(QString filePath) {
  // we aren`t using async load so it won't flicker with empty view
  ui.viewMode.enableDocumentView();
  loadPath(filePath);
}

void Core::onDirectoryViewFilesActivated(QList<QString> filePaths, QString activePath) {
  ui.viewMode.enableDocumentView();
  loadFileList(filePaths, activePath);
}

bool Core::loadFileList(const QList<QString> &filePaths, QString activePath) {
  if (filePaths.isEmpty())
      return false;

  stopSlideshow();
  
  if (!blockHistory && !model->directoryPath().isEmpty()) {
      backHistory.append(model->directoryPath());
      if (backHistory.count() > 100)
          backHistory.removeFirst();
      forwardHistory.clear();
  }
  
  if (!model->setFileList(filePaths))
      return false;
      
  state.hasActiveImage = false;
  QString toLoad = activePath;
  if (toLoad.isEmpty() || !model->containsFile(toLoad)) {
      toLoad = filePaths.first();
  }
  
  return loadPath(toLoad);
}

void Core::rotateLeft() { rotateByDegrees(-90); }

void Core::rotateRight() { rotateByDegrees(90); }

void Core::nextPage() {
  if (!state.hasActiveImage || !state.currentImg)
    return;
  int count = state.currentImg->frameCount();
  if (count <= 1)
    return;
  QString path = state.currentFilePath;
  int cur = ImageStatic::pageOverrideForPath(path);
  if (cur + 1 >= count)
    return;
  ImageStatic::setPageOverrideForPath(path, cur + 1);
  if (model->reload(path)) {
    showPageChangeMessage(path);
  } else {
    ImageStatic::setPageOverrideForPath(path, cur);
  }
}

void Core::prevPage() {
  if (!state.hasActiveImage || !state.currentImg)
    return;
  QString path = state.currentFilePath;
  int cur = ImageStatic::pageOverrideForPath(path);
  if (cur <= 0)
    return;
  ImageStatic::setPageOverrideForPath(path, cur - 1);
  if (model->reload(path)) {
    showPageChangeMessage(path);
  } else {
    ImageStatic::setPageOverrideForPath(path, cur);
  }
}

void Core::removePermanent() {
  auto paths = currentSelection();
  if (!paths.count())
    return;

  int dirCount = 0;
  int fileCount = 0;
  for (const auto &path : std::as_const(paths)) {
    if (QFileInfo(path).isDir()) {
      dirCount++;
    } else {
      fileCount++;
    }
  }

  if (settings->confirmDelete()) {
    QString msg;
    if (paths.count() > 1) {
      if (dirCount > 0 && fileCount == 0) {
        msg = tr("Delete ") + QString::number(paths.count()) +
              tr(" folders permanently?");
      } else if (fileCount > 0 && dirCount == 0) {
        msg = tr("Delete ") + QString::number(paths.count()) +
              tr(" files permanently?");
      } else {
        msg = tr("Delete ") + QString::number(paths.count()) +
              tr(" items permanently?");
      }
    } else {
      if (dirCount > 0)
        msg = tr("Delete folder permanently?");
      else
        msg = tr("Delete file permanently?");
    }
    if (!ui.dialogs.confirm({tr("Delete permanently"), msg}).accepted)
      return;
  }
  FileOpResult result;
  int successCount = 0;
  for (const auto &path : std::as_const(paths)) {
    QFileInfo fi(path);
    if (fi.isDir())
      model->removeDir(path, false, true, result);
    else
      result = removeFile(path, false);
    if (result == FileOpResult::SUCCESS)
      successCount++;
  }
  if (paths.count() == 1) {
    if (result == FileOpResult::SUCCESS) {
      if (dirCount > 0) {
        ui.shell.refreshFolderTree(QFileInfo(paths.first()).absolutePath());
        ui.notifications.showSuccess(tr("Folder removed"));
      } else
        ui.notifications.showSuccess(tr("File removed"));
    } else {
      outputError(result);
    }
  } else if (paths.count() > 1) {
    if (dirCount > 0 && fileCount == 0) {
      ui.notifications.showSuccess(tr("Removed: ") + QString::number(successCount) +
                             tr(" folders"));
    } else if (fileCount > 0 && dirCount == 0) {
      ui.notifications.showSuccess(tr("Removed: ") + QString::number(successCount) +
                             tr(" files"));
    } else {
      ui.notifications.showSuccess(tr("Removed: ") + QString::number(successCount) +
                             tr(" items"));
    }
  }
}

void Core::moveToTrash() {
  auto paths = currentSelection();
  if (!paths.count())
    return;

  int dirCount = 0;
  int fileCount = 0;
  for (const auto &path : std::as_const(paths)) {
    if (QFileInfo(path).isDir()) {
      dirCount++;
    } else {
      fileCount++;
    }
  }

  if (settings->confirmTrash()) {
    QString msg;
    if (paths.count() > 1) {
      if (dirCount > 0 && fileCount == 0) {
        msg = tr("Move ") + QString::number(paths.count()) + tr(" folders to trash?");
      } else if (fileCount > 0 && dirCount == 0) {
        msg = tr("Move ") + QString::number(paths.count()) + tr(" files to trash?");
      } else {
        msg = tr("Move ") + QString::number(paths.count()) + tr(" items to trash?");
      }
    } else {
      if (dirCount > 0)
        msg = tr("Move folder to trash?");
      else
        msg = tr("Move file to trash?");
    }
    if (!ui.dialogs.confirm({tr("Move to trash"), msg}).accepted)
      return;
  }
  FileOpResult result;
  int successCount = 0;
  for (const auto &path : std::as_const(paths)) {
    QFileInfo fi(path);
    if (fi.isDir())
      model->removeDir(path, true, true, result);
    else
      result = removeFile(path, true);
    if (result == FileOpResult::SUCCESS)
      successCount++;
  }
  if (paths.count() == 1) {
    if (result == FileOpResult::SUCCESS) {
      if (dirCount > 0) {
        ui.shell.refreshFolderTree(QFileInfo(paths.first()).absolutePath());
        ui.notifications.showSuccess(tr("Folder moved to trash"));
      } else
        ui.notifications.showSuccess(tr("Moved to trash"));
    } else {
      outputError(result);
    }
  } else if (paths.count() > 1) {
    if (dirCount > 0 && fileCount == 0) {
      ui.notifications.showSuccess(tr("Moved to trash: ") +
                             QString::number(successCount) + tr(" folders"));
    } else if (fileCount > 0 && dirCount == 0) {
      ui.notifications.showSuccess(tr("Moved to trash: ") +
                             QString::number(successCount) + tr(" files"));
    } else {
      ui.notifications.showSuccess(tr("Moved to trash: ") +
                             QString::number(successCount) + tr(" items"));
    }
  }
}

void Core::reloadImage() { reloadImage(selectedPath()); }

void Core::reloadImage(QString filePath) {
  if (model->isEmpty())
    return;
  model->reload(filePath);
}

void Core::enableFolderView() {
  if (ui.viewMode.currentViewMode() == MODE_FOLDERVIEW)
    return;
  stopSlideshow();
  
  if (model && model->source() == SOURCE_LIST) {
      QString dirToLoad;
      if (!backHistory.isEmpty()) {
          dirToLoad = backHistory.takeLast();
      } else if (model->fileCount() > 0) {
          dirToLoad = QFileInfo(model->filePathAt(0)).absolutePath();
      }

      if (!dirToLoad.isEmpty()) {
          blockHistory = true;
          setDirectory(dirToLoad);
          blockHistory = false;
      }
  }
  
  ui.viewMode.enableFolderView();
}

void Core::enableDocumentView() {
  if (ui.viewMode.currentViewMode() == MODE_DOCUMENT)
    return;
  const auto selectedPaths = folderViewPresenter.selectedPaths();
  ui.viewMode.enableDocumentView();
  if (!model || !model->fileCount())
    return;

  // Prefer the last selected file. A selected directory does not replace the
  // current image; if no image has been opened yet, fall back to the first one.
  const QString selected = selectedPaths.isEmpty() ? QString()
                                                    : selectedPaths.constLast();
  if (model->containsFile(selected))
    loadPath(selected);
  else if (state.currentFilePath.isEmpty())
    loadPath(model->firstFile());
}

void Core::toggleFolderView() {
  if (ui.viewMode.currentViewMode() == MODE_FOLDERVIEW)
    enableDocumentView();
  else
    enableFolderView();
}

void Core::copyFileClipboard() {
  if (model->isEmpty())
    return;

  MimePayloadResult payloadResult = mimePayloadManager->createPayload(
      {model->getImage(selectedPath()), MimePayloadTarget::Clipboard});
  if (!payloadResult.succeeded()) {
    ui.notifications.showError(mimePayloadFailureMessage(payloadResult.error));
    return;
  }

  QMimeData *mimeData = payloadResult.mimeData.release();

  // mimeData->text() should already contain an url
  QByteArray gnomeFormat =
      QByteArray("copy\n").append(QUrl(mimeData->text()).toEncoded());
  mimeData->setData("x-special/gnome-copied-files", gnomeFormat);

  QGuiApplication::clipboard()->setMimeData(mimeData);
  ui.notifications.showMessage(tr("File copied"));
}

void Core::copyPathClipboard() {
  if (model->isEmpty())
    return;
  QGuiApplication::clipboard()->setText(selectedPath());
  ui.notifications.showMessage(tr("Path copied"));
}

// open from clipboard
void Core::openFromClipboard() {
  auto cb = QGuiApplication::clipboard();
  auto mimeData = cb->mimeData();
  if (!mimeData)
    return;
  qDebug() << "=====================================";
  qDebug() << "hasUrls:" << mimeData->hasUrls();
  qDebug() << "hasImage:" << mimeData->hasImage();
  qDebug() << "hasText:" << mimeData->hasText();

  qDebug() << "TEXT:" << cb->text();

  // try opening url
  if (mimeData->hasUrls()) {
    auto url = mimeData->urls().constFirst();
    QString path = url.toLocalFile();
    if (path.isEmpty()) {
      qDebug() << "Could not load url:" << url;
      qDebug() << "Currently only local files are supported.";
    } else if (loadPath(path)) {
      return;
    }
  }
  // try to save buffer image then open
  if (mimeData->hasImage()) {
    auto image = cb->image();
    if (image.isNull())
      return;
    QString destPath;
    if (!model->isEmpty())
      destPath = model->directoryPath() + "/";
    else
      destPath = QDir::homePath() + "/";
    destPath.append("clipboard.png");
    destPath = ui.dialogs.requestSavePath({destPath}).path;
    if (destPath.isEmpty())
      return;

    QFileInfo fi(destPath);
    QString ext = fi.suffix();
    int quality = 95;
    if (ext.compare("png", Qt::CaseInsensitive) == 0)
      quality = settings->pngSaveQuality() * 10;
    else if (ext.compare("jpg", Qt::CaseInsensitive) == 0 ||
             ext.compare("jpeg", Qt::CaseInsensitive) == 0)
      quality = settings->JPEGSaveQuality();
    else if (ext.compare("jxl", Qt::CaseInsensitive) == 0 ||
             ext.compare("webp", Qt::CaseInsensitive) == 0 ||
             ext.compare("avif", Qt::CaseInsensitive) == 0)
      quality = settings->modernSaveQuality();

    const ImageSaveResult saveResult =
        FileOperations::saveImage(image, destPath, quality);
    if (saveResult.succeeded()) {
      loadPath(destPath);
      if (!saveResult.retainedBackupPath.isEmpty())
        ui.notifications.showWarning(retainedBackupWarningMessage(saveResult));
    } else {
      ui.notifications.showError(imageSaveFailureMessage(saveResult));
    }
  }
}

void Core::onDropIn(const QMimeData *mimeData, QObject *source) {
  // ignore self
  if (source == this)
    return;
  // check for our needed mime type, here a file or a list of files
  if (mimeData->hasUrls()) {
    QStringList pathList;
    QList<QUrl> urlList = mimeData->urls();
    // extract the local paths of the files
    for (int i = 0; i < urlList.size(); ++i)
      pathList.append(urlList.at(i).toLocalFile());
    // try to open first file in the list
    loadPath(pathList.constFirst());
  }
}

// drag'n'drop
// drag image out of the program
void Core::onDraggedOut() { onDraggedOut(currentSelection()); }

void Core::onDraggedOut(QList<QString> paths) {
  if (paths.isEmpty())
    return;
  QMimeData *mimeData;
  // single selection, image
  if (paths.count() == 1 && model->containsFile(paths.constFirst())) {
    MimePayloadResult payloadResult = mimePayloadManager->createPayload(
        {model->getImage(paths.constLast()), MimePayloadTarget::Drop});
    if (!payloadResult.succeeded()) {
      ui.notifications.showError(mimePayloadFailureMessage(payloadResult.error));
      return;
    }
    mimeData = payloadResult.mimeData.release();
  } else { // multi-selection, or single directory. drag urls
    mimeData = new QMimeData();
    QList<QUrl> urlList;
    for (const auto &path : std::as_const(paths))
      urlList << QUrl::fromLocalFile(path);
    mimeData->setUrls(urlList);
  }
  // auto thumb = Thumbnailer::getThumbnail(paths.last(), 100);
  mDrag = new QDrag(this);
  mDrag->setMimeData(mimeData);
  // mDrag->setPixmap(*thumb->pixmap().get());
  mDrag->exec(SupportedFileDragActions, Qt::CopyAction);
  mDrag->deleteLater();
  mDrag = nullptr;
}

void Core::sortBy(SortingMode mode) { model->setSortingMode(mode); }

void Core::setFoldersDisplay(bool mode) {
  if (folderViewPresenter.showDirs() != mode)
    folderViewPresenter.setShowDirs(mode);
}

void Core::renameCurrentSelection(QString newName) {
  if (newName.isEmpty() || selectedPath().isEmpty())
    return;
  FileOpResult result;
  model->renameEntry(selectedPath(), newName, false, result);
  if (result == FileOpResult::DESTINATION_DIR_EXISTS) {
    ui.shell.toggleRenamePrompt(newName);
  } else if (result == FileOpResult::DESTINATION_FILE_EXISTS) {
    if (ui.dialogs.confirm({tr("File exists"), tr("Overwrite file?")}).accepted) {
      model->renameEntry(selectedPath(), newName, true, result);
    } else {
      // show rename dialog again
      ui.shell.toggleRenamePrompt(newName);
    }
  }
  outputError(result);
}

FileOpResult Core::removeFile(QString filePath, bool trash) {
  if (model->isEmpty())
    return FileOpResult::NOTHING_TO_DO;

  bool reopen = false;
  std::shared_ptr<Image> img;
  if (state.currentFilePath == filePath) {
    img = model->getImage(filePath);
    if (img->type() == ANIMATED) {
      ui.viewer.closeImage();
      reopen = true;
    }
  }
  FileOpResult result;
  model->removeFile(filePath, trash, result);
  if (result != FileOpResult::SUCCESS && reopen)
    guiSetImage(img);
  return result;
}

void Core::onFileRemoved(QString filePath, int index) {
  // no files left
  if (model->isEmpty()) {
    ui.viewer.closeImage();
    state.hasActiveImage = false;
    state.currentFilePath = "";
  }
  // image mode && removed current file
  if (state.currentFilePath == filePath) {
    if (ui.viewMode.currentViewMode() == MODE_DOCUMENT) {
      if (!loadFileIndex(index, true, settings->usePreloader()))
        loadFileIndex(--index, true, settings->usePreloader());
    } else {
      state.hasActiveImage = false;
      state.currentFilePath = "";
    }
  }
  updateInfoString();
}

void Core::onFileRenamed(QString fromPath, int /*indexFrom*/,
                         QString /*toPath*/, int indexTo) {
  if (state.currentFilePath == fromPath) {
    loadFileIndex(indexTo, true, settings->usePreloader());
  }
}

void Core::onFileAdded(QString filePath) {
  Q_UNUSED(filePath)
  // update file count
  updateInfoString();
  if (model->fileCount() == 1 && state.currentFilePath == "")
    loadFileIndex(0, false, settings->usePreloader());
}

// !! fixme
void Core::onFileModified(QString filePath) { Q_UNUSED(filePath) }

void Core::outputError(const FileOpResult &error) const {
  if (error == FileOpResult::SUCCESS || error == FileOpResult::NOTHING_TO_DO)
    return;
  ui.notifications.showError(FileOperations::decodeResult(error));
  qDebug() << FileOperations::decodeResult(error);
}

void Core::showInDirectory() {
  if (!model)
    return;
  if (selectedPath().isEmpty()) {
    QDesktopServices::openUrl(QUrl::fromLocalFile(model->directoryPath()));
    return;
  }
  QStringList args;
  args << "/select," << QDir::toNativeSeparators(selectedPath());
  QProcess::startDetached("explorer", args);
}

void Core::createDirectory() {
  if (!model)
    return;
  QString currentDirPath = model->directoryPath();
  if (currentDirPath.isEmpty())
    return;

  const TextInputResult input =
      ui.dialogs.requestText({tr("Add folder"), tr("Folder name:"), QString()});
  const QString newFolderName = input.text;
  if (!input.accepted || newFolderName.trimmed().isEmpty())
    return;

  QDir currentDir(currentDirPath);
  if (currentDir.exists(newFolderName)) {
    ui.notifications.showError(tr("Folder already exists"));
    return;
  }

  if (currentDir.mkdir(newFolderName)) {
    QString newDirPath = currentDir.absoluteFilePath(newFolderName);
    model->insertDir(newDirPath);
    ui.shell.refreshFolderTree(currentDirPath);
  } else {
    ui.notifications.showError(tr("Failed to create folder"));
  }
}

void Core::interactiveCopy(QList<QString> paths, QString destDirectory) {
  // Recursion + I/O now happen off the GUI thread; see
  // components/fileoptask/. FileOpController::progress()/finished() report
  // back via onFileOpProgress()/onFileOpFinished().
  if (!fileOpController)
    return;
  fileOpController->startCopy(std::move(paths), std::move(destDirectory));
}

void Core::interactiveMove(QList<QString> paths, QString destDirectory) {
  if (!fileOpController)
    return;
  fileOpController->startMove(std::move(paths), std::move(destDirectory));
}
// -----------------------------------------------------------------------------------

void Core::onFileOpProgress(FileOpProgress progress) {
  const QString fileName = QFileInfo(progress.currentPath).fileName();
  ui.notifications.showMessage(progress.isMove
                       ? tr("Moving: %1 (%2)").arg(fileName).arg(progress.filesDone)
                       : tr("Copying: %1 (%2)").arg(fileName).arg(progress.filesDone));
}

void Core::onFileOpFinished(FileOpSummary summary) {
  if (summary.cancelled || summary.filesProcessed == 0) {
    ui.notifications.hideNotifications();
    return;
  }
  ui.notifications.showSuccess(summary.isMove
                              ? tr("Moved %n file(s)", "", summary.filesProcessed)
                              : tr("Copied %n file(s)", "", summary.filesProcessed));
}

// -----------------------------------------------------------------------------------

void Core::onClearThumbnailCacheRequested() {
  if (!thumbnailer->clearCache()) {
      ui.notifications.showError(tr("Failed to clear thumbnail cache"));
  }
}

// -----------------------------------------------------------------------------------

void Core::copyPathsTo(QList<QString> paths, QString destDirectory) {
  interactiveCopy(paths, destDirectory);
}

void Core::movePathsTo(QList<QString> paths, QString destDirectory) {
  interactiveMove(paths, destDirectory);
}

void Core::onDirectoryPresenterDroppedInto(QList<QString> paths, QString destDirectory, Qt::DropAction action) {
  if (action == Qt::CopyAction) {
    copyPathsTo(paths, destDirectory);
  } else if (action == Qt::MoveAction) {
    movePathsTo(paths, destDirectory);
  }
}

void Core::moveCurrentFile(QString destDirectory) {
  if (model->isEmpty())
    return;
  // pause updates to avoid flicker
  ui.window.setWindowUpdatesSuspended(true);
  // move fails during file playback, so we close it temporarily
  ui.viewer.closeImage();
  FileOpResult result;
  model->moveFileTo(selectedPath(), destDirectory, false, result);
  if (result == FileOpResult::SUCCESS) {
    ui.notifications.showSuccess(tr("File moved."));
  } else if (result == FileOpResult::DESTINATION_FILE_EXISTS) {
    if (ui.dialogs.confirm({tr("File exists"),
                            tr("Destination file exists. Overwrite?")}).accepted)
      model->moveFileTo(selectedPath(), destDirectory, true, result);
  }
  if (result != FileOpResult::SUCCESS) {
    guiSetImage(model->getImage(selectedPath()));
    updateInfoString();
    if (result != FileOpResult::DESTINATION_FILE_EXISTS)
      outputError(result);
  }
  ui.window.setWindowUpdatesSuspended(false);
}

void Core::copyCurrentFile(QString destDirectory) {
  if (model->isEmpty())
    return;
  FileOpResult result;
  model->copyFileTo(selectedPath(), destDirectory, false, result);
  if (result == FileOpResult::SUCCESS) {
    ui.notifications.showSuccess(tr("File copied."));
  } else if (result == FileOpResult::DESTINATION_FILE_EXISTS) {
    if (ui.dialogs.confirm({tr("File exists"),
                            tr("Destination file exists. Overwrite?")}).accepted)
      model->copyFileTo(selectedPath(), destDirectory, true, result);
  }
  if (result != FileOpResult::SUCCESS &&
      result != FileOpResult::DESTINATION_FILE_EXISTS)
    outputError(result);
}

void Core::toggleCropPanel() {
  if (model->isEmpty())
    return;
  if (ui.shell.isCropPanelActive()) {
    ui.shell.toggleCropPanel();
  } else if (state.hasActiveImage) {
    ui.shell.toggleCropPanel();
  }
}

void Core::toggleFullscreenInfoBar() { ui.shell.toggleFullscreenInfoBar(); }

void Core::requestSavePath() {
  if (model->isEmpty())
    return;
  const SavePathResult result = ui.dialogs.requestSavePath({selectedPath()});
  if (result.accepted())
    saveCurrentFileAs(result.path);
}

void Core::showResizeDialog() {
  if (model->isEmpty())
    return;
  auto img = model->getImage(selectedPath());
  if (!img)
    return;
  const std::optional<ResizeRequest> request = ui.dialogs.requestResize(img->size());
  if (request)
    resize(*request);
}

void Core::showBatchConverter() {
  // expandedSelectedPaths() used to run its recursive QDirIterator walk
  // synchronously right here, freezing the UI thread for as long as the
  // selected directories took to walk - the same freeze
  // DirectoryPresenter::onItemActivated()/onOpenSelectedRequested() had
  // before being moved onto DirectoryExpandWorker. Ask the presenter to
  // do the same background scan here instead of blocking; onBatchConverterPathsReady()
  // opens the dialog once expandedSelectedPathsReady() arrives.
  folderViewPresenter.requestExpandedSelectedPathsAsync();
}

void Core::onBatchConverterPathsReady(QList<QString> filePaths, QString defaultOutputDir) {
  // Mirrors the old synchronous showBatchConverter()'s guard: do nothing
  // if the selection expanded to no matching files.
  if (filePaths.isEmpty())
    return;

  QString currentDirPath = model->directoryPath();
  const BatchConversionResult result =
      ui.dialogs.runBatchConverter({filePaths, defaultOutputDir});
  if (result.conversionStarted && !currentDirPath.isEmpty()) {
    model->setDirectory(currentDirPath);
  }
}

// ---------------------------------------------------------------- image
// operations

std::shared_ptr<ImageStatic> Core::getEditableImage(const QString &filePath) {
  return std::dynamic_pointer_cast<ImageStatic>(model->getImage(filePath));
}

template <typename... Args>
void Core::edit_template(
    bool save, QString action,
    const std::function<QImage(std::shared_ptr<const QImage>, Args...)>
        &editFunc,
    Args &&...as) {
  if (model->isEmpty())
    return;
  if (save && !ui.dialogs.confirm({
                  action, tr("Perform action \"") + action + "\"? \n\n" +
                              tr("Changes will be saved immediately.")}).accepted)
    return;
  const auto selection = currentSelection();
  for (const auto &path : selection) {
    auto img = getEditableImage(path);
    if (!img)
      continue;
    img->setEditedImage(std::make_unique<const QImage>(
        editFunc(img->getImage(), std::forward<Args>(as)...)));
    model->updateImage(path, std::static_pointer_cast<Image>(img));
    if (save) {
      (void)saveFile(path);
      if (state.currentFilePath != path)
        model->unload(path);
    }
  }
  updateInfoString();
}

void Core::flipH() {
  edit_template((ui.viewMode.currentViewMode() == MODE_FOLDERVIEW),
                tr("Flip horizontal"), {ImageLib::flippedH});
}

void Core::flipV() {
  edit_template((ui.viewMode.currentViewMode() == MODE_FOLDERVIEW), tr("Flip vertical"),
                {ImageLib::flippedV});
}

void Core::rotateByDegrees(int degrees) {
  edit_template((ui.viewMode.currentViewMode() == MODE_FOLDERVIEW), tr("Rotate"),
                {ImageLib::rotated}, degrees);
}

void Core::resize(const ResizeRequest &resizeRequest) {
  const QSize size = resizeRequest.size;
  const ScalingFilter filter = resizeRequest.filter;
  if (resizeRequest.useUpscayl) {
    if (model->isEmpty())
      return;

    if (activeAiResizeOperation.has_value()) {
      ui.notifications.showAiUpscale(tr("AI resize is already running."));
      return;
    }

    const auto selection = currentSelection();
    if (selection.isEmpty())
      return;

    if (selection.size() > 1) {
      ui.notifications.showWarning(tr("AI resize supports one image at a time."));
      return;
    }

    const QString path = selection.constFirst();
    auto img = getEditableImage(path);
    if (!img) {
      ui.notifications.showError(tr("Could not resize image."));
      return;
    }

    std::shared_ptr<const QImage> source = img->getImage();
    if (!source || source->isNull()) {
      ui.notifications.showError(tr("Could not resize image."));
      return;
    }

    // Final safety net: AI upscaling only makes sense when the target is
    // actually larger than the source in some dimension. A caller might
    // still pass useUpscayl=true here from a stale/persisted checkbox state
    // for a same-size or downscale request - don't pay for a full AI model
    // pass in that case, just do the regular (much cheaper) CPU resize.
    if (size.width() <= source->width() && size.height() <= source->height()) {
      edit_template(false, tr("Resize"), {ImageLib::scaled}, size, filter);
      return;
    }

    UpscaylResizeRequest request;
    request.path = path;
    request.targetSize = size;
    request.filter = filter;
    request.modelName = resizeRequest.upscaylModel;
    request.sourceImage = source;
    request.generation = ++aiResizeGeneration;

    activeAiResizeOperation = AiResizeOperation{
        request.generation, path, img->contentRevision(), img};
    QGuiApplication::setOverrideCursor(Qt::WaitCursor);
    aiResizeBusyUiActive = true;
    ui.notifications.showAiUpscale(tr("AI resizing..."), 3600000);

    auto task = new UpscaylResizeRunnable(request);
    task->setAutoDelete(false);
    connect(task, &UpscaylResizeRunnable::finished, task, &QObject::deleteLater, Qt::QueuedConnection);
    connect(task, &UpscaylResizeRunnable::finished, this, &Core::onAiResizeFinished, Qt::QueuedConnection);
    QThreadPool::globalInstance()->start(task);
    return;
  }

  edit_template(false, tr("Resize"), {ImageLib::scaled}, size, filter);
}

void Core::clearAiResizeBusyUi() {
  if (!aiResizeBusyUiActive)
    return;

  aiResizeBusyUiActive = false;
  QGuiApplication::restoreOverrideCursor();
  ui.notifications.hideNotifications();
}

void Core::onAiResizeFinished(int generation, QString path, QImage image, bool success, QString error) {
  if (!activeAiResizeOperation.has_value() ||
      generation != activeAiResizeOperation->generation)
    return;

  const AiResizeOperation operation = *activeAiResizeOperation;
  activeAiResizeOperation.reset();
  clearAiResizeBusyUi();

  if (generation != aiResizeGeneration || path != operation.path)
    return;

  if (!success || image.isNull()) {
    ui.notifications.showError(error.isEmpty() ? tr("AI resize failed.") : error);
    return;
  }

  if (!model->containsFile(path)) {
    ui.notifications.showWarning(tr("AI resize finished, but the image is no longer in the list."));
    return;
  }

  auto img = getEditableImage(path);
  if (!img) {
    ui.notifications.showError(tr("Could not apply AI resize."));
    return;
  }

  const std::shared_ptr<ImageStatic> sourceImage = operation.sourceImage.lock();
  if (!sourceImage || img != sourceImage ||
      img->contentRevision() != operation.sourceRevision) {
    ui.notifications.showWarning(tr("AI resize finished, but the image has changed."));
    return;
  }

  img->setEditedImage(std::make_unique<const QImage>(std::move(image)));
  model->updateImage(path, std::static_pointer_cast<Image>(img));

  if (state.hasActiveImage && path == state.currentFilePath) {
    updateInfoString();
    ui.notifications.showSuccess(tr("AI resize finished."));
  } else {
    ui.notifications.showSuccess(tr("AI resize finished for %1.").arg(QFileInfo(path).fileName()));
  }
}

void Core::crop(QRect rect) {
  if (ui.viewMode.currentViewMode() == MODE_FOLDERVIEW)
    return;
  edit_template(false, tr("Crop"), {ImageLib::cropped}, rect);
}

void Core::cropAndSave(QRect rect) {
  if (ui.viewMode.currentViewMode() == MODE_FOLDERVIEW)
    return;
  edit_template(false, tr("Crop"), {ImageLib::cropped}, rect);
  (void)saveFile(selectedPath());
  updateInfoString();
}

void Core::applyColorAdjustments(const ColorAdjustments &adjustments) {
  if (model->isEmpty())
    return;

  QString path = state.currentFilePath;
  auto img = getEditableImage(path);
  if (!img)
    return;

  if (!adjustments.hasAdjustments())
    return;

  QImage adjusted = ImageLib::applyColorAdjustments(img->getImage(), adjustments);
  if (adjusted.isNull())
    return;

  img->setEditedImage(std::make_unique<const QImage>(adjusted));
  model->updateImage(path, std::static_pointer_cast<Image>(img));
  updateInfoString();
}

// ---------------------------------------------------------------- image
// operations ^

ImageSaveResult Core::saveFile(const QString &filePath) {
  return saveFile(filePath, filePath);
}

ImageSaveResult Core::saveFile(const QString &filePath, const QString &newPath) {
  const ImageSaveResult saveResult = model->saveFile(filePath, newPath);
  if (!saveResult.succeeded()) {
    ui.notifications.showError(imageSaveFailureMessage(saveResult));
    return saveResult;
  }
  if (!saveResult.retainedBackupPath.isEmpty())
    ui.notifications.showWarning(retainedBackupWarningMessage(saveResult));
  ui.shell.setSaveOverlayVisible(false);
  // switch to the new file
  if (model->containsFile(newPath) && state.currentFilePath != newPath) {
    discardEdits();
    if (ui.viewMode.currentViewMode() == MODE_DOCUMENT)
      loadPath(newPath);
  }
  return saveResult;
}

void Core::saveCurrentFile() { saveCurrentFileAs(selectedPath()); }

void Core::saveCurrentFileAs(QString destPath) {
  if (model->isEmpty())
    return;
  const ImageSaveResult saveResult = saveFile(selectedPath(), destPath);
  if (saveResult.succeeded()) {
    if (saveResult.retainedBackupPath.isEmpty())
      ui.notifications.showSuccess(tr("File saved"));
    updateInfoString();
  }
}

void Core::discardEdits() {
  if (model->isEmpty())
    return;

  std::shared_ptr<Image> img = model->getImage(selectedPath());
  if (img && img->type() == STATIC) {
    auto imgStatic = dynamic_cast<ImageStatic *>(img.get());
    imgStatic->discardEditedImage();
    model->updateImage(selectedPath(), img);
  }
  ui.shell.setSaveOverlayVisible(false);
}

QString Core::selectedPath() {
  if (!model)
    return "";
  else if (ui.viewMode.currentViewMode() == MODE_FOLDERVIEW) {
    auto paths = folderViewPresenter.selectedPaths();
    return paths.isEmpty() ? QString() : paths.constLast();
  }
  else
    return state.currentFilePath;
}

QList<QString> Core::currentSelection() {
  if (!model)
    return QList<QString>();
  else if (ui.viewMode.currentViewMode() == MODE_FOLDERVIEW)
    return folderViewPresenter.selectedPaths();
  else
    return QList<QString>() << state.currentFilePath;
}

//------------------------

void Core::sortByName() {
  auto mode = SortingMode::SORT_NAME;
  if (model->sortingMode() == mode)
    mode = SortingMode::SORT_NAME_DESC;
  model->setSortingMode(mode);
}

void Core::sortByTime() {
  auto mode = SortingMode::SORT_TIME;
  if (model->sortingMode() == mode)
    mode = SortingMode::SORT_TIME_DESC;
  model->setSortingMode(mode);
}

void Core::sortBySize() {
  auto mode = SortingMode::SORT_SIZE;
  if (model->sortingMode() == mode)
    mode = SortingMode::SORT_SIZE_DESC;
  model->setSortingMode(mode);
}

void Core::showRenameDialog() {
  if (selectedPath().isEmpty())
    return;
  QFileInfo fi(selectedPath());
  ui.shell.toggleRenamePrompt(fi.fileName());
}

void Core::runScript(const QString &scriptName) {
  if (model->isEmpty())
    return;
  scriptManager->runScript(scriptName, model->getImage(selectedPath()));
}

void Core::setWallpaper() {
  if (model->isEmpty() || selectedPath().isEmpty())
    return;
  auto img = model->getImage(selectedPath());
  if (!img || img->type() != DocumentType::STATIC) {
    ui.notifications.showMessage(tr("Set wallpaper: file not supported"));
    return;
  }

  auto imgStatic = std::dynamic_pointer_cast<ImageStatic>(img);
  if (!imgStatic) {
    ui.notifications.showMessage(tr("Set wallpaper: file not supported"));
    return;
  }

  auto sourceImage = imgStatic->getImage();
  if (!sourceImage || sourceImage->isNull()) {
    ui.notifications.showMessage(tr("Set wallpaper: failed to get image"));
    return;
  }

  wallpaperController->setWallpaper(sourceImage);
}

void Core::print() {
  if (model->isEmpty())
    return;
  auto img = model->getImage(selectedPath());
  if (!img) {
    ui.notifications.showError(tr("Could not open image"));
    return;
  }
  if (img->type() != DocumentType::STATIC) {
    ui.notifications.showError(tr("Can only print static images"));
    return;
  }
  QString pdfPath = model->directoryPath() + "/" + img->baseName() + ".pdf";
  ui.dialogs.print({img->getImage(), pdfPath});
}

void Core::scalingRequest(QSize size, ScalingFilter filter) {
  // filter out an unnecessary scale request at statup
  if (ui.window.isWindowVisible() && state.hasActiveImage) {
    std::shared_ptr<Image> forScale = model->getImage(state.currentFilePath);
    if (forScale) {
      model->requestScaled(
          ScalerRequest(forScale, size, state.currentFilePath, filter));
    }
  }
}

void Core::onScalingFinished(QImage scaled, ScalerRequest req) {
  if (state.hasActiveImage && req.path == state.currentFilePath) {
    ui.viewer.showScaledImage(scaled);
    updateUpscale(req.image, req.size, req.path);
  }
}

// The Qt Quick viewer scales on the GPU and asks for the upscale directly,
// without a CPU-scaled copy.
void Core::onUpscaleRequested(QSize size) {
  if (!ui.window.isWindowVisible() || !state.hasActiveImage)
    return;
  std::shared_ptr<Image> image = model->getImage(state.currentFilePath);
  if (image)
    updateUpscale(image, size, state.currentFilePath);
}

// size: the size the image is displayed at, device pixels.
void Core::updateUpscale(const std::shared_ptr<Image> &image, QSize size,
                         const QString &path) {
  constexpr float kPercent = 100.0f;
  const UpscaleInputs inputs{
      .panoramaMode = ui.viewer.panoramaMode(),
      .useUpscayl = settings->useUpscayl(),
      .staticImage = image && image->type() == DocumentType::STATIC,
      .limitEnabled = settings->upscaylLimitEnabled(),
      .limitPercent = settings->upscaylLimitValue(),
      .zoomPercent = ui.viewer.currentScale() * kPercent,
      .displayedWidth = size.width(),
      .imageWidth = image ? image->width() : 0,
  };
  switch (decideUpscale(inputs)) {
  case UpscaleAction::HideCropAndReset:
    ui.viewer.hideUpscaledCrop();
    upscaler->reset();
    break;
  case UpscaleAction::HideCrop:
    ui.viewer.hideUpscaledCrop();
    break;
  case UpscaleAction::Request:
    upscaler->requestUpscale(image, size, path);
    break;
  case UpscaleAction::InvalidatePreview:
    upscaler->invalidatePreview();
    break;
  case UpscaleAction::None:
    break;
  }
}


// reset state; clear cache; etc
void Core::reset() {
  upscaler->reset();
  aiResizeGeneration++;
  clearAiResizeBusyUi();
  state.hasActiveImage = false;
  state.currentFilePath = "";
  state.directoryPath = "";
  state.currentImg.reset();
  autoPageHintShown.clear();
  m_pendingDocumentLoad = PendingDocumentLoad::None;
  model->clearScaler();
  model->setDirectory("");
}

bool Core::loadPath(QString path) {
  upscaler->reset();
  if (path.isEmpty())
    return false;
  if (path.startsWith("file://", Qt::CaseInsensitive))
    path.remove(0, 7);

  stopSlideshow();
  state.delayModel = false;
  QFileInfo fileInfo(path);
  if (fileInfo.isDir()) {
    state.directoryPath = QDir(path).absolutePath();
  } else if (fileInfo.isFile()) {
    state.directoryPath = fileInfo.absolutePath();
    if (model->source() == SOURCE_LIST && model->containsFile(path)) {
      // already in the list, keep SOURCE_LIST
    } else if (model->directoryPath() != state.directoryPath) {
      state.delayModel = true;
    }
  } else {
    ui.notifications.showError(tr("Could not open path: ") + path);
    qDebug() << "Could not open path: " << path;
    return false;
  }
  
  bool skipSetDir = (model->source() == SOURCE_LIST && model->containsFile(path));
  if (!skipSetDir && !state.delayModel && !setDirectory(state.directoryPath))
    return false;

  // load file / folderview
  bool success = false;
  if (fileInfo.isFile()) {
    int index = model->indexOfFile(fileInfo.absoluteFilePath());
    // DirectoryManager only checks file extensions via regex (performance
    // reasons) But in this case we force check mimetype.
    // If the file index is not found (e.g. delayed loading), check against
    // supported regex. Falls back to QMimeDatabase query if regex check is not
    // matched.
    if (index == -1) {
      bool isSupported = false;
      QRegularExpression re(settings->supportedFormatsRegex(),
                            QRegularExpression::CaseInsensitiveOption);
      if (re.match(fileInfo.fileName()).hasMatch()) {
        isSupported = true;
      } else {
        QStringList types = settings->supportedMimeTypes();
        static QMimeDatabase db;
        QMimeType type = db.mimeTypeForFile(fileInfo.absoluteFilePath());
        if (types.contains(type.name())) {
          isSupported = true;
        }
      }

      if (isSupported) {
        if (model->forceInsert(fileInfo.absoluteFilePath())) {
          index = model->indexOfFile(fileInfo.absoluteFilePath());
        }
      }
    }
    ui.viewMode.enableDocumentView();
    success = loadFileIndex(index, false, settings->usePreloader());
  } else {
    if (ui.viewMode.currentViewMode() == MODE_DOCUMENT && model->fileCount() > 0) {
      success = loadFileIndex(0, false, settings->usePreloader());
    } else {
      ui.viewMode.enableFolderView();
      success = true;
    }
  }
  if (success && settings->rememberLastFolder()) {
    settings->setLastFolder(state.directoryPath);
  }
  return success;
}

bool Core::setDirectory(QString path) {
  if (model->directoryPath() != path) {
    if (!blockHistory && !model->directoryPath().isEmpty()) {
      backHistory.append(model->directoryPath());
      if (backHistory.count() > 100)
        backHistory.removeFirst();
      forwardHistory.clear();
    }
    this->reset();
    if (!model->setDirectory(path)) {
      ui.notifications.showError(tr("Could not load folder: ") + path);
      return false;
    }
    ui.shell.setDirectoryPath(path);
    state.directoryPath = path;
  }
  return true;
}

bool Core::loadFileIndex(int index, bool async, bool preload) {
  if (!model)
    return false;
  auto entry = model->fileEntryAt(index);
  if (entry.path.isEmpty())
    return false;
  state.currentFilePath = entry.path;
  preloadTimer.stop();
  model->unloadExcept(entry.path, preload);
  model->load(entry.path, async);
  thumbPanelPresenter.selectAndFocus(entry.path);
  folderViewPresenter.selectAndFocus(entry.path);
  updateInfoString();
  return true;
}

void Core::loadParentDir() {
  if (ui.viewMode.currentViewMode() != MODE_FOLDERVIEW)
    return;
  if (model->directoryPath().isEmpty()) {
      if (model->source() == SOURCE_LIST) {
          if (!backHistory.isEmpty()) {
              historyBack();
          } else if (model->fileCount() > 0) {
              QString path = model->filePathAt(0);
              loadPath(QFileInfo(path).absolutePath());
          }
      }
      return;
  }
  stopSlideshow();
  QFileInfo currentDir(model->directoryPath());
  QFileInfo parentDir(currentDir.absolutePath());
  if (parentDir.exists() && parentDir.isReadable()) {
    // Directory scanning is asynchronous, so the parent directory isn't
    // loaded yet at this point - selecting it now would no-op. Defer to
    // onModelLoaded() instead (see pendingFolderViewSelectPath).
    pendingFolderViewSelectPath = currentDir.absoluteFilePath();
    loadPath(parentDir.absoluteFilePath());
  }
}

void Core::nextDirectory() {
  if (model->directoryPath().isEmpty() ||
      ui.viewMode.currentViewMode() != MODE_DOCUMENT ||
      model->loaderBusy())
    return;
  stopSlideshow();
  auto nextResult = model->nextSiblingDir(model->directoryPath());
  if (!nextResult) {
    ui.notifications.showError(tr("Could not enumerate sibling folders: ") +
                  QString::fromStdString(nextResult.error().message()));
    return;
  }
  QString next = *nextResult;
  if (!next.isEmpty()) {
    if (!setDirectory(next))
      return;
    m_pendingDocumentLoad = PendingDocumentLoad::FirstImage;
    QFileInfo fi(next);
    ui.notifications.showDirectory(fi.baseName());
    if (settings->rememberLastFolder())
      settings->setLastFolder(next);
  } else {
    ui.notifications.showDirectoryEnd();
  }
}

void Core::prevDirectory(bool selectLast) {
  if (model->directoryPath().isEmpty() ||
      ui.viewMode.currentViewMode() != MODE_DOCUMENT ||
      model->loaderBusy())
    return;
  stopSlideshow();
  auto prevResult = model->prevSiblingDir(model->directoryPath());
  if (!prevResult) {
    ui.notifications.showError(tr("Could not enumerate sibling folders: ") +
                  QString::fromStdString(prevResult.error().message()));
    return;
  }
  QString prev = *prevResult;
  if (!prev.isEmpty()) {
    if (!setDirectory(prev))
      return;
    m_pendingDocumentLoad = selectLast ? PendingDocumentLoad::LastImage : PendingDocumentLoad::FirstImage;
    QFileInfo fi(prev);
    ui.notifications.showDirectory(fi.baseName());
    if (settings->rememberLastFolder())
      settings->setLastFolder(prev);
  } else {
    ui.notifications.showDirectoryStart();
  }
}

void Core::prevDirectory() { prevDirectory(false); }

void Core::historyBack() {
  if (!backHistory.isEmpty()) {
    QString childPath = model->directoryPath();
    QString path = backHistory.takeLast();
    forwardHistory.append(childPath);
    blockHistory = true;
    // Restore selection/scroll position to the folder we just left once the
    // parent directory has actually finished loading (see
    // pendingFolderViewSelectPath) - otherwise the view jumps back to the
    // top instead of where we were.
    if (!childPath.isEmpty())
      pendingFolderViewSelectPath = childPath;
    // When in document mode (thumbnail panel with subfolders), stay in
    // document mode instead of switching to folder view.  loadPath() would
    // see fileCount()==0 (async scan) and flip to folder view.
    if (ui.viewMode.currentViewMode() == MODE_DOCUMENT && QFileInfo(path).isDir()) {
      stopSlideshow();
      setDirectory(path);
      m_pendingDocumentLoad = PendingDocumentLoad::FirstImage;
      if (settings->rememberLastFolder())
        settings->setLastFolder(path);
    } else {
      loadPath(path);
    }
    blockHistory = false;
  } else {
    loadParentDir();
  }
}

void Core::historyForward() {
  if (!forwardHistory.isEmpty()) {
    QString path = forwardHistory.takeLast();
    backHistory.append(model->directoryPath());
    blockHistory = true;
    if (ui.viewMode.currentViewMode() == MODE_DOCUMENT && QFileInfo(path).isDir()) {
      stopSlideshow();
      setDirectory(path);
      m_pendingDocumentLoad = PendingDocumentLoad::FirstImage;
      if (settings->rememberLastFolder())
        settings->setLastFolder(path);
    } else {
      loadPath(path);
    }
    blockHistory = false;
  }
}

void Core::nextImage() {
  if (ui.viewMode.currentViewMode() == MODE_FOLDERVIEW) {
    historyForward();
    return;
  }

  if ((model->isEmpty() && folderEndAction != FOLDER_END_GOTO_ADJACENT))
    return;
  stopSlideshow();
  if (shuffle) {
    loadFileIndex(randomizer.next(), true, false);
    return;
  }
  int newIndex = model->indexOfFile(state.currentFilePath) + 1;
  if (newIndex >= model->fileCount()) {
    if (folderEndAction == FOLDER_END_LOOP) {
      newIndex = 0;
    } else if (folderEndAction == FOLDER_END_GOTO_ADJACENT) {
      nextDirectory();
      return;
    } else {
      if (!model->loaderBusy())
        ui.notifications.showDirectoryEnd();
      return;
    }
  }
  loadFileIndex(newIndex, true, settings->usePreloader());
}

void Core::prevImage() {
  if (ui.viewMode.currentViewMode() == MODE_FOLDERVIEW) {
    historyBack();
    return;
  }

  if ((model->isEmpty() && folderEndAction != FOLDER_END_GOTO_ADJACENT))
    return;
  stopSlideshow();
  if (shuffle) {
    loadFileIndex(randomizer.prev(), true, false);
    return;
  }

  int newIndex = model->indexOfFile(state.currentFilePath) - 1;
  if (newIndex < 0) {
    if (folderEndAction == FOLDER_END_LOOP) {
      newIndex = model->fileCount() - 1;
    } else if (folderEndAction == FOLDER_END_GOTO_ADJACENT) {
      prevDirectory(true);
      return;
    } else {
      if (!model->loaderBusy())
        ui.notifications.showDirectoryStart();
      return;
    }
  }
  loadFileIndex(newIndex, true, settings->usePreloader());
}

void Core::nextImageSlideshow() {
  if (model->isEmpty() || ui.viewMode.currentViewMode() == MODE_FOLDERVIEW)
    return;
  if (shuffle) {
    loadFileIndex(randomizer.next(), false, false);
  } else {
    int newIndex = model->indexOfFile(state.currentFilePath) + 1;
    if (newIndex >= model->fileCount()) {
      if (loopSlideshow) {
        newIndex = 0;
      } else {
        stopSlideshow();
        ui.notifications.showMessage(tr("End of directory."));
        return;
      }
    }
    loadFileIndex(newIndex, false, true);
  }
  startSlideshowTimer();
}

void Core::startSlideshowTimer() {
  // start timer only for static images or single frame gifs
  auto img = model->getImage(state.currentFilePath);
  if (img && (img->type() == STATIC || img->frameCount() <= 1)) {
    slideshowTimer.start();
  }
}

void Core::jumpToFirst() {
  if (model->isEmpty())
    return;
  stopSlideshow();
  loadFileIndex(0, true, settings->usePreloader());
  ui.notifications.showDirectoryStart();
}

void Core::jumpToLast() {
  if (model->isEmpty())
    return;
  stopSlideshow();
  loadFileIndex(model->fileCount() - 1, true, settings->usePreloader());
  ui.notifications.showDirectoryEnd();
}

void Core::onLoadFailed(const QString &path) {
  ui.notifications.showMessage(tr("Load failed: ") + path);
  model->clearScaler();
  if (path == state.currentFilePath)
    ui.viewer.closeImage();
}

void Core::onModelItemReady(std::shared_ptr<Image> img, const QString &path) {
  if (path == state.currentFilePath) {
    state.currentImg = img;
    guiSetImage(img);
    maybeShowPageHint(img);
    updateInfoString();
    if (state.delayModel) {
      this->showGui();
      state.delayModel = false;
      QTimer::singleShot(40, this, SLOT(modelDelayLoad()));
    }
    model->unloadExcept(state.currentFilePath, settings->usePreloader());
    if (settings->usePreloader()) {
      preloadTimer.start(PreloadDebounceDelayMs);
    }
  }
}

void Core::modelDelayLoad() {
  model->clearScaler();
  model->setDirectory(state.directoryPath);
  ui.shell.setDirectoryPath(state.directoryPath);
  pendingModelImageSync = true;
}

void Core::preloadNeighbors() {
  if (state.hasActiveImage && !state.currentFilePath.isEmpty()) {
    model->preload(model->nextOf(state.currentFilePath));
    model->preload(model->prevOf(state.currentFilePath));
  }
}

void Core::onModelItemUpdated(QString filePath) {
  if (filePath == state.currentFilePath) {
    guiSetImage(model->getImage(filePath));
    updateInfoString();
  }
}

void Core::onModelSortingChanged(SortingMode mode) {
  settings->setSortingMode(mode);
  ui.shell.notifySortingChanged(mode);
  thumbPanelPresenter.reloadModel();
  thumbPanelPresenter.selectAndFocus(state.currentFilePath);
  folderViewPresenter.reloadModel();
  folderViewPresenter.selectAndFocus(state.currentFilePath);
}

void Core::onFolderSortingSelected(SortingMode mode) {
  settings->setFolderIconSortingMode(mode);
  ui.shell.notifyFolderSortingChanged(mode);
  folderViewPresenter.reloadModel();
}

void Core::onFormatFilterSelected(QStringList extensions) {
  settings->setFormatFilter(extensions);
  model->setFormatFilter(extensions);
}

void Core::onNameFilterSelected(QString nameFilter) {
  model->setNameFilter(std::move(nameFilter));
}

void Core::guiSetImage(std::shared_ptr<Image> img) {
  state.hasActiveImage = true;
  if (!img) {
    ui.notifications.showMessage(tr("Error: could not load image."));
    return;
  }
  DocumentType type = img->type();
  if (type == STATIC) {
    auto displayImage = img->getDisplayImage();
    if (!displayImage) {
      ui.notifications.showMessage(tr("Error: could not load image."));
      return;
    }
    ui.viewer.showImage(displayImage, img->filePath());
  } else if (type == ANIMATED) {
    ui.viewer.showAnimation(img->filePath(), img->format(), img->size());
  }
  img->isEdited() ? ui.shell.setSaveOverlayVisible(true) : ui.shell.setSaveOverlayVisible(false);

  // EXIF tags don't have a meaningful display order (alphabetical is fine),
  // but generation info does — QList<QPair<>> is used there instead of
  // QMap specifically to keep that order intact through to the UI.
  QList<QPair<QString, QString>> info;
  auto qimg = img->getDecodedImage();
  if (qimg) {
    QString hdrProfile = HdrToneMapper::detectHdrProfile(*qimg);
    if (!hdrProfile.isEmpty()) {
      info.append({ tr("HDR Profile"), hdrProfile });
      QString maxCll = qimg->text(QStringLiteral("HDR_MaxCLL"));
      QString maxPall = qimg->text(QStringLiteral("HDR_MaxPALL"));
      if (!maxCll.isEmpty() || !maxPall.isEmpty()) {
        QString clli = QStringLiteral("%1 / %2 nits").arg(maxCll.isEmpty() ? QStringLiteral("-") : maxCll,
                                                           maxPall.isEmpty() ? QStringLiteral("-") : maxPall);
        info.append({ tr("MaxCLL / MaxFALL"), clli });
      }
    }
  }
  const QMap<QString, QString> exifTags = img->getExifTags();
  for (auto it = exifTags.constBegin(); it != exifTags.constEnd(); ++it)
    info.append({ it.key(), it.value() });
  info.append(img->getGenerationInfo());
  ui.shell.setMetadata(info);
}

// Shows a one-time "this document has multiple pages" hint the first time
// browsing lands on a multi-page static document (PDF/TIFF, etc.) during
// the current folder visit. Explicit page turns are handled separately by
// showPageChangeMessage(), called directly from nextPage()/prevPage() —
// see its comment for why no shared flag is needed to tell the two apart.
// Animated formats are excluded: their frameCount() reports animation
// frames, not document pages, and isn't relevant to this notification.
void Core::maybeShowPageHint(const std::shared_ptr<Image> &img) {
  if (!img || img->type() != STATIC || img->frameCount() <= 1)
    return;

  QString path = img->filePath();
  if (autoPageHintShown.contains(path))
    return;
  autoPageHintShown.insert(path);

  int page = ImageStatic::pageOverrideForPath(path) + 1;
  ui.notifications.showMessage(tr("Page %1/%2").arg(page).arg(img->frameCount()),
                   PageChangeMessageDurationMs);
}

// Shows "Page N/M" for `path` right after an explicit page-turn hotkey.
// Callers only reach this after DirectoryModel::reload() has returned true,
// confirming a fresh image was actually loaded -- not just that the target
// page number was stored by ImageStatic. DirectoryModel::reload()
// performs its load synchronously and emits imageReady() through a same-thread
// direct connection, so by the time reload(path) returns in nextPage()/prevPage(),
// state.currentImg already reflects the freshly reloaded page. This lets us
// report the result directly instead of correlating it via a shared flag
// across two events, which sidesteps any risk of that flag being consumed
// by an unrelated event.
// NOTE: this relies on reload() staying synchronous; if it's ever made
// asynchronous, this call must move to a completion callback instead.
void Core::showPageChangeMessage(const QString &path) {
  if (state.currentFilePath != path || !state.currentImg)
    return;
  if (state.currentImg->type() != STATIC || state.currentImg->frameCount() <= 1)
    return;

  int page = ImageStatic::pageOverrideForPath(path) + 1;
  ui.notifications.showMessage(tr("Page %1/%2").arg(page).arg(state.currentImg->frameCount()),
                   PageChangeMessageDurationMs);
}

void Core::updateInfoString() {
  QSize imageSize(0, 0);
  qint64 fileSize = 0;
  bool edited = false;
  QString format;
  QString colorProfile;

  if (model->isLoaded(state.currentFilePath)) {
    auto img = model->getImage(state.currentFilePath);
    if (img) {
      imageSize = img->size();
      fileSize = img->fileSize();
      edited = img->isEdited();
      format = img->format();
      auto qimg = img->getDecodedImage();
      if (qimg) {
        QString hdrProfile = HdrToneMapper::detectHdrProfile(*qimg);
        if (!hdrProfile.isEmpty()) {
          colorProfile = hdrProfile;
        } else {
          QColorSpace cs = qimg->colorSpace();
          if (cs.isValid()) {
            QString desc = cs.description();
            if (desc.isEmpty()) {
              if (cs == QColorSpace(QColorSpace::SRgb)) {
                desc = "sRGB";
              } else if (cs == QColorSpace(QColorSpace::SRgbLinear)) {
                desc = "Linear sRGB";
              } else if (cs == QColorSpace(QColorSpace::AdobeRgb)) {
                desc = "Adobe RGB";
              } else if (cs == QColorSpace(QColorSpace::DisplayP3)) {
                desc = "Display P3";
#if QT_VERSION >= QT_VERSION_CHECK(6, 1, 0)
              } else if (cs == QColorSpace(QColorSpace::ProPhotoRgb)) {
                desc = "ProPhoto RGB";
              } else if (cs == QColorSpace(QColorSpace::Bt2020)) {
                desc = "BT.2020";
#endif
              } else {
                desc = "Custom";
              }
            }
            colorProfile = desc;
          }
        }
      }
    }
  }
  int index = model->indexOfFile(state.currentFilePath);
  ShellFileInfo info;
  info.index = index;
  info.fileCount = model->fileCount();
  info.filePath = model->filePathAt(index);
  info.fileName = model->fileNameAt(index);
  info.imageSize = imageSize;
  info.fileSize = fileSize;
  info.format = format;
  info.colorProfile = colorProfile;
  info.slideshow = slideshow;
  info.shuffle = shuffle;
  info.edited = edited;
  ui.shell.setCurrentInfo(info);
}

void Core::suspendToStandby() {
    m_resumeFromStandby = true;
    m_lastViewMode = ui.viewMode.currentViewMode();
    stopSlideshow();
    preloadTimer.stop();
    ui.viewer.closeImage();
    this->reset();
    ui.shell.setDirectoryPath("");

    ui.window.saveWindowGeometry();
    ui.window.hideWindow();
    EmptyWorkingSet(GetCurrentProcess());
}

bool Core::hasActiveState() const {
    return state.hasActiveImage || !state.directoryPath.isEmpty();
}

void Core::loadDefaultPath() {
    if (settings->rememberLastFolder() && !settings->lastFolder().isEmpty() && QFileInfo(settings->lastFolder()).exists()) {
        loadPath(settings->lastFolder());
    } else if (settings->defaultViewMode() == MODE_FOLDERVIEW) {
        QStringList bookmarks = settings->bookmarks();
        if (!bookmarks.isEmpty() && QFileInfo(bookmarks.first()).exists()) {
            loadPath(bookmarks.first());
        } else {
            loadPath(QDir::homePath());
        }
    }
}

void Core::forceExit() {
    QCoreApplication::quit();
}
