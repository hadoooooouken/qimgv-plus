#pragma once

#include <QtPlugin>
#include <QList>
#include <memory>

#include <QDropEvent>

class Thumbnail;
class QString;
class QMimeData;

class IDirectoryView {
public:
    virtual ~IDirectoryView() {}

    virtual void populate(int) = 0;
    virtual void setThumbnail(int pos, std::shared_ptr<Thumbnail> thumb) = 0;
    virtual void setThumbnailUnavailable(int pos, int size) = 0;
    virtual void select(QList<int>) = 0;
    virtual void select(int) = 0;
    virtual void focusOn(int) = 0;
    virtual void focusOnSelection() = 0;
    virtual QList<int> selection() = 0;
    virtual void setDirectoryPath(QString path) = 0;
    virtual void insertItem(int index) = 0;
    virtual void removeItem(int index) = 0;
    virtual void reloadItem(int index) = 0;
    virtual void setDragHover(int index) = 0;
    virtual void setDirCount(int count) { Q_UNUSED(count) }

//signals
    virtual void itemActivated(int) = 0;
    virtual void thumbnailsRequested(QList<int>, int, bool, bool) = 0;
    virtual void draggedOut() = 0;
    virtual void draggedToBookmarks(QList<int>) = 0;
    virtual void draggedOver(int) = 0;
    virtual void droppedInto(const QMimeData*, QObject*, int, Qt::DropAction) = 0;
    virtual void backRequested() = 0;
    virtual void forwardRequested() = 0;
    virtual void openSelectedRequested() {}
    // Emitted for a plain printable keypress (no Ctrl/Alt/Meta) so the
    // presenter can do Explorer-style type-ahead navigation against the
    // model. text is the character(s) produced by the key event.
    virtual void typeAheadTextEntered(QString text) = 0;
};

Q_DECLARE_INTERFACE(IDirectoryView, "IDirectoryView")
