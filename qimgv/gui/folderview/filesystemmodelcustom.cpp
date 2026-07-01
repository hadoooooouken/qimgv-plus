#include "filesystemmodelcustom.h"
#include "settings.h"
#include <QDirIterator>

FileSystemModelCustom::FileSystemModelCustom(QObject *parent) : QFileSystemModel(parent) {
    qreal dpr = qApp->devicePixelRatio();
    QString iconPath = ":/res/icons/common/menuitem/folder16.png";
    if(dpr >= (1.0 + 0.001))
        iconPath.replace(".", "@2x.");
    folderIcon.load(iconPath);
    ImageLib::recolor(this->folderIcon, settings->colorScheme().icons);

    connect(settings, &Settings::settingsChanged, this, [this]() {
        ImageLib::recolor(this->folderIcon, settings->colorScheme().icons);
    });

    connect(this, &QAbstractItemModel::rowsInserted, this, [this](const QModelIndex &parent, int first, int last) {
        if (parent.isValid()) {
            hasSubfoldersCache.remove(filePath(parent));
        }
    });
    connect(this, &QAbstractItemModel::rowsRemoved, this, [this](const QModelIndex &parent, int first, int last) {
        if (parent.isValid()) {
            hasSubfoldersCache.remove(filePath(parent));
        }
    });
    connect(this, &QAbstractItemModel::modelReset, this, [this]() {
        hasSubfoldersCache.clear();
    });
    connect(this, &QAbstractItemModel::layoutChanged, this, [this]() {
        hasSubfoldersCache.clear();
    });
}

FileSystemModelCustom::~FileSystemModelCustom() {
    setRootPath("");
}

QVariant FileSystemModelCustom::data( const QModelIndex& index, int role ) const {
    if(role == Qt::DecorationRole)
        return folderIcon;
    return QFileSystemModel::data(index, role);
}

Qt::ItemFlags FileSystemModelCustom::flags(const QModelIndex& index) const {
    if(!index.isValid()) {
        return Qt::NoItemFlags; // 0
        //return Qt::ItemIsDropEnabled;    // Allow drops in the top-level (no parent)
    }
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDropEnabled;
}

void FileSystemModelCustom::refreshPath(const QString &path) {
    hasSubfoldersCache.clear();

    if (!path.isEmpty()) {
        QModelIndex targetIndex = index(path);
        if (targetIndex.isValid()) {
            QString rootPath = filePath(targetIndex);
            if (!rootPath.isEmpty()) {
                setRootPath(rootPath);
                setRootPath("");
                setRootPath(rootPath);
                return;
            }
        }
    }

    setRootPath("");
    setRootPath("");
}

bool FileSystemModelCustom::hasChildren(const QModelIndex &parent) const {
    if (parent.column() > 0) {
        return false;
    }
    if (!parent.isValid()) {
        return true;
    }
    if (!isDir(parent)) {
        return false;
    }

    QString path = filePath(parent);

    auto it = hasSubfoldersCache.constFind(path);
    if (it != hasSubfoldersCache.constEnd()) {
        return it.value();
    }

    if (!canFetchMore(parent)) {
        bool hasSub = rowCount(parent) > 0;
        hasSubfoldersCache.insert(path, hasSub);
        return hasSub;
    }

    QDir::Filters filters = filter();
    if (!(filters & QDir::Dirs)) {
        filters |= QDir::Dirs;
    }
    filters |= QDir::NoDotAndDotDot;
    filters &= ~QDir::Files;

    QDirIterator dirIt(path, filters);
    bool hasSub = dirIt.hasNext();
    hasSubfoldersCache.insert(path, hasSub);
    return hasSub;
}
