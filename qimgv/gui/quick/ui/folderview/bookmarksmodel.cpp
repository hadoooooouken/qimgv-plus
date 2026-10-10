#include "bookmarksmodel.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>

BookmarksModel::BookmarksModel(QString homePath, QObject *parent)
    : QObject(parent), mHomePath(std::move(homePath)), mEntries(std::vector<BookmarkEntry>{}) {}

BookmarksModel::~BookmarksModel() = default;

QAbstractItemModel *BookmarksModel::items() const {
    return mEntries.model();
}

QString BookmarksModel::currentPath() const {
    return mCurrentPath;
}

QStringList BookmarksModel::paths() const {
    QStringList result;
    for (const BookmarkEntry &entry : mEntries.range())
        result.append(entry.path);
    return result;
}

// BookmarksWidget::readSettings(); duplicates are dropped.
void BookmarksModel::setPaths(const QStringList &paths) {
    std::vector<BookmarkEntry> entries;
    QStringList seen;
    for (const QString &path : paths) {
        if (path.isEmpty() || seen.contains(path))
            continue;
        seen.append(path);
        entries.push_back(entryFor(path));
    }
    const bool addHome = entries.empty();
    if (addHome)
        entries.push_back(entryFor(mHomePath));
    if (entries != mEntries.range())
        mEntries.assign(std::move(entries));
    if (addHome)
        publish();
}

void BookmarksModel::setCurrentPath(const QString &path) {
    if (path == mCurrentPath)
        return;
    mCurrentPath = path;
    emit currentPathChanged();
}

void BookmarksModel::add(const QString &path) {
    if (path.isEmpty() || rowOf(path) >= 0)
        return;
    const int row = static_cast<int>(mEntries.range().size());
    if (!mEntries.insertRow(row, entryFor(path))) {
        qWarning() << "BookmarksModel could not add" << path;
        return;
    }
    publish();
}

void BookmarksModel::remove(const QString &path) {
    const int row = rowOf(path);
    if (row < 0)
        return;
    if (!mEntries.removeRow(row)) {
        qWarning() << "BookmarksModel could not remove" << path;
        return;
    }
    publish();
}

void BookmarksModel::moveUp(const QString &path) {
    const int row = rowOf(path);
    if (row > 0)
        move(row, row - 1);
}

void BookmarksModel::moveDown(const QString &path) {
    const int row = rowOf(path);
    if (row >= 0)
        move(row, row + 1);
}

void BookmarksModel::move(int from, int to) {
    const int count = static_cast<int>(mEntries.range().size());
    if (from < 0 || from >= count || to < 0 || to >= count || from == to)
        return;
    // QAbstractItemModel::moveRows() names the row the moved one goes in
    // front of, counted before the move.
    const int destination = to > from ? to + 1 : to;
    if (!mEntries.moveRow(from, destination)) {
        qWarning() << "BookmarksModel could not move row" << from << "to" << to;
        return;
    }
    publish();
}

bool BookmarksModel::addFolders(const QList<QUrl> &urls) {
    bool added = false;
    for (const QUrl &url : urls) {
        const QString localPath = url.toLocalFile();
        if (localPath.isEmpty() || !QFileInfo(localPath).isDir())
            continue;
        add(localPath);
        added = true;
    }
    return added;
}

int BookmarksModel::rowOf(const QString &path) const {
    const auto &entries = mEntries.range();
    for (std::size_t row = 0; row < entries.size(); ++row) {
        if (entries.at(row).path == path)
            return static_cast<int>(row);
    }
    return -1;
}

// A root folder has no name; its path is shown instead.
BookmarkEntry BookmarksModel::entryFor(const QString &path) {
    const QString name = QFileInfo(path).fileName();
    return {.name = name.isEmpty() ? QDir::toNativeSeparators(path) : name, .path = path};
}

void BookmarksModel::publish() {
    emit pathsChanged(paths());
}
