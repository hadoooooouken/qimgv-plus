#include "folderviewcontroller.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QFileSystemModel>

namespace {
constexpr int kFirstSortingMode = static_cast<int>(SettingsEnums::SortingMode::Name);
constexpr int kLastSortingMode = static_cast<int>(SettingsEnums::SortingMode::TimeDescending);

// Directories are compared case-insensitively with forward slashes.
QString normalizedDirectory(const QString &path) {
    if (path.isEmpty())
        return {};
    return QDir::cleanPath(QDir::fromNativeSeparators(path)).toCaseFolded();
}

bool isSortingMode(int mode) {
    return mode >= kFirstSortingMode && mode <= kLastSortingMode;
}

// A folder, or a link to one.
bool isFolder(const QFileInfo &info) {
    if (info.isDir())
        return true;
    return info.isSymLink() && QFileInfo(info.symLinkTarget()).isDir();
}
} // namespace

FolderViewController::FolderViewController(FolderGridController &grid,
                                           const UiSettingsSnapshot &settings,
                                           const QString &homePath, QObject *parent)
    : QObject(parent),
      mGrid(grid),
      mBookmarks(homePath),
      mHomePath(homePath),
      mSortingMode(settings.folderView.sortingMode) {
    connect(&mBookmarks, &BookmarksModel::pathsChanged, this,
            &FolderViewController::bookmarksRequested);
    connect(&mFormatFilter, &FormatFilterModel::filterSelected, this,
            &FolderViewController::formatFilterSelected);
    mNameFilterTimer.setSingleShot(true);
    mNameFilterTimer.setInterval(kNameFilterDelayMs);
    connect(&mNameFilterTimer, &QTimer::timeout, this,
            [this]() { emit nameFilterSelected(mNameFilter); });
    applySettings(settings);
}

FolderViewController::~FolderViewController() = default;

FolderGridController *FolderViewController::grid() const {
    return &mGrid;
}

BookmarksModel *FolderViewController::bookmarks() {
    return &mBookmarks;
}

FormatFilterModel *FolderViewController::formatFilter() {
    return &mFormatFilter;
}

QAbstractItemModel *FolderViewController::folderTree() const {
    return mTree.data();
}

QModelIndex FolderViewController::currentFolder() const {
    return mCurrentFolder;
}

bool FolderViewController::hasCurrentFolder() const {
    return mCurrentFolder.isValid();
}

bool FolderViewController::isActive() const {
    return mActive;
}

bool FolderViewController::isCreated() const {
    return mCreated;
}

bool FolderViewController::isFullscreen() const {
    return mFullscreen;
}

QString FolderViewController::directoryPath() const {
    return mDirectoryPath;
}

QUrl FolderViewController::bookmarkDialogFolder() const {
    const bool exists = !mDirectoryPath.isEmpty() && QDir(mDirectoryPath).exists();
    return QUrl::fromLocalFile(exists ? mDirectoryPath : mHomePath);
}

int FolderViewController::sortingMode() const {
    return static_cast<int>(mSortingMode);
}

int FolderViewController::folderSortingMode() const {
    return static_cast<int>(mFolderSortingMode);
}

QString FolderViewController::nameFilter() const {
    return mNameFilter;
}

bool FolderViewController::isPlacesPanelEnabled() const {
    return mPlacesPanelEnabled;
}

bool FolderViewController::isPlacesPanelShown() const {
    return mPlacesPanelEnabled && (mViewWidth == 0 || mViewWidth >= kPlacesPanelMinimumViewWidth);
}

int FolderViewController::placesPanelWidth() const {
    return mPlacesPanelWidth;
}

int FolderViewController::placesPanelMinimumWidth() {
    return kPlacesPanelMinimumWidth;
}

bool FolderViewController::bookmarksExpanded() const {
    return mBookmarksExpanded;
}

bool FolderViewController::treeExpanded() const {
    return mTreeExpanded;
}

bool FolderViewController::isCompactTopBar() const {
    return mViewWidth > 0 && mViewWidth < kCompactTopBarWidth;
}

//------------------------------------------------------------------------------
// Application

// FolderView::readSettings().
void FolderViewController::applySettings(const UiSettingsSnapshot &settings) {
    const FolderViewSettings &folderView = settings.folderView;
    setPlacesPanelState(folderView.placesPanel, folderView.placesPanelWidth,
                        folderView.bookmarksExpanded, folderView.treeExpanded);
    setFolderSortingMode(folderView.folderSortingMode);
    mFormatFilter.setCheckedExtensions(folderView.formatFilter);
    mBookmarks.setPaths(folderView.bookmarks);
    mGrid.applySettings(settings);
}

void FolderViewController::setActive(bool active) {
    if (active == mActive)
        return;
    mActive = active;
    emit activeChanged();
    if (!active) {
        mGrid.focusLost();
        return;
    }
    if (!mCreated) {
        mCreated = true;
        emit createdChanged();
    }
    if (!mTree)
        emit folderTreeRequested();
}

void FolderViewController::setFullscreen(bool fullscreen) {
    if (fullscreen == mFullscreen)
        return;
    mFullscreen = fullscreen;
    emit fullscreenChanged();
}

void FolderViewController::setFolderTree(QFileSystemModel *tree) {
    if (tree == mTree)
        return;
    if (mTree)
        disconnect(mTree, nullptr, this, nullptr);
    mTree = tree;
    mCurrentFolder = {};
    if (mTree) {
        connect(mTree, &QFileSystemModel::directoryLoaded, this,
                &FolderViewController::onDirectoryLoaded);
    }
    emit folderTreeChanged();
    updateCurrentFolder();
    notifyIfFilesystemViewReady();
}

// FolderView::setDirectoryPath().
void FolderViewController::setDirectoryPath(const QString &path) {
    if (path != mDirectoryPath) {
        mDirectoryPath = path;
        emit directoryPathChanged();
    }
    mReadyReported = false;
    mDirectoryToLoad = normalizedDirectory(QFileInfo(path).absolutePath());
    mBookmarks.setCurrentPath(path);
    updateCurrentFolder();
    notifyIfFilesystemViewReady();
}

void FolderViewController::setSortingMode(SettingsEnums::SortingMode mode) {
    if (mode == mSortingMode)
        return;
    mSortingMode = mode;
    emit sortingModeChanged();
}

void FolderViewController::setFolderSortingMode(SettingsEnums::SortingMode mode) {
    if (mode == mFolderSortingMode)
        return;
    mFolderSortingMode = mode;
    emit folderSortingModeChanged();
}

// FolderView::refreshFilesystemModel().
void FolderViewController::beginFolderTreeRefresh(const QString &path) {
    mLoadedDirectories.clear();
    mReadyReported = false;
    mDirectoryToLoad = path.isEmpty() ? QString()
                                      : normalizedDirectory(QFileInfo(path).absolutePath());
}

void FolderViewController::endFolderTreeRefresh() {
    notifyIfFilesystemViewReady();
}

// The tree knows the folders up to path once asked for its index; the
// folders above it are listed here, as the widget tree view listed them when
// it expanded to the folder. The readiness waits for the parent; the tree
// scrolls to the folder now and after each of them was listed.
void FolderViewController::updateCurrentFolder() {
    QModelIndex folder;
    if (mTree && !mDirectoryPath.isEmpty())
        folder = mTree->index(mDirectoryPath);
    if (folder != QModelIndex(mCurrentFolder)) {
        mCurrentFolder = folder;
        emit currentFolderChanged();
    }
    mListingAncestors.clear();
    if (!folder.isValid())
        return;
    for (QModelIndex ancestor = folder.parent(); ancestor.isValid(); ancestor = ancestor.parent()) {
        if (!mTree->canFetchMore(ancestor))
            continue;
        mListingAncestors.insert(normalizedDirectory(mTree->filePath(ancestor)));
        mTree->fetchMore(ancestor);
    }
    emit currentFolderScrollRequested();
}

void FolderViewController::onDirectoryLoaded(const QString &path) {
    const QString directory = normalizedDirectory(path);
    mLoadedDirectories.insert(directory);
    if (mListingAncestors.remove(directory))
        emit currentFolderScrollRequested();
    notifyIfFilesystemViewReady();
}

// FolderView::notifyFilesystemViewReady().
void FolderViewController::notifyIfFilesystemViewReady() {
    if (mReadyReported || mViewWidth == 0)
        return;
    if (!isPlacesPanelShown() || !mTreeExpanded) {
        mReadyReported = true;
        emit filesystemViewReady();
        return;
    }
    if (mDirectoryToLoad.isEmpty() || !mLoadedDirectories.contains(mDirectoryToLoad))
        return;
    mReadyReported = true;
    emit filesystemViewReady();
}

void FolderViewController::setPlacesPanelState(bool enabled, int width, bool bookmarks,
                                               bool tree) {
    const int boundedWidth = qMax(kPlacesPanelMinimumWidth, width);
    if (enabled == mPlacesPanelEnabled && boundedWidth == mPlacesPanelWidth &&
        bookmarks == mBookmarksExpanded && tree == mTreeExpanded)
        return;
    mPlacesPanelEnabled = enabled;
    mPlacesPanelWidth = boundedWidth;
    mBookmarksExpanded = bookmarks;
    mTreeExpanded = tree;
    emit placesPanelChanged();
    notifyIfFilesystemViewReady();
}

//------------------------------------------------------------------------------
// View

void FolderViewController::setViewWidth(qreal width) {
    const int viewWidth = qMax(0, static_cast<int>(width));
    if (viewWidth == mViewWidth)
        return;
    const bool shown = isPlacesPanelShown();
    const bool compact = isCompactTopBar();
    mViewWidth = viewWidth;
    if (shown != isPlacesPanelShown())
        emit placesPanelChanged();
    if (compact != isCompactTopBar())
        emit compactTopBarChanged();
    notifyIfFilesystemViewReady();
}

void FolderViewController::selectSorting(int mode) {
    if (!isSortingMode(mode)) {
        qWarning() << "FolderViewController ignores the sorting mode" << mode;
        return;
    }
    emit sortingSelected(static_cast<SettingsEnums::SortingMode>(mode));
}

void FolderViewController::selectFolderSorting(int mode) {
    if (!isSortingMode(mode)) {
        qWarning() << "FolderViewController ignores the folder sorting mode" << mode;
        return;
    }
    emit folderSortingSelected(static_cast<SettingsEnums::SortingMode>(mode));
}

void FolderViewController::editNameFilter(const QString &text) {
    if (text != mNameFilter) {
        mNameFilter = text;
        emit nameFilterChanged();
    }
    mNameFilterTimer.start();
}

void FolderViewController::setPlacesPanelEnabled(bool enabled) {
    if (enabled == mPlacesPanelEnabled)
        return;
    setPlacesPanelState(enabled, mPlacesPanelWidth, mBookmarksExpanded, mTreeExpanded);
    emit placesPanelEnabledRequested(enabled);
}

void FolderViewController::resizePlacesPanel(int width) {
    const int boundedWidth = qMax(kPlacesPanelMinimumWidth, width);
    if (boundedWidth == mPlacesPanelWidth)
        return;
    setPlacesPanelState(mPlacesPanelEnabled, boundedWidth, mBookmarksExpanded, mTreeExpanded);
    emit placesPanelWidthRequested(boundedWidth);
}

void FolderViewController::toggleBookmarks() {
    setPlacesPanelState(mPlacesPanelEnabled, mPlacesPanelWidth, !mBookmarksExpanded,
                        mTreeExpanded);
    emit bookmarksExpandedRequested(mBookmarksExpanded);
}

void FolderViewController::toggleTree() {
    setPlacesPanelState(mPlacesPanelEnabled, mPlacesPanelWidth, mBookmarksExpanded,
                        !mTreeExpanded);
    emit treeExpandedRequested(mTreeExpanded);
}

void FolderViewController::goHome() {
    emit directorySelected(mHomePath);
}

void FolderViewController::openBookmark(const QString &path) {
    if (!path.isEmpty())
        emit directorySelected(path);
}

void FolderViewController::openFolder(const QModelIndex &index) {
    if (!mTree || !index.isValid() || index.model() != mTree) {
        qWarning() << "FolderViewController cannot open a folder outside the folder tree";
        return;
    }
    emit directorySelected(mTree->fileInfo(index).absoluteFilePath());
}

void FolderViewController::listFolder(const QModelIndex &index) {
    if (!mTree || !index.isValid() || index.model() != mTree) {
        qWarning() << "FolderViewController cannot list a folder outside the folder tree";
        return;
    }
    if (mTree->canFetchMore(index))
        mTree->fetchMore(index);
}

void FolderViewController::addBookmark(const QUrl &folder) {
    const QString path = folder.toLocalFile();
    if (path.isEmpty()) {
        qWarning() << "FolderViewController cannot bookmark" << folder;
        return;
    }
    mBookmarks.add(path);
}

// FolderView::onDroppedInByIndex().
void FolderViewController::dropOnFolder(const QList<QUrl> &urls, const QModelIndex &index,
                                        int action) {
    if (!mTree || !index.isValid() || index.model() != mTree) {
        qWarning() << "FolderViewController cannot drop onto a folder outside the folder tree";
        return;
    }
    QList<QString> paths;
    for (const QUrl &url : urls)
        paths.append(url.toLocalFile());
    requestTransfer(paths, mTree->filePath(index), static_cast<Qt::DropAction>(action));
}

// BookmarksItem::dropEvent(): folders become bookmarks, files go into the
// bookmarked folder.
void FolderViewController::dropOnBookmark(const QList<QUrl> &urls, const QString &path,
                                          int action) {
    QList<QString> files;
    for (const QUrl &url : urls) {
        const QString localPath = url.toLocalFile();
        if (localPath.isEmpty())
            continue;
        const QFileInfo info(localPath);
        if (isFolder(info))
            mBookmarks.add(localPath);
        else if (info.isFile())
            files.append(localPath);
    }
    if (!files.isEmpty())
        requestTransfer(files, path, static_cast<Qt::DropAction>(action));
}

bool FolderViewController::dropFoldersOnBookmarks(const QList<QUrl> &urls) {
    return mBookmarks.addFolders(urls);
}

void FolderViewController::requestTransfer(const QList<QString> &paths,
                                           const QString &destination, Qt::DropAction action) {
    if (action == Qt::CopyAction)
        emit copyUrlsRequested(paths, destination);
    else if (action == Qt::MoveAction)
        emit moveUrlsRequested(paths, destination);
    else
        qWarning() << "FolderViewController ignores a drop with action" << action;
}
