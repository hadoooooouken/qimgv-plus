#pragma once

#include "gui/folderview/folderview.h"
#include <QMutexLocker>

struct FolderViewStateBuffer {
    QString directory;
    QList<int> selection;
    int itemCount = 0;
    int dirCount = 0;
    SortingMode sortingMode;
    SortingMode folderSortingMode = SortingMode::SORT_TIME_DESC;
    bool fullscreenMode;
};

class FolderViewProxy : public QWidget, public IDirectoryView {
    Q_OBJECT
    Q_INTERFACES(IDirectoryView)
public:
    FolderViewProxy(QWidget *parent = nullptr);
    void init();

public slots:
    virtual void populate(int) override;
    virtual void setThumbnail(int pos, std::shared_ptr<Thumbnail> thumb) override;
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
    FloatingWidgetContainer *getWidgetContainer();

protected:
    void showEvent(QShowEvent *event) override;

signals:
    void itemActivated(int) override;
    void thumbnailsRequested(QList<int>, int, bool, bool) override;
    void draggedOut() override;
    void draggedToBookmarks(QList<int>) override;
    void sortingSelected(SortingMode);
    void folderSortingSelected(SortingMode);
    void showFoldersChanged(bool mode);
    void directorySelected(QString);
    void copyUrlsRequested(QList<QString>, QString path);
    void moveUrlsRequested(QList<QString>, QString path);
    void droppedInto(const QMimeData*, QObject*, int) override;
    void draggedOver(int) override;
    void backRequested() override;
    void forwardRequested() override;
    void batchRequested();
    void openSelectedRequested();

private:
    std::shared_ptr<FolderView> folderView;
    QVBoxLayout layout;
    FolderViewStateBuffer stateBuf;
    QMutex m;
};
