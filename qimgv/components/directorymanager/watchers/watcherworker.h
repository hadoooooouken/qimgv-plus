#pragma once

#include <QObject>
#include <atomic>

class WatcherWorker : public QObject
{
    Q_OBJECT
public:
    WatcherWorker();
    virtual void run() = 0;

public slots:
    void setRunning(bool running);

signals:
    void error(const QString& errorMessage);
    void started();
    void finished();

protected:
    std::atomic<bool> isRunning;
};
