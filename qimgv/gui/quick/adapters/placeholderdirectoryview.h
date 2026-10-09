#pragma once

#include <QList>
#include <QObject>
#include <QString>

#include <memory>

#include "gui/idirectoryview.h"

// IDirectoryView with no visible representation, standing in for the Qt Quick
// thumbnail strip (S2.4) and folder view (S3.1) so that DirectoryPresenter
// and Core run unchanged. It keeps the selection the presenter sets and
// reports each populate(); it never requests thumbnails or emits user input.
// GUI thread only.
class PlaceholderDirectoryView final : public QObject, public IDirectoryView {
    Q_OBJECT
    Q_INTERFACES(IDirectoryView)
public:
    using QObject::QObject;

    void populate(int count) override;
    void setThumbnail(int pos, std::shared_ptr<Thumbnail> thumb) override;
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

    [[nodiscard]] int itemCount() const;

signals:
    // After every populate(): the (empty) view is laid out.
    void populated();

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

private:
    int count = 0;
    QList<int> selectedIndices;
};
