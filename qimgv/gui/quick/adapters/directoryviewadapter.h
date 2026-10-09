#pragma once

#include <QList>
#include <QObject>
#include <QString>

#include <memory>

#include "gui/idirectoryview.h"
#include "gui/quick/ui/thumbnails/thumbnaillistmodel.h"

class Thumbnail;

// The directory view of the Qt Quick thumbnail strip: DirectoryPresenter
// drives it through IDirectoryView like the widget ThumbnailStrip, and QML
// shows it as a list model (ThumbnailListModel). Thumbnails are handed over
// as their decoded image (Thumbnail::image()), without a QPixmap conversion.
// The requests of the model go out as the IDirectoryView signals.
// GUI thread only.
class DirectoryViewAdapter final : public ThumbnailListModel, public IDirectoryView {
    Q_OBJECT
    Q_INTERFACES(IDirectoryView)
public:
    explicit DirectoryViewAdapter(QObject *parent = nullptr);

    void populate(int count) override;
    void setThumbnail(int pos, std::shared_ptr<Thumbnail> thumb) override;
    void setThumbnailPending(int pos, bool pending) override;
    void setThumbnailUnavailable(int pos, int size) override;
    void select(QList<int> indices) override;
    void select(int index) override;
    void focusOn(int index) override;
    void focusOnSelection() override;
    QList<int> selection() override;
    void setDirectoryPath(QString path) override;
    void insertItem(int index) override;
    void removeItem(int index) override;
    void reloadItem(int index) override;
    void setDragHover(int index) override;
    void setDirCount(int count) override;

signals:
    // IDirectoryView
    void itemActivated(int) override;
    void thumbnailsRequested(QList<int>, int, bool, bool) override;
    void draggedOut() override;
    void draggedToBookmarks(QList<int>) override;
    void draggedOver(int) override;
    void droppedInto(const QMimeData *, QObject *, int, Qt::DropAction) override;
    void backRequested() override;
    void forwardRequested() override;
    void openSelectedRequested() override;
    void typeAheadTextEntered(QString text) override;
};
