#include "widgetui.h"

#include "components/actionmanager/actionmanager.h"
#include "gui/mainwindow.h"
#include "settings.h"

WidgetUi::WidgetUi()
    : viewModeController(settings->defaultViewMode()),
      window(std::make_unique<MW>()),
      notificationAdapter(*window),
      dialogAdapter(*window),
      viewerAdapter(*window),
      shellAdapter(*window),
      windowAdapter(*window),
      viewerToggleStore(*settings),
      viewerToggles(viewerToggleStore) {
    // The window is revealed by Core (cold start / raise), never implicitly.
    window->hide();
    forwardWindowEvents();
    followViewMode();
    connectWindowActions();
    connectViewerToggles();
}

WidgetUi::~WidgetUi() = default;

UiPorts WidgetUi::ports() {
    return UiPorts{
        notificationAdapter,
        dialogAdapter,
        viewerAdapter,
        shellAdapter,
        windowAdapter,
        viewModeController,
        events,
        window->getThumbnailPanel(),
        window->getFolderView(),
    };
}

void WidgetUi::forwardWindowEvents() {
    MW *mw = window.get();
    UiEvents *uiEvents = &events;
    connect(mw, &MW::opened, uiEvents, &UiEvents::pathOpened);
    connect(mw, &MW::droppedIn, uiEvents, &UiEvents::droppedIn);
    connect(mw, &MW::draggedOut, uiEvents, &UiEvents::draggedOut);
    connect(mw, &MW::nextImageRequested, uiEvents, &UiEvents::nextImageRequested);
    connect(mw, &MW::prevImageRequested, uiEvents, &UiEvents::prevImageRequested);
    connect(mw, &MW::copyRequested, uiEvents, &UiEvents::copyRequested);
    connect(mw, &MW::moveRequested, uiEvents, &UiEvents::moveRequested);
    connect(mw, &MW::copyUrlsRequested, uiEvents, &UiEvents::copyUrlsRequested);
    connect(mw, &MW::moveUrlsRequested, uiEvents, &UiEvents::moveUrlsRequested);
    connect(mw, &MW::renameRequested, uiEvents, &UiEvents::renameRequested);
    connect(mw, &MW::batchRequested, uiEvents, &UiEvents::batchConversionRequested);
    connect(mw, &MW::cropRequested, uiEvents, &UiEvents::cropRequested);
    connect(mw, &MW::cropAndSaveRequested, uiEvents, &UiEvents::cropAndSaveRequested);
    connect(mw, &MW::colorAdjustmentsApplyRequested, uiEvents,
            &UiEvents::colorAdjustmentsApplyRequested);
    connect(mw, &MW::discardEditsRequested, uiEvents, &UiEvents::discardEditsRequested);
    connect(mw, &MW::saveRequested, uiEvents, &UiEvents::saveRequested);
    connect(mw, &MW::saveAsClicked, uiEvents, &UiEvents::saveAsRequested);
    connect(mw, &MW::sortingSelected, uiEvents, &UiEvents::sortingSelected);
    connect(mw, &MW::folderSortingSelected, uiEvents, &UiEvents::folderSortingSelected);
    connect(mw, &MW::formatFilterSelected, uiEvents, &UiEvents::formatFilterSelected);
    connect(mw, &MW::nameFilterSelected, uiEvents, &UiEvents::nameFilterSelected);
    connect(mw, &MW::showFoldersChanged, uiEvents, &UiEvents::showFoldersChanged);
    connect(mw, &MW::scalingRequested, uiEvents, &UiEvents::scalingRequested);
    connect(mw, &MW::clearThumbnailCacheRequested, uiEvents,
            &UiEvents::clearThumbnailCacheRequested);
    connect(mw, &MW::suspendRequested, uiEvents, &UiEvents::suspendRequested);
    connect(mw, &MW::documentRenderingSettled, uiEvents,
            &UiEvents::documentRenderingSettled);

    const std::shared_ptr<FolderViewProxy> folderView = mw->getFolderView();
    connect(folderView.get(), &FolderViewProxy::visibleThumbnailsReady, uiEvents,
            &UiEvents::visibleThumbnailsReady);
    connect(folderView.get(), &FolderViewProxy::filesystemViewReady, uiEvents,
            &UiEvents::filesystemViewReady);
}

void WidgetUi::followViewMode() {
    connect(&viewModeController, &ViewModeController::viewModeApplied,
            window.get(), [mw = window.get()](ViewMode mode) {
        if (mode == MODE_FOLDERVIEW)
            mw->enableFolderView();
        else
            mw->enableDocumentView();
    });
}

void WidgetUi::connectWindowActions() {
    MW *mw = window.get();
    connect(actionManager, &ActionManager::fitWindow, mw, &MW::fitWindow);
    connect(actionManager, &ActionManager::fitWidth, mw, &MW::fitWidth);
    connect(actionManager, &ActionManager::fitNormal, mw, &MW::fitOriginal);
    connect(actionManager, &ActionManager::fitHeight, mw, &MW::fitHeight);
    connect(actionManager, &ActionManager::toggleFitMode, mw, &MW::switchFitMode);
    connect(actionManager, &ActionManager::toggleFullscreen, mw,
            &MW::triggerFullScreen);
    connect(actionManager, &ActionManager::lockZoom, mw, &MW::toggleLockZoom);
    connect(actionManager, &ActionManager::lockView, mw, &MW::toggleLockView);
    connect(actionManager, &ActionManager::zoomIn, mw, &MW::zoomIn);
    connect(actionManager, &ActionManager::zoomOut, mw, &MW::zoomOut);
    connect(actionManager, &ActionManager::zoomInCursor, mw, &MW::zoomInCursor);
    connect(actionManager, &ActionManager::zoomOutCursor, mw, &MW::zoomOutCursor);
    connect(actionManager, &ActionManager::scrollUp, mw, &MW::scrollUp);
    connect(actionManager, &ActionManager::scrollDown, mw, &MW::scrollDown);
    connect(actionManager, &ActionManager::scrollLeft, mw, &MW::scrollLeft);
    connect(actionManager, &ActionManager::scrollRight, mw, &MW::scrollRight);
    connect(actionManager, &ActionManager::openSettings, mw, &MW::showSettings);
    connect(actionManager, &ActionManager::closeFullScreenOrExit, mw,
            &MW::closeFullScreenOrExit);
    connect(actionManager, &ActionManager::copyFile, mw, &MW::triggerCopyOverlay);
    connect(actionManager, &ActionManager::moveFile, mw, &MW::triggerMoveOverlay);
    connect(actionManager, &ActionManager::copyViewportClipboard, mw,
            &MW::copyViewportToClipboard);
    connect(actionManager, &ActionManager::contextMenu, mw, &MW::showContextMenu);
    connect(actionManager, &ActionManager::toggleTransparencyGrid, mw,
            &MW::toggleTransparencyGrid);
    connect(actionManager, &ActionManager::toggleImageInfo, mw,
            &MW::toggleImageInfoOverlay);
    connect(actionManager, &ActionManager::toggleScalingFilter, mw,
            &MW::toggleScalingFilter);
    connect(actionManager, &ActionManager::cycleScalingFilter, mw,
            &MW::cycleScalingFilter);
    connect(actionManager, &ActionManager::togglePanorama, mw, &MW::togglePanorama);
    connect(actionManager, &ActionManager::colorAdjustments, mw,
            &MW::toggleColorAdjustments);
    connect(actionManager, &ActionManager::casSettings, mw, &MW::toggleCasSettings);
}

void WidgetUi::connectViewerToggles() {
    MW *mw = window.get();
    ViewerToggles *toggles = &viewerToggles;
    connect(actionManager, &ActionManager::toggleUpscayl, toggles,
            &ViewerToggles::toggleUpscayl);
    connect(actionManager, &ActionManager::cycleUpscaylModel, toggles,
            &ViewerToggles::cycleUpscaylModel);
    connect(actionManager, &ActionManager::toggleHdrToneMapping, toggles,
            &ViewerToggles::toggleHdrToneMapping);
    connect(toggles, &ViewerToggles::notificationRequested, mw, &MW::showNotification);
    connect(toggles, &ViewerToggles::upscaledCropHideRequested, mw, &MW::hideUpscaledCrop);
}
