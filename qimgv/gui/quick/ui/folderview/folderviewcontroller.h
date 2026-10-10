#pragma once

#include <QAbstractItemModel>
#include <QList>
#include <QObject>
#include <QPersistentModelIndex>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

#include "gui/quick/bridges/settingsenums.h"
#include "gui/quick/bridges/uisettings.h"
#include "gui/quick/ui/folderview/bookmarksmodel.h"
#include "gui/quick/ui/folderview/foldergridcontroller.h"
#include "gui/quick/ui/folderview/formatfiltermodel.h"

class QFileSystemModel;

// The folder view (FolderView in the widget UI) apart from its grid: the top
// bar (path, sorting of files and of folders, format and name filters), the
// places panel (bookmarks and the folder tree, both collapsible, next to the
// grid behind a splitter) and its readiness for the cold-start reveal.
// - The places panel shows while enabled in a view of at least 600 px; the
//   grid size slider and its label hide below 510 px.
// - The name filter is published 150 ms after the last edit.
// - The folder tree (a QFileSystemModel of folders, provided by the host
//   when the view is first activated) follows the directory shown in the
//   grid. The view is ready once it is laid out (it reported its width) and
//   the parent of that directory was loaded into the tree, or the tree or
//   the panel is hidden (filesystemViewReady, once per directory or
//   refresh). Like the widget view, which exists only once the window is
//   shown, it is never ready before it is laid out.
// - Files dropped onto a folder of the tree or onto a bookmark are copied
//   or moved there; folders dropped onto a bookmark or the bookmarks header
//   are bookmarked.
// User intents leave as signals; the panel layout requests (enabled,
// width, collapsed sections) are to be stored by the host. GUI thread only.
class FolderViewController : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by the Quick UI host")
    Q_PROPERTY(FolderGridController *grid READ grid CONSTANT FINAL)
    Q_PROPERTY(BookmarksModel *bookmarks READ bookmarks CONSTANT FINAL)
    Q_PROPERTY(FormatFilterModel *formatFilter READ formatFilter CONSTANT FINAL)
    Q_PROPERTY(QAbstractItemModel *folderTree READ folderTree NOTIFY folderTreeChanged FINAL)
    Q_PROPERTY(QModelIndex currentFolder READ currentFolder NOTIFY currentFolderChanged FINAL)
    Q_PROPERTY(bool hasCurrentFolder READ hasCurrentFolder NOTIFY currentFolderChanged FINAL)
    Q_PROPERTY(bool active READ isActive NOTIFY activeChanged FINAL)
    // The view content exists from its first activation on.
    Q_PROPERTY(bool created READ isCreated NOTIFY createdChanged FINAL)
    Q_PROPERTY(bool fullscreen READ isFullscreen NOTIFY fullscreenChanged FINAL)
    Q_PROPERTY(QString directoryPath READ directoryPath NOTIFY directoryPathChanged FINAL)
    // Where the "new bookmark" dialog starts: the directory if it exists,
    // the home folder otherwise.
    Q_PROPERTY(QUrl bookmarkDialogFolder READ bookmarkDialogFolder NOTIFY directoryPathChanged FINAL)
    Q_PROPERTY(int sortingMode READ sortingMode NOTIFY sortingModeChanged FINAL)
    Q_PROPERTY(int folderSortingMode READ folderSortingMode NOTIFY folderSortingModeChanged FINAL)
    Q_PROPERTY(QString nameFilter READ nameFilter NOTIFY nameFilterChanged FINAL)
    Q_PROPERTY(bool placesPanelEnabled READ isPlacesPanelEnabled NOTIFY placesPanelChanged FINAL)
    Q_PROPERTY(bool placesPanelShown READ isPlacesPanelShown NOTIFY placesPanelChanged FINAL)
    Q_PROPERTY(int placesPanelWidth READ placesPanelWidth NOTIFY placesPanelChanged FINAL)
    Q_PROPERTY(int placesPanelMinimumWidth READ placesPanelMinimumWidth CONSTANT FINAL)
    Q_PROPERTY(bool bookmarksExpanded READ bookmarksExpanded NOTIFY placesPanelChanged FINAL)
    Q_PROPERTY(bool treeExpanded READ treeExpanded NOTIFY placesPanelChanged FINAL)
    Q_PROPERTY(bool compactTopBar READ isCompactTopBar NOTIFY compactTopBarChanged FINAL)

public:
    // The places panel needs a view at least this wide.
    static constexpr int kPlacesPanelMinimumViewWidth = 600;
    static constexpr int kPlacesPanelMinimumWidth = 260;
    // Below this view width the grid size slider and its label hide.
    static constexpr int kCompactTopBarWidth = 510;
    static constexpr int kNameFilterDelayMs = 150;

    // grid must outlive the controller; settings: the initial UI settings;
    // homePath: the folder of the home button and of an empty bookmark list.
    FolderViewController(FolderGridController &grid, const UiSettingsSnapshot &settings,
                         const QString &homePath, QObject *parent = nullptr);
    ~FolderViewController() override;

    [[nodiscard]] FolderGridController *grid() const;
    [[nodiscard]] BookmarksModel *bookmarks();
    [[nodiscard]] FormatFilterModel *formatFilter();
    [[nodiscard]] QAbstractItemModel *folderTree() const;
    [[nodiscard]] QModelIndex currentFolder() const;
    [[nodiscard]] bool hasCurrentFolder() const;
    [[nodiscard]] bool isActive() const;
    [[nodiscard]] bool isCreated() const;
    [[nodiscard]] bool isFullscreen() const;
    [[nodiscard]] QString directoryPath() const;
    [[nodiscard]] QUrl bookmarkDialogFolder() const;
    [[nodiscard]] int sortingMode() const;
    [[nodiscard]] int folderSortingMode() const;
    [[nodiscard]] QString nameFilter() const;
    [[nodiscard]] bool isPlacesPanelEnabled() const;
    [[nodiscard]] bool isPlacesPanelShown() const;
    [[nodiscard]] int placesPanelWidth() const;
    [[nodiscard]] static int placesPanelMinimumWidth();
    [[nodiscard]] bool bookmarksExpanded() const;
    [[nodiscard]] bool treeExpanded() const;
    [[nodiscard]] bool isCompactTopBar() const;

    // --- application (Quick UI host) -------------------------------------
    void applySettings(const UiSettingsSnapshot &settings);
    // The folder view is the window's view; the first activation creates
    // the content and asks for the folder tree.
    void setActive(bool active);
    void setFullscreen(bool fullscreen);
    // tree must outlive the controller or be replaced first.
    void setFolderTree(QFileSystemModel *tree);
    // The directory shown in the grid.
    void setDirectoryPath(const QString &path);
    void setSortingMode(SettingsEnums::SortingMode mode);
    void setFolderSortingMode(SettingsEnums::SortingMode mode);
    // The tree is about to re-read path (empty: everything); the view
    // waits for it again.
    void beginFolderTreeRefresh(const QString &path);
    void endFolderTreeRefresh();

    // --- view (QML) ------------------------------------------------------
    Q_INVOKABLE void setViewWidth(qreal width);
    Q_INVOKABLE void selectSorting(int mode);
    Q_INVOKABLE void selectFolderSorting(int mode);
    Q_INVOKABLE void editNameFilter(const QString &text);
    Q_INVOKABLE void setPlacesPanelEnabled(bool enabled);
    // The splitter was moved to width.
    Q_INVOKABLE void resizePlacesPanel(int width);
    Q_INVOKABLE void toggleBookmarks();
    Q_INVOKABLE void toggleTree();
    Q_INVOKABLE void goHome();
    Q_INVOKABLE void openBookmark(const QString &path);
    Q_INVOKABLE void openFolder(const QModelIndex &index);
    // Lists the subfolders of a folder of the tree that expanded.
    Q_INVOKABLE void listFolder(const QModelIndex &index);
    // A folder picked in the "new bookmark" dialog.
    Q_INVOKABLE void addBookmark(const QUrl &folder);
    // Drops (action: a Qt::DropAction).
    Q_INVOKABLE void dropOnFolder(const QList<QUrl> &urls, const QModelIndex &index, int action);
    Q_INVOKABLE void dropOnBookmark(const QList<QUrl> &urls, const QString &path, int action);
    // Folders dropped onto the bookmarks header or the new bookmark button;
    // returns whether one was bookmarked.
    Q_INVOKABLE bool dropFoldersOnBookmarks(const QList<QUrl> &urls);

signals:
    void folderTreeChanged();
    void currentFolderChanged();
    // The tree should scroll the current folder into view (again: each
    // listed folder above it moves its row).
    void currentFolderScrollRequested();
    void activeChanged();
    void createdChanged();
    void fullscreenChanged();
    void directoryPathChanged();
    void sortingModeChanged();
    void folderSortingModeChanged();
    void nameFilterChanged();
    void placesPanelChanged();
    void compactTopBarChanged();
    // The first activation needs the folder tree (setFolderTree()).
    void folderTreeRequested();
    // Readiness for the cold-start reveal.
    void filesystemViewReady();

    // User intents.
    void sortingSelected(SettingsEnums::SortingMode mode);
    void folderSortingSelected(SettingsEnums::SortingMode mode);
    void formatFilterSelected(const QStringList &extensions);
    void nameFilterSelected(const QString &nameFilter);
    void directorySelected(const QString &path);
    void copyUrlsRequested(const QList<QString> &paths, const QString &destDirectory);
    void moveUrlsRequested(const QList<QString> &paths, const QString &destDirectory);

    // To be stored.
    void placesPanelEnabledRequested(bool enabled);
    void placesPanelWidthRequested(int width);
    void bookmarksExpandedRequested(bool expanded);
    void treeExpandedRequested(bool expanded);
    void bookmarksRequested(const QStringList &paths);

private:
    void updateCurrentFolder();
    void onDirectoryLoaded(const QString &path);
    void notifyIfFilesystemViewReady();
    void requestTransfer(const QList<QString> &paths, const QString &destination,
                         Qt::DropAction action);
    void setPlacesPanelState(bool enabled, int width, bool bookmarks, bool tree);

    FolderGridController &mGrid;
    BookmarksModel mBookmarks;
    FormatFilterModel mFormatFilter;
    QString mHomePath;
    QPointer<QFileSystemModel> mTree;
    QPersistentModelIndex mCurrentFolder;

    bool mActive = false;
    bool mCreated = false;
    bool mFullscreen = false;
    QString mDirectoryPath;
    SettingsEnums::SortingMode mSortingMode = SettingsEnums::SortingMode::Name;
    SettingsEnums::SortingMode mFolderSortingMode = SettingsEnums::SortingMode::Name;
    QString mNameFilter;
    QTimer mNameFilterTimer;

    bool mPlacesPanelEnabled = false;
    int mPlacesPanelWidth = kPlacesPanelMinimumWidth;
    bool mBookmarksExpanded = true;
    bool mTreeExpanded = true;
    // 0 until the view reports its width (the panel counts as fitting).
    int mViewWidth = 0;

    // Readiness: the normalized parent of the directory, the directories
    // the tree loaded and whether the current wait was reported.
    QString mDirectoryToLoad;
    QSet<QString> mLoadedDirectories;
    bool mReadyReported = false;
    // Folders above the current one that are being listed (normalized).
    QSet<QString> mListingAncestors;
};
