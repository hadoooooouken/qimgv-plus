#pragma once

#include <QObject>
#include <QPointer>
#include <QImage>
#include <QMetaType>
#include <QThread>
#include <QSize>
#include <QString>
#include <QtTypes>
#include <memory>
#include <atomic>

#include "gui/ports/notificationport.h"

enum class WallpaperApplyError {
    None,
    StorageDirectoryCreationFailed,
    RegistryOpenFailed,
    WallpaperStyleWriteFailed,
    TileWallpaperWriteFailed,
    RegistryCloseFailed,
    SystemParametersInfoFailed
};

struct WallpaperApplyResult {
    WallpaperApplyError error = WallpaperApplyError::None;
    quint32 nativeError = 0;

    [[nodiscard]] bool succeeded() const noexcept {
        return error == WallpaperApplyError::None;
    }
};

Q_DECLARE_METATYPE(WallpaperApplyResult)

class WallpaperController : public QObject {
    Q_OBJECT
public:
    explicit WallpaperController(QObject *parent = nullptr);
    ~WallpaperController() override;

    void setWallpaper(std::shared_ptr<const QImage> sourceImage);
    void cancelActiveTask();

signals:
    void wallpaperApplyFinished(WallpaperApplyResult result);
    void wallpaperFileCleanupFailed(QString path);
    // Progress and early-failure messages for the person. Always emitted on
    // the GUI thread.
    void notificationRequested(NotificationRequest request);

private:
    struct WallpaperRequestState;

    std::unique_ptr<QThread> m_workerThread;
    std::shared_ptr<WallpaperRequestState> m_activeRequest;

    // Thread-safe: queues notificationRequested() onto this object's thread.
    void postNotification(const NotificationRequest &request);
    void stopActiveTask(bool reportCleanupFailure);
    bool finalizeRequest(const std::shared_ptr<WallpaperRequestState> &request,
                         bool reportCleanupFailure);
    bool cleanupFile(const QString &path, bool reportCleanupFailure);
};
