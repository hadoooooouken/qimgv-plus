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

class MW;

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

    void setWallpaper(std::shared_ptr<const QImage> sourceImage, MW *mw);
    void cancelActiveTask();

signals:
    void wallpaperApplyFinished(WallpaperApplyResult result);
    void wallpaperFileCleanupFailed(QString path);

private:
    struct WallpaperRequestState;

    std::unique_ptr<QThread> m_workerThread;
    std::shared_ptr<WallpaperRequestState> m_activeRequest;

    void stopActiveTask(bool reportCleanupFailure);
    bool finalizeRequest(const std::shared_ptr<WallpaperRequestState> &request,
                         bool reportCleanupFailure);
    bool cleanupFile(const QString &path, bool reportCleanupFailure);
};
