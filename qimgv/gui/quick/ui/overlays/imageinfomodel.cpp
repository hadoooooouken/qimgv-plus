#include "imageinfomodel.h"

ImageInfoModel::ImageInfoModel(QObject *parent) : QAbstractListModel(parent) {}

int ImageInfoModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : static_cast<int>(mEntries.size());
}

QVariant ImageInfoModel::data(const QModelIndex &index, int role) const {
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return {};
    const auto &[name, value] = mEntries.at(index.row());
    switch (role) {
    case NameRole:
        return name;
    case ValueRole:
        return value;
    case StackedRole:
        return value.size() > kStackedValueThreshold;
    default:
        return {};
    }
}

QHash<int, QByteArray> ImageInfoModel::roleNames() const {
    return {{NameRole, "name"}, {ValueRole, "value"}, {StackedRole, "stacked"}};
}

bool ImageInfoModel::isEmpty() const {
    return mEntries.isEmpty();
}

void ImageInfoModel::setEntries(const MetadataEntries &entries) {
    const bool wasEmpty = mEntries.isEmpty();
    beginResetModel();
    mEntries = entries;
    endResetModel();
    if (wasEmpty != mEntries.isEmpty())
        emit emptyChanged();
}
