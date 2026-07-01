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
    void droppedInto(QList<QString>, QString);
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

    void onDroppedInto(const QMimeData *data, QObject *source, int targetIndex);
private:
    std::shared_ptr<IDirectoryView> view = nullptr;
    std::shared_ptr<DirectoryModel> model = nullptr;
    Thumbnailer thumbnailer;
    bool mShowDirs;
    QMultiMap<QString, int> dirThumbnailTasks;

    std::shared_ptr<Thumbnail> composeFolderThumbnail(int size, const QString &dirName, const QPixmap &innerThumb);
    std::shared_ptr<Thumbnail> composeUpArrowThumbnail(int size);
};
