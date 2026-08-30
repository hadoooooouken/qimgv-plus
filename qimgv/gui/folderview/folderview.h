#pragma once

#include <QWidget>
#include <QStyledItemDelegate>
#include <QAbstractItemView>
#include <QFileSystemModel>
#include <QFileDialog>
#include <QElapsedTimer>
#include <QTimer>
#include "gui/customwidgets/floatingwidgetcontainer.h"
#include "gui/idirectoryview.h"
#include "gui/folderview/foldergridview.h"
#include "gui/folderview/filesystemmodelcustom.h"
#include "gui/folderview/bookmarkswidget.h"
#include "gui/customwidgets/actionbutton.h"
#include "gui/customwidgets/styledcombobox.h"
#include "gui/customwidgets/formatfiltercombobox.h"

class ClickableLabel;
class QSplitter;
class QLabel;
class QLineEdit;
class QSlider;
class QPushButton;
class TreeViewCustom;
class IconButton;
class QSpacerItem;

class FolderView : public FloatingWidgetContainer, public IDirectoryView {
    Q_OBJECT
    Q_INTERFACES(IDirectoryView)
public:
    explicit FolderView(QWidget *parent = nullptr);
    ~FolderView();

public slots:
    void show();
    void hide();
    virtual void populate(int) override;
    virtual void setThumbnail(int pos, std::shared_ptr<Thumbnail> thumb) override;
    void setThumbnailUnavailable(int pos, int size) override;
    virtual void select(QList<int>) override;
    virtual void select(int) override;
    virtual QList<int> selection() override;
    virtual void focusOn(int) override;
    virtual void focusOnSelection() override;
    virtual void setDirectoryPath(QString path) override;
    virtual void insertItem(int index) override;
    virtual void removeItem(int index) override;
    virtual void reloadItem(int index) override;
    virtual void setDragHover(int) override;
    virtual void setDirCount(int count) override;
    void addItem();
    void onFullscreenModeChanged(bool mode);
    void onSortingChanged(SortingMode mode);
    void onFolderSortingChanged(SortingMode mode);
    void refreshFilesystemModel(const QString &path = QString());


protected:
    void wheelEvent(QWheelEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

protected slots:
    void onThumbnailSizeChanged(int newSize);
    void onZoomSliderValueChanged(int value);

signals:
    void itemActivated(int) override;
    void thumbnailsRequested(QList<int>, int, bool, bool) override;
    void visibleThumbnailsReady();
    void draggedOut() override;
    void draggedToBookmarks(QList<int>) override;
    void sortingSelected(SortingMode);
    void folderSortingSelected(SortingMode);
    void formatFilterSelected(QStringList);
    void nameFilterSelected(QString);
    void directorySelected(QString path);
    void showFoldersChanged(bool mode);
    void copyUrlsRequested(QList<QString>, QString path);
    void moveUrlsRequested(QList<QString>, QString path);
    void droppedInto(const QMimeData*, QObject*, int, Qt::DropAction) override;
    void draggedOver(int) override;
    void backRequested() override;
    void forwardRequested() override;
    void typeAheadTextEntered(QString text) override;
    void batchRequested();
    void openSelectedRequested();

private slots:
    void onSortingSelected(int);
    void onFolderSortingSelected(int);
    void onNameFilterTextChanged(const QString &text);
    void readSettings();

    void onTreeViewClicked(QModelIndex index);
    void onDroppedInByIndex(QList<QString>, QModelIndex index, Qt::DropAction action);
    void onBookmarkDroppedIn(QList<QString> paths, QString dirPath, Qt::DropAction action);
    void toggleBookmarks();
    void toggleFilesystemView();
    void setPlacesPanel(bool mode);
    void onPlacesPanelButtonChecked(bool mode);
    void onBookmarkClicked(QString dirPath);
    void newBookmark();

    void onSplitterMoved();
    void onHomeBtn();
    void onTreeViewTabOut();
    void onSelectionChanged();
    void onBatchClicked();

private:
    void setupUi();
    QString m_pendingScrollPath;
    int lastThumbnailResolution = 256;
    int dirCount = 0;
    QTimer nameFilterTimer;
    FileSystemModelCustom *dirModel;
    QElapsedTimer popupTimerClutch;

    // Top Bar
    QWidget *topBar;
    ActionButton *togglePlacesPanelButton;
    QWidget *pathBar;
    ActionButton *upButton;
    QLabel *pathLabel;
    QSpacerItem *pathbarSpacer;
    QLabel *selectionCountLabel;
    QPushButton *batchButton;
    QLabel *gridSizeLabel;
    QSlider *zoomSlider;
    QSpacerItem *zoomSliderSpacer;
    StyledComboBox *folderSortingComboBox;
    QSpacerItem *horizontalSpacer_folderSort;
    StyledComboBox *sortingComboBox;
    QSpacerItem *horizontalSpacer_formatFilter;
    FormatFilterComboBox *formatFilterComboBox;
    QSpacerItem *horizontalSpacer_nameFilter;
    QLineEdit *nameFilterEdit;
    ActionButton *docViewButton;
    ActionButton *settingsButton;
    QSpacerItem *panelRightEdgeSpacer;
    ActionButton *exitButton;

    // Contents
    QWidget *contentsWidget;
    QSplitter *splitter;
    
    // Places Panel
    QWidget *placesPanel;
    ClickableLabel *bookmarksLabel;
    IconButton *newBookmarkButton;
    BookmarksWidget *bookmarksWidget;
    QSpacerItem *placesPanelSpacer;
    ClickableLabel *directoriesLabel;
    IconButton *homeButton;
    TreeViewCustom *dirTreeView;

    // Grid
    QWidget *thumbnailGridHolder;
    FolderGridView *thumbnailGrid;
};
