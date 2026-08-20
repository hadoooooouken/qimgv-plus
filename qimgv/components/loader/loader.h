#pragma once

#include <stop_token>

#include <QHash>
#include <QPointer>
#include <QThreadPool>
#include "components/cache/thumbnailcache.h"
#include "loaderrunnable.h"

class Loader : public QObject {
    Q_OBJECT
public:
    explicit Loader();
    ~Loader();
    std::shared_ptr<Image> load(QString path);
    void loadAsyncPriority(QString path);
    void loadAsync(QString path);

    void clearTasks();
    bool isBusy() const;
    bool isLoading(QString path);
private:
    enum class TaskPhase {
        Queued,
        CancellationRequested,
    };

    struct TaskRecord {
        QString path;
        std::stop_source cancellationSource;
        QPointer<LoaderRunnable> runnable;
        TaskPhase phase = TaskPhase::Queued;
    };

    QHash<QString, quint64> taskIdsByPath;
    QHash<quint64, TaskRecord> tasks;
    quint64 mNextTaskId = 0;
    QThreadPool *pool;    
    [[nodiscard]] quint64 nextTaskId();
    void cancelTasks();
    void doLoadAsync(QString path, int priority);

signals:
    void loadFinished(std::shared_ptr<Image>, const QString &path);
    void loadFailed(const QString &path);

private slots:
    void onLoadFinished(quint64 taskId, std::shared_ptr<Image> image);
};
