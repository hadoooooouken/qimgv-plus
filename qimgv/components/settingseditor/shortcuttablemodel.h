#pragma once

#include <QAbstractTableModel>

#include <optional>

#include "settingsvalues.h"

// The shortcut table of the settings dialog ("Controls"): one row per
// shortcut, sorted by action (rows of the same action keep their order, new
// ones go last), columns Action and Shortcut. A shortcut is bound once:
// putting a shortcut removes the row that had it before. Edits stay in the
// table until the dialog applies them.
//
// QML reads the roles `action` and `shortcut`. GUI thread only.
class ShortcutTableModel final : public QAbstractTableModel {
    Q_OBJECT

public:
    enum Column { ActionColumn, ShortcutColumn, ColumnCount };
    enum Role { ActionRole = Qt::UserRole + 1, ShortcutRole };

    explicit ShortcutTableModel(QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] int columnCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation,
                                      int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] const ShortcutList &entries() const;
    // Replaces the table; entries are sorted by action.
    void setEntries(const ShortcutList &entries);
    // The row at row, or std::nullopt when there is none.
    [[nodiscard]] std::optional<ShortcutEntry> entryAt(int row) const;

    // Binds shortcut to action: removes the row replacedRow (an edited row;
    // -1 for none) and the row that had shortcut, then inserts the entry.
    // Returns the row of the new entry, or -1 (nothing changed) when action
    // or shortcut is empty.
    int put(const ShortcutEntry &entry, int replacedRow = -1);
    // Removes the row at row; false when there is none.
    bool removeAt(int row);

private:
    [[nodiscard]] int insertionRow(const QString &action) const;

    ShortcutList mEntries;
};
