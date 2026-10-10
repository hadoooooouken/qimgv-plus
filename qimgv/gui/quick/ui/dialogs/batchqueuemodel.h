#pragma once

#include <QAbstractListModel>
#include <QImage>
#include <QList>
#include <QMultiHash>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QThreadPool>
#include <QtQml/qqmlregistration.h>

#include <atomic>
#include <memory>

#include "components/batchconverter/batchjob.h"
#include "gui/quick/render/thumbnailitem.h"

// Header data of one queued file, read on a worker thread.
struct BatchSourceInfo {
    QString format;
    QSize imageSize;
    qint64 fileSize = 0;
};

// The files of the batch converter dialog (BatchItemWidget rows of the
// widget dialog): name, source format / size, whether the file is selected
// for the batch, its state and what happened to it, and its thumbnail.
//
// The image headers and file sizes are read on a worker thread of the model
// and applied in groups, so a large selection does not hold up the window;
// a reset or clear() drops the results of the previous files. GUI thread
// only.
class BatchQueueModel final : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by BatchConverterDialogModel")
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged FINAL)
    Q_PROPERTY(int selectedCount READ selectedCount NOTIFY selectionChanged FINAL)
    Q_PROPERTY(QString selectionText READ selectionText NOTIFY selectionChanged FINAL)

public:
    enum Role {
        NameRole = Qt::UserRole + 1,
        SourceInfoRole,
        CheckedRole,
        StateRole,
        StateTextRole,
        DetailsRole,
        ThumbnailRole
    };
    Q_ENUM(Role)

    // BatchItemState for QML.
    enum ItemState {
        Pending = static_cast<int>(BatchItemState::Pending),
        Processing = static_cast<int>(BatchItemState::Processing),
        Done = static_cast<int>(BatchItemState::Done),
        Failed = static_cast<int>(BatchItemState::Failed),
        Stopped = static_cast<int>(BatchItemState::Stopped)
    };
    Q_ENUM(ItemState)

    explicit BatchQueueModel(QObject *parent = nullptr);
    ~BatchQueueModel() override;

    [[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    // Queues paths, all selected and pending, and starts reading their
    // headers.
    void reset(const QStringList &paths);
    void clear();

    [[nodiscard]] QStringList paths() const;
    [[nodiscard]] QList<int> selectedRows() const;
    [[nodiscard]] int selectedCount() const;
    [[nodiscard]] QString selectionText() const;
    // True once every queued header was read.
    [[nodiscard]] bool sourceInfoComplete() const;

    void setItemState(int row, BatchItemState state, const QString &details);
    // Sets the thumbnail of every row of path.
    void setThumbnail(const QString &path, const QImage &image);

    Q_INVOKABLE void setChecked(int row, bool checked);
    Q_INVOKABLE void setAllChecked(bool checked);

signals:
    void countChanged();
    void selectionChanged();
    void sourceInfoCompleted();

private:
    struct Item {
        QString path;
        QString name;
        bool checked = true;
        BatchItemState state = BatchItemState::Pending;
        QString details;
        bool hasSourceInfo = false;
        BatchSourceInfo sourceInfo;
        QImage thumbnail;
    };

    struct SourceInfoResult {
        int row = 0;
        BatchSourceInfo info;
    };

    void startSourceScan();
    void cancelSourceScan();
    void applySourceInfo(quint64 generation, const QList<SourceInfoResult> &results);
    void notifyRowChanged(int row, const QList<int> &roles);
    [[nodiscard]] bool isValidRow(int row) const;

    QList<Item> mItems;
    QMultiHash<QString, int> mRowsByPath;
    int mSelectedCount = 0;
    qint64 mSelectedBytes = 0;
    int mSourceInfoCount = 0;
    // Identity of the running header scan; results of another scan are
    // dropped.
    quint64 mScanGeneration = 0;
    std::shared_ptr<std::atomic<bool>> mScanCancelled;
    // Declared last: destroyed first, waiting for a running scan before the
    // rest of the model goes away.
    QThreadPool mScanPool;
};
