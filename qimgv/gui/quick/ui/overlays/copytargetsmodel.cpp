#include "copytargetsmodel.h"

#include <QDebug>
#include <QDir>
#include <QUrl>

#include "components/copytargets/copytargetlist.h"

namespace {
QString displayNameOf(const QString &directory) {
    const QStringList parts = QDir::fromNativeSeparators(directory).split(u'/', Qt::SkipEmptyParts);
    return parts.isEmpty() ? directory : parts.constLast();
}

// Digit shortcut of row: "1" for the first row.
QString shortcutOf(int row) {
    return QString::number(row + 1);
}
} // namespace

CopyTargetsModel::CopyTargetsModel(QObject *parent) : QAbstractListModel(parent) {}

int CopyTargetsModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : static_cast<int>(mTargets.size());
}

QVariant CopyTargetsModel::data(const QModelIndex &index, int role) const {
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return {};
    const QString &directory = mTargets.at(index.row());
    switch (role) {
    case DirectoryRole:
        return directory;
    case DisplayNameRole:
        return displayNameOf(directory);
    case ShortcutRole:
        return shortcutOf(index.row());
    default:
        return {};
    }
}

QHash<int, QByteArray> CopyTargetsModel::roleNames() const {
    return {{DirectoryRole, "directory"},
            {DisplayNameRole, "displayName"},
            {ShortcutRole, "shortcut"}};
}

CopyTargetsModel::Mode CopyTargetsModel::mode() const {
    return mMode;
}

void CopyTargetsModel::setMode(Mode mode) {
    if (mMode == mode)
        return;
    mMode = mode;
    emit modeChanged();
}

bool CopyTargetsModel::isLoaded() const {
    return mLoaded;
}

void CopyTargetsModel::setTargets(const QStringList &targets) {
    beginResetModel();
    mTargets = targets.mid(0, kMaxCopyTargets);
    endResetModel();
    mLoaded = true;
}

QStringList CopyTargetsModel::targets() const {
    return mTargets;
}

void CopyTargetsModel::activate(int row) {
    if (!isValidRow(row)) {
        qWarning() << "CopyTargetsModel::activate: no row" << row;
        return;
    }
    const QString &directory = mTargets.at(row);
    if (directory.isEmpty())
        return;
    if (mMode == Mode::Copy)
        emit copyRequested(directory);
    else
        emit moveRequested(directory);
}

bool CopyTargetsModel::activateShortcut(const QString &key) {
    for (int row = 0; row < mTargets.size(); ++row) {
        if (shortcutOf(row) == key) {
            activate(row);
            return true;
        }
    }
    return false;
}

void CopyTargetsModel::setDirectory(int row, const QString &directory) {
    if (!isValidRow(row)) {
        qWarning() << "CopyTargetsModel::setDirectory: no row" << row;
        return;
    }
    // The QML folder picker hands over file URLs.
    const QUrl url(directory);
    const QString path = url.isLocalFile() ? url.toLocalFile() : directory;
    if (path.isEmpty() || mTargets.at(row) == path)
        return;
    mTargets[row] = path;
    const QModelIndex changed = index(row);
    emit dataChanged(changed, changed, {DirectoryRole, DisplayNameRole});
    emit targetsEdited(savableCopyTargets(mTargets));
}

bool CopyTargetsModel::isValidRow(int row) const {
    return row >= 0 && row < mTargets.size();
}
