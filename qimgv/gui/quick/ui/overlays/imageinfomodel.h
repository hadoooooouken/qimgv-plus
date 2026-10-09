#pragma once

#include <QAbstractListModel>
#include <QtQml/qqmlregistration.h>

#include "gui/ports/shellport.h"

// Metadata of the current image for the image info overlay
// (ImageInfoOverlay.qml), in the order Core delivers it. Roles: name, value,
// stacked (the value is long and shown below its name across the whole
// panel instead of in the value column, like EntryInfoItem).
//
// Owned by OverlayCoordinator. GUI thread only.
class ImageInfoModel final : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by OverlayCoordinator")
    Q_PROPERTY(bool empty READ isEmpty NOTIFY emptyChanged FINAL)

public:
    enum Role { NameRole = Qt::UserRole + 1, ValueRole, StackedRole };
    Q_ENUM(Role)

    // Values longer than this are stacked (ImageInfoOverlay).
    static constexpr int kStackedValueThreshold = 100;

    explicit ImageInfoModel(QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] bool isEmpty() const;
    void setEntries(const MetadataEntries &entries);

signals:
    void emptyChanged();

private:
    MetadataEntries mEntries;
};
