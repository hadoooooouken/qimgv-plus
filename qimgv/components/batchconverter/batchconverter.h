#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QSize>
#include <QThreadPool>
#include <QList>
#include <QSet>
#include <atomic>
#include <memory>

#include "batchjob.h"

class BatchConverter : public QObject {
    Q_OBJECT

public:
    explicit BatchConverter(QObject *parent = nullptr);
    ~BatchConverter();

    void start(const QStringList &allPaths, const QList<int> &selectedIndices, const BatchJob &job);
    void cancel();
    void cancelAndWait();
    void onRunnableCreated();
    void onRunnableDestroyed();
    void enableSelfDestruct();
    int inFlightTasksCount() const { return m_inFlightTasksCount.load(); }

    bool isRunning() const { return m_isConverting; }
    bool isCancelling() const { return m_isCancelling; }

signals:
    // State of the file at index (of the paths passed to start()) and what
    // happened to it (the output or the reason it failed); emitted with
    // BatchItemState::Processing when the file is handed to a worker.
    void progressUpdated(int index, BatchItemState state, QString details);
    void finished(int successCount, int failedCount, int totalCount);
    void cancelled(int successCount, int failedCount, int totalCount);
    void startFailed(const QString &reason);

private slots:
    void onTaskFinished(int index, BatchItemState state, QString details);
    void finishCancellation();

private:
    QString buildDestPath(const QString &srcPath, const QString &pattern, int index, const QString &formatExt, const QString &finalOutDir) const;
    QString makeUniqueDestPath(const QString &destPath,
                               const QSet<QString> &reservedDestinationKeys,
                               bool overwrite) const;
    QString createUniqueSubfolder(const QString &baseDir) const;

    QThreadPool m_threadPool;
    std::atomic<bool> m_isConverting{false};
    std::atomic<bool> m_isCancelling{false};
    std::shared_ptr<std::atomic<bool>> m_cancelFlag;
    std::atomic<int> m_inFlightTasksCount{0};
    std::atomic<bool> m_selfDestructOnFinished{false};
    std::atomic<bool> m_deleteLaterCalled{false};

    int m_totalFiles = 0;
    int m_processedFiles = 0;
    int m_successCount = 0;
    int m_failedCount = 0;
};
