#pragma once

#include <QObject>
#include <memory>
#include "gui/idirectoryview.h"
#include "components/thumbnailer/thumbnailer.h"
#include "directorymodel.h"
#include "sharedresources.h"
#include <QMimeData>

//tmp
#include <QtSvg/QSvgRenderer>
#include <QDir>
#include <QImageReader>
#include <QMultiMap>

class DirectoryPresenter : public QObject {
    Q_OBJECT
public:
    explicit DirectoryPresenter(QObject *parent = nullptr);

    void setView(std::shared_ptr<IDirectoryView>);
    void setModel(std::shared_ptr<DirectoryModel> newModel);
    void unsetModel();
    // Lets multiple DirectoryPresenter instances (e.g. the thumbnail panel
    // and the folder view, which always mirror the same DirectoryModel)
    // share one Thumbnailer. Sharing makes Thumbnailer's own request
    // deduplication (queuedTasks/runningTasks/pendingReruns) effective
    // across both presenters instead of just within each one, so opening a
    // folder no longer decodes+resizes every image twice in parallel.
    // Must be called before the presenter starts requesting thumbnails
    // (i.e. right after construction) - swapping it later would silently
    // drop the connection for any thumbnail already in flight on the
    // previous instance.
    void setThumbnailer(std::shared_ptr<Thumbnailer> newThumbnailer);

    void selectAndFocus(int index);
    void selectAndFocus(QString path);

    void onFileRemoved(QString filePath, int index);
    void onFileRenamed(QString fromPath, int indexFrom, QString toPath, int indexTo);
    void onFileAdded(QString filePath);
    void onFileModified(QString filePath);

    void onDirRemoved(QString dirPath, int index);
    void onDirRenamed(QString fromPath, int indexFrom, QString toPath, int indexTo);
    void onDirAdded(QString dirPath);

    bool showDirs();
    void setShowDirs(bool mode);

    QList<QString> selectedPaths() const;
    QList<QString> expandedSelectedPaths() const;
    QString firstSelectedDirectoryPath() const;
    int upArrowCount() const;


signals:
    void fileActivated(QString filePath);
    void filesActivated(QList<QString> filePaths, QString activePath);
    void dirActivated(QString dirPath);
    void draggedOut(QList<QString>);
    void droppedInto(QList<QString>, QString, Qt::DropAction);
    void backRequested();
    void forwardRequested();

public slots:
    void disconnectView();
    void reloadModel();

private slots:
    void onSettingsChanged();
    void generateThumbnails(QList<int>, int, bool, bool);
    void onThumbnailReady(std::shared_ptr<Thumbnail> thumb, QString filePath);
    void populateView();
    void onItemActivated(int absoluteIndex);
    void onOpenSelectedRequested();
    void onDraggedOut();
    void onDraggedOver(int index);

    void onDroppedInto(const QMimeData *data, QObject *source, int targetIndex, Qt::DropAction action);
private:
    std::shared_ptr<IDirectoryView> view = nullptr;
    std::shared_ptr<DirectoryModel> model = nullptr;
    std::shared_ptr<Thumbnailer> thumbnailer;
    bool mShowDirs;
    QMultiMap<QString, int> dirThumbnailTasks;

    std::shared_ptr<Thumbnail> composeFolderThumbnail(int size, const QString &dirName, const QPixmap &innerThumb);
    std::shared_ptr<Thumbnail> composeUpArrowThumbnail(int size);
};
