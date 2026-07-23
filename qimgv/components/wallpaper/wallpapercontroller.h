#pragma once

#include <QObject>
#include <QPointer>
#include <QImage>
#include <QThread>
#include <QSize>
#include <QString>
#include <memory>
#include <atomic>

class MW;

class WallpaperController : public QObject {
    Q_OBJECT
public:
    explicit WallpaperController(QObject *parent = nullptr);
    ~WallpaperController() override;

    void setWallpaper(std::shared_ptr<const QImage> sourceImage, MW *mw);
    void cancelActiveTask();

private:
    QThread *m_workerThread = nullptr;
    std::shared_ptr<std::atomic<bool>> m_cancelToken;
    QString m_currentWallpaperPath;

    void cleanupFile(const QString &path);
};
