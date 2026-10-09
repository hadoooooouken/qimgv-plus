#pragma once

#include <QAbstractListModel>
#include <QString>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

// Destination folders of the copy / move overlay (CopyOverlay.qml), with the
// digit shortcuts 1 - 9 of the widget overlay. Roles: directory (full path),
// displayName (last path segment), shortcut ("1" - "9"). Activating a row
// requests the copy or move of the current file there; the folder of a row
// can be replaced, which publishes the list to save.
//
// Owned by OverlayCoordinator. GUI thread only.
class CopyTargetsModel final : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by OverlayCoordinator")
    Q_PROPERTY(Mode mode READ mode NOTIFY modeChanged FINAL)

public:
    enum class Mode { Copy, Move };
    Q_ENUM(Mode)

    enum Role { DirectoryRole = Qt::UserRole + 1, DisplayNameRole, ShortcutRole };
    Q_ENUM(Role)

    explicit CopyTargetsModel(QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] Mode mode() const;
    void setMode(Mode mode);
    // True once setTargets() was called.
    [[nodiscard]] bool isLoaded() const;
    // Shows targets; only the first kMaxCopyTargets are used.
    void setTargets(const QStringList &targets);
    [[nodiscard]] QStringList targets() const;

    // Requests the operation of mode() into the folder of row.
    Q_INVOKABLE void activate(int row);
    // Activates the row whose shortcut is key (a key name such as "3");
    // returns false when no row has it.
    Q_INVOKABLE bool activateShortcut(const QString &key);
    // Replaces the folder of row (folder picker).
    Q_INVOKABLE void setDirectory(int row, const QString &directory);

signals:
    void modeChanged();
    void copyRequested(const QString &directory);
    void moveRequested(const QString &directory);
    // The folders after an edit, without empty and repeated entries.
    void targetsEdited(const QStringList &targets);

private:
    [[nodiscard]] bool isValidRow(int row) const;

    QStringList mTargets;
    Mode mMode = Mode::Copy;
    bool mLoaded = false;
};
