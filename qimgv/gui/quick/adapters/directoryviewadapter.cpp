#include "directoryviewadapter.h"

#include <QDebug>

#include "sourcecontainers/thumbnail.h"

DirectoryViewAdapter::DirectoryViewAdapter(QObject *parent) : ThumbnailListModel(parent) {
    connect(this, &ThumbnailListModel::activated, this, &DirectoryViewAdapter::itemActivated);
    connect(this, &ThumbnailListModel::thumbnailsNeeded, this,
            &DirectoryViewAdapter::thumbnailsRequested);
    connect(this, &ThumbnailListModel::dragOutRequested, this, &DirectoryViewAdapter::draggedOut);
    connect(this, &ThumbnailListModel::backNavigationRequested, this,
            &DirectoryViewAdapter::backRequested);
    connect(this, &ThumbnailListModel::forwardNavigationRequested, this,
            &DirectoryViewAdapter::forwardRequested);
}

void DirectoryViewAdapter::populate(int count) {
    ThumbnailListModel::populate(count);
}

void DirectoryViewAdapter::setThumbnail(int pos, std::shared_ptr<Thumbnail> thumb) {
    if (!thumb) {
        qWarning() << "DirectoryViewAdapter: no thumbnail for item" << pos;
        return;
    }
    ThumbnailListModel::setThumbnail(
        pos,
        ThumbnailEntry{
            .handle = {.image = thumb->image(), .sourceSize = thumb->sourceSize()},
            .name = thumb->name(),
            .info = thumb->info(),
        },
        thumb->size());
}

void DirectoryViewAdapter::setThumbnailPending(int pos, bool pending) {
    ThumbnailListModel::setThumbnailPending(pos, pending);
}

void DirectoryViewAdapter::setThumbnailUnavailable(int pos, int size) {
    ThumbnailListModel::setThumbnailUnavailable(pos, size);
}

void DirectoryViewAdapter::select(QList<int> indices) {
    ThumbnailListModel::select(std::move(indices));
}

void DirectoryViewAdapter::select(int index) {
    ThumbnailListModel::select(index);
}

void DirectoryViewAdapter::focusOn(int index) {
    ThumbnailListModel::focusOn(index);
}

void DirectoryViewAdapter::focusOnSelection() {
    ThumbnailListModel::focusOnSelection();
}

QList<int> DirectoryViewAdapter::selection() {
    return ThumbnailListModel::selection();
}

// The strip shows no path (ThumbnailStrip ignores it as well).
void DirectoryViewAdapter::setDirectoryPath(QString path) {
    Q_UNUSED(path)
}

void DirectoryViewAdapter::insertItem(int index) {
    ThumbnailListModel::insertItem(index);
}

void DirectoryViewAdapter::removeItem(int index) {
    ThumbnailListModel::removeItem(index);
}

void DirectoryViewAdapter::reloadItem(int index) {
    ThumbnailListModel::reloadItem(index);
}

void DirectoryViewAdapter::setDragHover(int index) {
    ThumbnailListModel::setDragHover(index);
}

void DirectoryViewAdapter::setDirCount(int count) {
    ThumbnailListModel::setDirCount(count);
}
