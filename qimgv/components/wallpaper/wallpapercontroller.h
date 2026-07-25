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

private:
    QThread *m_workerThread = nullptr;
    std::shared_ptr<std::atomic<bool>> m_cancelToken;
    QString m_currentWallpaperPath;

    void cleanupFile(const QString &path);
};
