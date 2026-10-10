#include "batchqueuemodel.h"

#include <QDebug>
#include <QFileInfo>
#include <QImageReader>

#include "components/batchconverter/batchjobrules.h"

namespace {
// Headers are read one file after the other; one thread keeps the disk
// access sequential.
constexpr int kScanThreads = 1;
// Results are handed to the GUI thread in groups of this many files.
constexpr qsizetype kSourceInfoGroupSize = 32;

BatchSourceInfo readSourceInfo(const QString &path) {
    QImageReader reader(path);
    return {.format = QString::fromLatin1(reader.format()),
            .imageSize = reader.size(),
            .fileSize = QFileInfo(path).size()};
}
} // namespace

BatchQueueModel::BatchQueueModel(QObject *parent)
    : QAbstractListModel(parent), mScanCancelled(std::make_shared<std::atomic<bool>>(false)) {
    mScanPool.setMaxThreadCount(kScanThreads);
}

BatchQueueModel::~BatchQueueModel() {
    cancelSourceScan();
}

int BatchQueueModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : static_cast<int>(mItems.size());
}

QVariant BatchQueueModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || !isValidRow(index.row()))
        return {};
    const Item &item = mItems[index.row()];
    switch (role) {
    case NameRole:
        return item.name;
    case SourceInfoRole:
        return item.hasSourceInfo
                   ? BatchJobRules::sourceInfoText(item.sourceInfo.format,
                                                   item.sourceInfo.imageSize,
                                                   item.sourceInfo.fileSize)
                   : QString();
    case CheckedRole:
        return item.checked;
    case StateRole:
        return static_cast<int>(item.state);
    case StateTextRole:
        return BatchJobRules::itemStateText(item.state);
    case DetailsRole:
        return item.details;
    case ThumbnailRole:
        return QVariant::fromValue(
            ThumbnailHandle{.image = item.thumbnail, .sourceSize = item.sourceInfo.imageSize});
    default:
        return {};
    }
}

QHash<int, QByteArray> BatchQueueModel::roleNames() const {
    return {{NameRole, "name"},
            {SourceInfoRole, "sourceInfo"},
            {CheckedRole, "checked"},
            {StateRole, "itemState"},
            {StateTextRole, "stateText"},
            {DetailsRole, "details"},
            {ThumbnailRole, "thumbnail"}};
}

void BatchQueueModel::reset(const QStringList &paths) {
    cancelSourceScan();
    beginResetModel();
    mItems.clear();
    mRowsByPath.clear();
    mItems.reserve(paths.size());
    for (const QString &path : paths) {
        mRowsByPath.insert(path, static_cast<int>(mItems.size()));
        mItems.append({.path = path, .name = QFileInfo(path).fileName()});
    }
    mSelectedCount = static_cast<int>(mItems.size());
    mSelectedBytes = 0;
    mSourceInfoCount = 0;
    endResetModel();
    emit countChanged();
    emit selectionChanged();
    startSourceScan();
}

void BatchQueueModel::clear() {
    reset({});
}

QStringList BatchQueueModel::paths() const {
    QStringList result;
    result.reserve(mItems.size());
    for (const Item &item : mItems)
        result.append(item.path);
    return result;
}

QList<int> BatchQueueModel::selectedRows() const {
    QList<int> rows;
    for (int row = 0; row < mItems.size(); ++row) {
        if (mItems[row].checked)
            rows.append(row);
    }
    return rows;
}

int BatchQueueModel::selectedCount() const {
    return mSelectedCount;
}

QString BatchQueueModel::selectionText() const {
    return BatchJobRules::selectionText(mSelectedCount, mSelectedBytes);
}

bool BatchQueueModel::sourceInfoComplete() const {
    return mSourceInfoCount == mItems.size();
}

void BatchQueueModel::setItemState(int row, BatchItemState state, const QString &details) {
    if (!isValidRow(row)) {
        qWarning() << "Batch converter: a state was reported for row" << row << "of"
                   << mItems.size();
        return;
    }
    Item &item = mItems[row];
    item.state = state;
    item.details = details;
    notifyRowChanged(row, {StateRole, StateTextRole, DetailsRole});
}

void BatchQueueModel::setThumbnail(const QString &path, const QImage &image) {
    const QList<int> rows = mRowsByPath.values(path);
    for (const int row : rows) {
        mItems[row].thumbnail = image;
        notifyRowChanged(row, {ThumbnailRole});
    }
}

void BatchQueueModel::setChecked(int row, bool checked) {
    if (!isValidRow(row) || mItems[row].checked == checked)
        return;
    Item &item = mItems[row];
    item.checked = checked;
    const int sign = checked ? 1 : -1;
    mSelectedCount += sign;
    if (item.hasSourceInfo)
        mSelectedBytes += sign * item.sourceInfo.fileSize;
    notifyRowChanged(row, {CheckedRole});
    emit selectionChanged();
}

void BatchQueueModel::setAllChecked(bool checked) {
    if (mItems.isEmpty())
        return;
    mSelectedCount = checked ? static_cast<int>(mItems.size()) : 0;
    mSelectedBytes = 0;
    for (Item &item : mItems) {
        item.checked = checked;
        if (checked && item.hasSourceInfo)
            mSelectedBytes += item.sourceInfo.fileSize;
    }
    emit dataChanged(index(0), index(static_cast<int>(mItems.size()) - 1), {CheckedRole});
    emit selectionChanged();
}

void BatchQueueModel::startSourceScan() {
    if (mItems.isEmpty())
        return;
    const quint64 generation = ++mScanGeneration;
    mScanCancelled = std::make_shared<std::atomic<bool>>(false);
    const std::shared_ptr<std::atomic<bool>> cancelled = mScanCancelled;
    const QStringList scanPaths = paths();
    mScanPool.start([this, generation, cancelled, scanPaths]() {
        QList<SourceInfoResult> group;
        group.reserve(kSourceInfoGroupSize);
        const auto post = [this, generation, &group]() {
            QMetaObject::invokeMethod(
                this,
                [this, generation, results = std::move(group)]() {
                    applySourceInfo(generation, results);
                },
                Qt::QueuedConnection);
            group = {};
            group.reserve(kSourceInfoGroupSize);
        };
        for (int row = 0; row < scanPaths.size(); ++row) {
            if (cancelled->load())
                return;
            group.append({.row = row, .info = readSourceInfo(scanPaths[row])});
            if (group.size() == kSourceInfoGroupSize)
                post();
        }
        if (!group.isEmpty())
            post();
    });
}

void BatchQueueModel::cancelSourceScan() {
    mScanCancelled->store(true);
    ++mScanGeneration;
}

void BatchQueueModel::applySourceInfo(quint64 generation, const QList<SourceInfoResult> &results) {
    if (generation != mScanGeneration)
        return;
    bool selectionMoved = false;
    for (const SourceInfoResult &result : results) {
        if (!isValidRow(result.row))
            continue;
        Item &item = mItems[result.row];
        item.sourceInfo = result.info;
        item.hasSourceInfo = true;
        ++mSourceInfoCount;
        if (item.checked) {
            mSelectedBytes += result.info.fileSize;
            selectionMoved = true;
        }
        notifyRowChanged(result.row, {SourceInfoRole, ThumbnailRole});
    }
    if (selectionMoved)
        emit selectionChanged();
    if (sourceInfoComplete())
        emit sourceInfoCompleted();
}

void BatchQueueModel::notifyRowChanged(int row, const QList<int> &roles) {
    const QModelIndex changed = index(row);
    emit dataChanged(changed, changed, roles);
}

bool BatchQueueModel::isValidRow(int row) const {
    return row >= 0 && row < mItems.size();
}
