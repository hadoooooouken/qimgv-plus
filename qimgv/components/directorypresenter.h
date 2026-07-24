#pragma once

#include <QObject>
#include <memory>
#include "gui/idirectoryview.h"
#include "components/thumbnailer/thumbnailer.h"
#include "directorymodel.h"
#include "sharedresources.h"
#include "directoryexpandworker.h"
#include <QMimeData>
#include <QPointer>
#include <QThread>

//tmp
#include <QtSvg/QSvgRenderer>
#include <QDir>
#include <QImageReader>
#include <QMultiMap>

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
    // expandedSelectedPaths(): expands the current selection into the full
    // list of matching files on the same background DirectoryExpandWorker
    // used by onItemActivated()/onOpenSelectedRequested() (see the
    // ExpandPurpose comment below), instead of walking the selected
    // directories synchronously on the UI thread. For
    // Core::showBatchConverter(), which needs the complete file list up
    // front rather than the activation-signal semantics the other two
    // call sites use. Emits expandedSelectedPathsReady() once the scan
    // completes; emits nothing if the expansion is empty, matching what
    // the old synchronous helper implicitly did for its one caller.
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

public slots:
    void disconnectView();
    void reloadModel();

private slots:
    void onSettingsChanged();
    void generateThumbnails(QList<int>, int, bool, bool);
    void onThumbnailReady(std::shared_ptr<Thumbnail> thumb, QString filePath);
    void populateView();
    void onItemActivated(int absoluteIndex);
    void onOpenSelectedRequested();
    void onDraggedOut();
    void onDraggedOver(int index);

    void onDroppedInto(const QMimeData *data, QObject *source, int targetIndex, Qt::DropAction action);

    // Delivered from the background DirectoryExpandWorker thread (queued
    // connection) - see startExpandedPathsScan()/launchExpandScan() below.
    void onExpandBatchReady(QList<QString> paths);
    void onExpandScanFinished();
private:
    std::shared_ptr<IDirectoryView> view = nullptr;
    std::shared_ptr<DirectoryModel> model = nullptr;
    std::shared_ptr<Thumbnailer> thumbnailer;
    bool mShowDirs;
    QMultiMap<QString, int> dirThumbnailTasks;

    std::shared_ptr<Thumbnail> composeFolderThumbnail(int size, const QString &dirName, const QPixmap &innerThumb);
    std::shared_ptr<Thumbnail> composeUpArrowThumbnail(int size);

    // Picks the folder-cover candidate file with a single linear pass
    // instead of materializing and sorting the full entryInfoList() just
    // to read list.first() - see composeFolderThumbnail() call site.
    QString findFolderCoverImage(const QString &dirPath, const QStringList &filters,
                                  SortingMode mode) const;

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
        OpenSelected,   // from onOpenSelectedRequested(): always emits filesActivated(), or nothing
        BatchConvert    // from requestExpandedSelectedPathsAsync(): emits expandedSelectedPathsReady(), or nothing
    };

    struct PendingExpandContext {
        ExpandPurpose purpose = ExpandPurpose::OpenSelected;
        int fallbackAbsoluteIndex = -1;
        QString fallbackActivePath;
        // Only populated (in launchExpandScan()) when purpose == BatchConvert.
        QString batchConvertDefaultOutputDir;
    };

    // Starts a scan, or - if one is already running - cancels it and
    // queues this request to start once the previous one has actually
    // stopped (see onExpandScanFinished()).
    void startExpandedPathsScan(ExpandPurpose purpose, int fallbackAbsoluteIndex,
                                 const QString &fallbackActivePath);
    void launchExpandScan(const PendingExpandContext &ctx);
    // Requests cancellation of any scan in progress and drops any queued
    // restart; safe to call unconditionally (destructor, unsetModel()).
    void cancelExpandedPathsScan();

    QPointer<QThread> expandThread;
    QPointer<DirectoryExpandWorker> expandWorker;
    QList<QString> expandAccumulatedPaths;
    PendingExpandContext expandContext;
    bool expandRestartPending = false;
    PendingExpandContext expandNextContext;

    // Bumped only by a "hard" cancel (destructor, unsetModel()) - never by
    // the ordinary supersede-with-a-newer-request path in
    // startExpandedPathsScan(). onExpandScanFinished() compares the epoch
    // it was launched under against the current one and silently discards
    // the result on a mismatch, so a worker that's still unwinding after
    // model/view teardown can never emit a signal or dereference model
    // against whatever gets attached next.
    quint64 expandEpoch = 0;
    quint64 expandLaunchEpoch = 0;
};
