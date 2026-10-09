#include "placeholderdirectoryview.h"

void PlaceholderDirectoryView::populate(int newCount) {
    count = qMax(0, newCount);
    selectedIndices.clear();
    emit populated();
}

void PlaceholderDirectoryView::setThumbnail(int pos, std::shared_ptr<Thumbnail> thumb) {
    Q_UNUSED(pos)
    Q_UNUSED(thumb)
}

void PlaceholderDirectoryView::setThumbnailUnavailable(int pos, int size) {
    Q_UNUSED(pos)
    Q_UNUSED(size)
}

void PlaceholderDirectoryView::select(QList<int> indices) {
    selectedIndices.clear();
    for (int index : std::as_const(indices)) {
        if (index >= 0 && index < count && !selectedIndices.contains(index))
            selectedIndices.append(index);
    }
}

void PlaceholderDirectoryView::select(int index) {
    select(QList<int>{index});
}

void PlaceholderDirectoryView::focusOn(int index) {
    Q_UNUSED(index)
}

void PlaceholderDirectoryView::focusOnSelection() {
}

QList<int> PlaceholderDirectoryView::selection() {
    return selectedIndices;
}

void PlaceholderDirectoryView::setDirectoryPath(QString path) {
    Q_UNUSED(path)
}

// The selection follows the items, as in the thumbnail views.
void PlaceholderDirectoryView::insertItem(int index) {
    ++count;
    for (int &selected : selectedIndices) {
        if (selected >= index)
            ++selected;
    }
}

void PlaceholderDirectoryView::removeItem(int index) {
    if (index < 0 || index >= count)
        return;
    --count;
    selectedIndices.removeAll(index);
    for (int &selected : selectedIndices) {
        if (selected > index)
            --selected;
    }
}

void PlaceholderDirectoryView::reloadItem(int index) {
    Q_UNUSED(index)
}

void PlaceholderDirectoryView::setDragHover(int index) {
    Q_UNUSED(index)
}

int PlaceholderDirectoryView::itemCount() const {
    return count;
}
