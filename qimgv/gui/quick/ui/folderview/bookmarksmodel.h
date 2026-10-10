#pragma once

#include <QAbstractItemModel>
#include <QList>
#include <QObject>
#include <QRangeModel>
#include <QRangeModelAdapter>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

#include <vector>

// One bookmark: the folder's name and its path.
struct BookmarkEntry {
    Q_GADGET
    Q_PROPERTY(QString name MEMBER name)
    Q_PROPERTY(QString path MEMBER path)

public:
    QString name;
    QString path;

    friend bool operator==(const BookmarkEntry &, const BookmarkEntry &) = default;
};

// Each gadget property is a role (name, path) of a single-column list.
template <> struct QRangeModel::RowOptions<BookmarkEntry> {
    static constexpr auto rowCategory = QRangeModel::RowCategory::MultiRoleItem;
};

// The bookmarked folders of the folder view's places panel (BookmarksWidget
// in the widget UI): added once each, removed, moved one step or dragged to
// another place; the folder shown in the grid is highlighted. An empty list
// holds the home folder. Every change publishes the paths to be stored.
// GUI thread only.
class BookmarksModel : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by FolderViewController.bookmarks")
    // Roles: name, path.
    Q_PROPERTY(QAbstractItemModel *items READ items CONSTANT FINAL)
    Q_PROPERTY(QString currentPath READ currentPath NOTIFY currentPathChanged FINAL)

public:
    // homePath: the folder an empty list holds.
    explicit BookmarksModel(QString homePath, QObject *parent = nullptr);
    ~BookmarksModel() override;

    [[nodiscard]] QAbstractItemModel *items() const;
    [[nodiscard]] QString currentPath() const;
    [[nodiscard]] QStringList paths() const;

    // The stored bookmarks; publishes nothing unless the home folder had to
    // be added.
    void setPaths(const QStringList &paths);
    // The folder shown in the grid.
    void setCurrentPath(const QString &path);

    Q_INVOKABLE void add(const QString &path);
    Q_INVOKABLE void remove(const QString &path);
    Q_INVOKABLE void moveUp(const QString &path);
    Q_INVOKABLE void moveDown(const QString &path);
    // Moves the bookmark at row from to row to (drag and drop).
    Q_INVOKABLE void move(int from, int to);
    // Bookmarks the local folders among urls; returns whether there was one.
    Q_INVOKABLE bool addFolders(const QList<QUrl> &urls);

signals:
    void currentPathChanged();
    // The bookmarks changed; to be stored.
    void pathsChanged(const QStringList &paths);

private:
    [[nodiscard]] int rowOf(const QString &path) const;
    [[nodiscard]] static BookmarkEntry entryFor(const QString &path);
    void publish();

    QString mHomePath;
    QString mCurrentPath;
    QRangeModelAdapter<std::vector<BookmarkEntry>> mEntries;
};
