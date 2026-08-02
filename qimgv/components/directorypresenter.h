#pragma once

#include <QObject>
#include <memory>
#include "gui/idirectoryview.h"
#include "components/foldercoverresolver.h"
#include "components/thumbnailer/thumbnailer.h"
#include "directorymodel.h"
#include "sharedresources.h"
#include "directoryexpandworker.h"
#include <QColor>
#include <QHash>
#include <QMap>
#include <QMimeData>
#include <QPointer>
#include <QThread>
#include <QtSvg/QSvgRenderer>

#include <tuple>

class DirectoryPresenter : public QObject {
    Q_OBJECT
public:
    explicit DirectoryPresenter(QObject *parent = nullptr);
    ~DirectoryPresenter() override;

    void setView(std::shared_ptr<IDirectoryView>);
    void setModel(std::shared_ptr<DirectoryModel> newModel);
    void unsetModel();
    // Lets multiple DirectoryPresenter instances (e.g. the thumbnail panel
    // and the folder view, which always mirror the same DirectoryModel)
    // share one Thumbnailer. Sharing makes Thumbnailer's own request
    // deduplication (queuedTasks/runningTasks/pendingReruns) effective
    // across both presenters instead of just within each one, so opening a
    // folder no longer decodes+resizes every image twice in parallel.
    // Must be called before the presenter starts requesting thumbnails
    // (i.e. right after construction) - swapping it later would silently
    // drop the connection for any thumbnail already in flight on the
    // previous instance.
    void setThumbnailer(std::shared_ptr<Thumbnailer> newThumbnailer);

    void selectAndFocus(int index);
    void selectAndFocus(QString path);

    void onFileRemoved(QString filePath, int index);
    void onFileRenamed(QString fromPath, int indexFrom, QString toPath, int indexTo);
    void onFileAdded(QString filePath);
    void onFileModified(QString filePath);

    void onDirRemoved(QString dirPath, int index);
    void onDirRenamed(QString fromPath, int indexFrom, QString toPath, int indexTo);
    void onDirAdded(QString dirPath);

    bool showDirs();
    void setShowDirs(bool mode);

    QList<QString> selectedPaths() const;
    QString firstSelectedDirectoryPath() const;
    int upArrowCount() const;

    // Asynchronous counterpart of the old (now removed) synchronous
    // expandedSelectedPaths(): expands the current selection into a bounded
    // list of matching files on the same background DirectoryExpandWorker
    // used by onItemActivated()/onOpenSelectedRequested() (see the
    // ExpandPurpose comment below), instead of walking the selected
    // directories synchronously on the UI thread. For
    // Core::showBatchConverter(), which needs the complete file list up
    // front rather than the activation-signal semantics the other two
    // call sites use. Emits expandedSelectedPathsReady() once a scan within
    // DirectoryExpandWorker::MAX_RESULT_COUNT completes; emits
    // selectionExpansionFailed() instead of returning a partial list if the
    // limit is exceeded.
    void requestExpandedSelectedPathsAsync();


signals:
    void fileActivated(QString filePath);
    void filesActivated(QList<QString> filePaths, QString activePath);
    void dirActivated(QString dirPath);
    void draggedOut(QList<QString>);
    void droppedInto(QList<QString>, QString, Qt::DropAction);
    void backRequested();
    void forwardRequested();
    // Delivered from onExpandScanFinished() once a
    // requestExpandedSelectedPathsAsync() scan completes with a non-empty
    // result (see the BatchConvert case in ExpandPurpose below).
    void expandedSelectedPathsReady(QList<QString> filePaths, QString defaultOutputDir);
    // A user-facing failure for the current expansion. Partial paths are
    // discarded before this is emitted.
    void selectionExpansionFailed(QString errorMessage);

public slots:
    void disconnectView();
    void reloadModel();

private slots:
    void onSettingsChanged();
    void generateThumbnails(QList<int>, int, bool, bool);
    void onThumbnailReady(std::shared_ptr<Thumbnail> thumb, QString filePath);
    void onThumbnailFailed(QString filePath, int size);
    void onFolderCoverResolved(FolderCoverResult result);
    void populateView();
    void onItemActivated(int absoluteIndex);
    void onOpenSelectedRequested();
    void onDraggedOut();
    void onDraggedOver(int index);

    void onDroppedInto(const QMimeData *data, QObject *source, int targetIndex, Qt::DropAction action);

    // Delivered from the background DirectoryExpandWorker thread (queued
    // connection) with the immutable generation captured for that launch.
    // See startExpandedPathsScan()/launchExpandScan() below.
    void onExpandBatchReady(quint64 generation, QList<QString> paths);
    void onExpandScanFinished(quint64 generation);
private:
    std::shared_ptr<IDirectoryView> view = nullptr;
    std::shared_ptr<DirectoryModel> model = nullptr;
    std::shared_ptr<Thumbnailer> thumbnailer;
    std::unique_ptr<FolderCoverResolver> folderCoverResolver;
    bool mShowDirs;

    struct PendingFolderThumbnail {
        QString folderPath;
        int thumbnailSize = 0;
        quint64 generation = 0;
    };

    struct DefaultFolderIconKey {
        int thumbnailSize = 0;
        qreal devicePixelRatio = 1.0;
        QRgb color = {};

        friend bool operator<(const DefaultFolderIconKey &left,
                              const DefaultFolderIconKey &right)
        {
            return std::tie(left.thumbnailSize, left.devicePixelRatio,
                            left.color) <
                   std::tie(right.thumbnailSize, right.devicePixelRatio,
                            right.color);
        }
    };

    QHash<QString, QList<PendingFolderThumbnail>> dirThumbnailTasks;
    QSvgRenderer folderIconRenderer;
    QMap<DefaultFolderIconKey, std::shared_ptr<QPixmap>>
        defaultFolderIconCache;
    quint64 folderThumbnailGeneration = {};
    SortingMode lastFolderIconSort;
    int lastFolderViewIconSize;

    std::shared_ptr<QPixmap> defaultFolderPixmap(int size);
    std::shared_ptr<Thumbnail>
    defaultFolderThumbnail(int size, const QString &dirName);
    std::shared_ptr<Thumbnail> composeFolderThumbnail(
        int size, const QString &dirName, const QPixmap &innerThumb);
    std::shared_ptr<Thumbnail> composeUpArrowThumbnail(int size);
    static QString thumbnailPathKey(const QString &path);
    int directoryIndexForPath(const QString &path) const;
    void invalidateFolderThumbnailRequests();

    // ---- asynchronous, cancellable selection expansion ----
    // Backs onItemActivated()/onOpenSelectedRequested()/
    // requestExpandedSelectedPathsAsync() with a background
    // DirectoryExpandWorker (see directoryexpandworker.h) instead of a
    // synchronous QDirIterator scan on the UI thread, so activating a
    // multi-selection (or opening the batch converter) on a large
    // directory tree no longer freezes the UI thread.

    // Distinguishes which call site requested the scan, so
    // onExpandScanFinished() knows how to turn the resulting file list
    // back into the same signal that call site used to emit inline.
    enum class ExpandPurpose {
        ItemActivation, // from onItemActivated(): may fall back to a single fileActivated/dirActivated
        OpenSelected,   // from onOpenSelectedRequested(): emits filesActivated(), nothing, or a failure
        BatchConvert    // from requestExpandedSelectedPathsAsync(): emits ready, nothing, or a failure
    };

    struct PendingExpandContext {
        ExpandPurpose purpose = ExpandPurpose::OpenSelected;
        int fallbackAbsoluteIndex = -1;
        QString fallbackActivePath;
        // Only populated (in launchExpandScan()) when purpose == BatchConvert.
        QString batchConvertDefaultOutputDir;
        // Assigned once when the request is made. Every queued worker callback
        // carries this value so it cannot be mistaken for a later request.
        quint64 generation = {};
        // Non-empty only when the worker rejected the scan. A failed scan
        // never exposes its partial aggregate to activation or batch conversion.
        QString failureMessage;
    };

    struct ExpandFailure {
        quint64 generation = {};
        QString errorMessage;
    };

    // Starts a scan, or - if one is already running - cancels it and
    // queues this request to start once the previous one has actually
    // stopped (see onExpandScanFinished()).
    void startExpandedPathsScan(ExpandPurpose purpose, int fallbackAbsoluteIndex,
                                 const QString &fallbackActivePath);
    void launchExpandScan(const PendingExpandContext &ctx);
    QString expandResultLimitErrorMessage(int resultLimit) const;
    void onExpandScanFailed(ExpandFailure failure);
    // Requests cancellation of any scan in progress and drops any queued
    // restart; safe to call unconditionally (destructor, unsetModel()).
    void cancelExpandedPathsScan();

    QPointer<QThread> expandThread;
    QPointer<DirectoryExpandWorker> expandWorker;
    QList<QString> expandAccumulatedPaths;
    PendingExpandContext expandContext;
    bool expandRestartPending = false;
    PendingExpandContext expandNextContext;

    // Incremented for every request and hard cancellation. The current value
    // identifies the only request allowed to append batches or consume a
    // completion, so callbacks queued before model replacement are harmless.
    quint64 expandEpoch = {};
};
