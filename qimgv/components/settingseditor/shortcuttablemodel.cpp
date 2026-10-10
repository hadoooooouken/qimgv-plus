#include "shortcuttablemodel.h"

#include <QCoreApplication>

#include <algorithm>

namespace {
constexpr char kContext[] = "SettingsDialog";

bool actionLess(const ShortcutEntry &a, const ShortcutEntry &b) {
    return a.action < b.action;
}
} // namespace

ShortcutTableModel::ShortcutTableModel(QObject *parent) : QAbstractTableModel(parent) {}

int ShortcutTableModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : static_cast<int>(mEntries.size());
}

int ShortcutTableModel::columnCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant ShortcutTableModel::data(const QModelIndex &index, int role) const {
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return {};
    const ShortcutEntry &entry = mEntries.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
        return index.column() == ActionColumn ? entry.action : entry.shortcut;
    case Qt::TextAlignmentRole:
        return QVariant::fromValue(Qt::Alignment(Qt::AlignCenter));
    case ActionRole:
        return entry.action;
    case ShortcutRole:
        return entry.shortcut;
    default:
        return {};
    }
}

QVariant ShortcutTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QAbstractTableModel::headerData(section, orientation, role);
    if (section == ActionColumn)
        return QCoreApplication::translate(kContext, QT_TRANSLATE_NOOP("SettingsDialog", "Action"));
    if (section == ShortcutColumn)
        return QCoreApplication::translate(kContext, QT_TRANSLATE_NOOP("SettingsDialog", "Shortcut"));
    return {};
}

QHash<int, QByteArray> ShortcutTableModel::roleNames() const {
    return {{ActionRole, "action"}, {ShortcutRole, "shortcut"}};
}

const ShortcutList &ShortcutTableModel::entries() const {
    return mEntries;
}

void ShortcutTableModel::setEntries(const ShortcutList &entries) {
    beginResetModel();
    mEntries = entries;
    std::ranges::stable_sort(mEntries, actionLess);
    endResetModel();
}

std::optional<ShortcutEntry> ShortcutTableModel::entryAt(int row) const {
    if (row < 0 || row >= mEntries.size())
        return std::nullopt;
    return mEntries.at(row);
}

int ShortcutTableModel::put(const ShortcutEntry &entry, int replacedRow) {
    if (entry.action.isEmpty() || entry.shortcut.isEmpty())
        return -1;
    removeAt(replacedRow);
    const auto bound = std::ranges::find(mEntries, entry.shortcut, &ShortcutEntry::shortcut);
    if (bound != mEntries.cend())
        removeAt(static_cast<int>(std::distance(mEntries.begin(), bound)));
    const int row = insertionRow(entry.action);
    beginInsertRows({}, row, row);
    mEntries.insert(row, entry);
    endInsertRows();
    return row;
}

bool ShortcutTableModel::removeAt(int row) {
    if (row < 0 || row >= mEntries.size())
        return false;
    beginRemoveRows({}, row, row);
    mEntries.removeAt(row);
    endRemoveRows();
    return true;
}

int ShortcutTableModel::insertionRow(const QString &action) const {
    const auto position = std::ranges::upper_bound(mEntries, action, std::less<>{},
                                                   &ShortcutEntry::action);
    return static_cast<int>(std::distance(mEntries.cbegin(), position));
}
