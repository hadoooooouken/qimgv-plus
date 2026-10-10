#include "quickfolderviewactions.h"

#include <QDir>
#include <QMimeData>

#include "components/actionmanager/actionmanager.h"
#include "gui/folderview/filesystemmodelcustom.h"
#include "gui/ports/uievents.h"
#include "gui/quick/adapters/directoryviewadapter.h"
#include "gui/quick/ui/folderview/foldergridcontroller.h"
#include "gui/quick/ui/folderview/folderviewcontroller.h"
#include "settings.h"

QuickFolderViewActions::QuickFolderViewActions(const QuickFolderViewContext &context,
                                               QObject *parent)
    : QObject(parent),
      folderView(context.folderView),
      grid(context.grid),
      gridView(context.gridView),
      events(context.events),
      settings(context.settings),
      actions(context.actions) {
    connect(&folderView, &FolderViewController::folderTreeRequested, this,
            &QuickFolderViewActions::createFolderTree);
    forwardIntents();
    forwardGridRequests();
    storeLayout();
}

// The tree model is handed to QML through the controller, so the controller
// lets go of it before it is destroyed.
QuickFolderViewActions::~QuickFolderViewActions() {
    folderView.setFolderTree(nullptr);
}

// FolderView's constructor: folders only, watched from the root.
void QuickFolderViewActions::createFolderTree() {
    if (folderTree)
        return;
    folderTree = std::make_unique<FileSystemModelCustom>();
    folderTree->setFilter(QDir::NoDotAndDotDot | QDir::AllDirs);
    folderTree->setRootPath(QString());
    folderView.setFolderTree(folderTree.get());
}

// FolderView::refreshFilesystemModel().
void QuickFolderViewActions::refreshFolderTree(const QString &directoryPath) {
    // Before the first activation there is no tree to re-read; it lists the
    // current state when it is created.
    if (!folderTree)
        return;
    folderView.beginFolderTreeRefresh(directoryPath);
    folderTree->refreshPath(directoryPath);
    folderView.endFolderTreeRefresh();
}

// The SettingsEnums values mirror the settings enums one to one.
void QuickFolderViewActions::forwardIntents() {
    UiEvents *uiEvents = &events;
    connect(&folderView, &FolderViewController::sortingSelected, uiEvents,
            [uiEvents](SettingsEnums::SortingMode mode) {
                emit uiEvents->sortingSelected(static_cast<SortingMode>(mode));
            });
    connect(&folderView, &FolderViewController::folderSortingSelected, uiEvents,
            [uiEvents](SettingsEnums::SortingMode mode) {
                emit uiEvents->folderSortingSelected(static_cast<SortingMode>(mode));
            });
    connect(&folderView, &FolderViewController::formatFilterSelected, uiEvents,
            &UiEvents::formatFilterSelected);
    connect(&folderView, &FolderViewController::nameFilterSelected, uiEvents,
            &UiEvents::nameFilterSelected);
    connect(&folderView, &FolderViewController::directorySelected, uiEvents,
            &UiEvents::pathOpened);
    connect(&folderView, &FolderViewController::copyUrlsRequested, uiEvents,
            &UiEvents::copyUrlsRequested);
    connect(&folderView, &FolderViewController::moveUrlsRequested, uiEvents,
            &UiEvents::moveUrlsRequested);
    connect(&grid, &FolderGridController::batchConversionRequested, uiEvents,
            &UiEvents::batchConversionRequested);
    connect(&folderView, &FolderViewController::filesystemViewReady, uiEvents,
            &UiEvents::filesystemViewReady);
    connect(&gridView, &ThumbnailListModel::visibleThumbnailsReady, uiEvents,
            &UiEvents::visibleThumbnailsReady);
}

void QuickFolderViewActions::forwardGridRequests() {
    DirectoryViewAdapter *view = &gridView;
    connect(&grid, &FolderGridController::typeAheadRequested, view,
            &DirectoryViewAdapter::typeAheadTextEntered);
    connect(&grid, &FolderGridController::draggedOver, view, &DirectoryViewAdapter::draggedOver);
    connect(&grid, &FolderGridController::openSelectedRequested, view,
            &DirectoryViewAdapter::openSelectedRequested);
    // DirectoryPresenter reads the URLs synchronously, so the payload can
    // live on the stack for the duration of the emission.
    connect(&grid, &FolderGridController::urlsDropped, view,
            [view](const QList<QUrl> &urls, QObject *source, int index, Qt::DropAction action) {
                QMimeData mimeData;
                mimeData.setUrls(urls);
                emit view->droppedInto(&mimeData, source, index, action);
            });
    connect(&grid, &FolderGridController::actionRequested, this,
            [this](const QString &name) { actions.invokeAction(name); });
}

void QuickFolderViewActions::storeLayout() {
    Settings *store = &settings;
    connect(&grid, &FolderGridController::iconSizeCommitted, store,
            &Settings::setFolderViewIconSize);
    connect(&folderView, &FolderViewController::placesPanelEnabledRequested, store,
            &Settings::setPlacesPanel);
    connect(&folderView, &FolderViewController::placesPanelWidthRequested, store,
            &Settings::setPlacesPanelWidth);
    connect(&folderView, &FolderViewController::bookmarksExpandedRequested, store,
            &Settings::setPlacesPanelBookmarksExpanded);
    connect(&folderView, &FolderViewController::treeExpandedRequested, store,
            &Settings::setPlacesPanelTreeExpanded);
    connect(&folderView, &FolderViewController::bookmarksRequested, store,
            &Settings::setBookmarks);
}
