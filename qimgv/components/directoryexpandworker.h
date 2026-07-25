#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <atomic>

// Cancellable, off-UI-thread expansion of a directory-view selection into
// the full list of matching files.
//
// This is a plain QObject meant to be moveToThread()'d, driven by run(), and
// stoppable at any point via requestStop(). Execution lifecycle and cancellation
// are tracked independently so a stop requested before run() cannot be lost.
// Matches are streamed back to the caller in bounded batches through pathsReady()
// rather than accumulated into a single QList<QString> and returned only once
// the whole subtree has been walked, so a directory containing a huge number of
// files never forces one unbounded, uninterruptible pass over the filesystem.
class DirectoryExpandWorker : public QObject {
    Q_OBJECT
public:
    // One entry from DirectoryPresenter::selectedPaths(), already
    // classified as a plain file or a directory by the caller (a cheap
    // in-memory DirectoryModel lookup) so this worker never has to touch
    // DirectoryModel - which lives on the UI thread - from a background
    // thread.
    struct SelectedEntry {
        QString path;
        bool isDirectory = false;
    };

    // Caps how many discovered paths accumulate before being flushed via
    // pathsReady(), bounding the peak memory/signal-payload size of a
    // single batch regardless of how large the scanned subtree is.
    static constexpr int MAX_BATCH_SIZE = 512;

    DirectoryExpandWorker(QList<SelectedEntry> entries, QString formatsRegex);

    bool isWorkerRunning() const;

public slots:
    void run();
    void requestStop();

signals:
    // Emitted (possibly many times) from the worker's own thread as
    // matching paths are discovered. Cross-thread connections default to
    // Qt::QueuedConnection, so batches are always handled on the
    // receiver's thread.
    void pathsReady(QList<QString> paths);
    void finished();
    void error(QString errorMessage);

private:
    bool isCancellationRequested() const;

    QList<SelectedEntry> entries;
    QString formatsRegex;
    std::atomic<bool> isRunning{false};
    // Monotonic per worker instance: requestStop() is the only writer.
    std::atomic<bool> cancellationRequested{false};
};
