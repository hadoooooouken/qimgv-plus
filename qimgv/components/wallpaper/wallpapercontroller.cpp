#include "components/wallpaper/wallpapercontroller.h"
#include "gui/mainwindow.h"
#include "settings.h"
#include "components/upscaler/upscaler.h"
#include "utils/imagelib.h"

#include <QGuiApplication>
#include <QScreen>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QUuid>
#include <windows.h>
#include <array>
#include <cmath>
#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>

namespace {
constexpr wchar_t DesktopRegistryPath[] = L"Control Panel\\Desktop";
constexpr wchar_t WallpaperStyleValueName[] = L"WallpaperStyle";
constexpr wchar_t CenteredWallpaperStyle[] = L"0";
constexpr wchar_t TileWallpaperValueName[] = L"TileWallpaper";
constexpr wchar_t TileWallpaperDisabled[] = L"0";
constexpr double AspectRatioComparisonTolerance = 0.001;
constexpr int WallpaperProgressMessageDurationMs = 10'000;
constexpr int AiUpscaleProgressMessageDurationMs = 60'000;
const QString WallpaperDirectoryName = QStringLiteral("wallpapers");
const QString WallpaperFilePrefix = QStringLiteral("qimgv_wallpaper_");
const QString WallpaperFileSuffix = QStringLiteral(".png");
constexpr char WallpaperImageFormat[] = "PNG";

struct WallpaperStoragePreparation {
    QString requestPath;
    QString supersededPath;
    WallpaperApplyResult result;

    [[nodiscard]] bool succeeded() const noexcept {
        return result.succeeded();
    }
};

template<std::size_t Size>
constexpr DWORD registryStringSize(const wchar_t (&)[Size]) noexcept {
    return static_cast<DWORD>(Size * sizeof(wchar_t));
}

WallpaperApplyResult applyDesktopWallpaper(const QString &wallpaperPath) {
    HKEY registryKey = nullptr;
    const LONG openStatus = RegOpenKeyExW(HKEY_CURRENT_USER,
                                          DesktopRegistryPath,
                                          0,
                                          KEY_SET_VALUE,
                                          &registryKey);
    if (openStatus != ERROR_SUCCESS) {
        return {WallpaperApplyError::RegistryOpenFailed,
                static_cast<quint32>(openStatus)};
    }
    if (registryKey == nullptr) {
        return {WallpaperApplyError::RegistryOpenFailed,
                static_cast<quint32>(ERROR_INVALID_HANDLE)};
    }

    const LONG styleStatus = RegSetValueExW(
        registryKey,
        WallpaperStyleValueName,
        0,
        REG_SZ,
        reinterpret_cast<const BYTE *>(CenteredWallpaperStyle),
        registryStringSize(CenteredWallpaperStyle));
    const LONG tileStatus = RegSetValueExW(
        registryKey,
        TileWallpaperValueName,
        0,
        REG_SZ,
        reinterpret_cast<const BYTE *>(TileWallpaperDisabled),
        registryStringSize(TileWallpaperDisabled));
    const LONG closeStatus = RegCloseKey(registryKey);

    if (styleStatus != ERROR_SUCCESS) {
        return {WallpaperApplyError::WallpaperStyleWriteFailed,
                static_cast<quint32>(styleStatus)};
    }
    if (tileStatus != ERROR_SUCCESS) {
        return {WallpaperApplyError::TileWallpaperWriteFailed,
                static_cast<quint32>(tileStatus)};
    }
    if (closeStatus != ERROR_SUCCESS) {
        return {WallpaperApplyError::RegistryCloseFailed,
                static_cast<quint32>(closeStatus)};
    }

    std::wstring nativeWallpaperPath = wallpaperPath.toStdWString();
    SetLastError(ERROR_SUCCESS);
    const BOOL applyStatus = SystemParametersInfoW(
        SPI_SETDESKWALLPAPER,
        0,
        nativeWallpaperPath.data(),
        SPIF_UPDATEINIFILE | SPIF_SENDWININICHANGE);
    if (applyStatus == FALSE) {
        return {WallpaperApplyError::SystemParametersInfoFailed,
                static_cast<quint32>(GetLastError())};
    }

    return {};
}

QString configuredDesktopWallpaperPath() {
    std::array<wchar_t, MAX_PATH> wallpaperPath{};
    SetLastError(ERROR_SUCCESS);
    const BOOL queryStatus = SystemParametersInfoW(
        SPI_GETDESKWALLPAPER,
        static_cast<UINT>(wallpaperPath.size()),
        wallpaperPath.data(),
        0);
    if (queryStatus == FALSE) {
        qWarning() << "Failed to query the configured wallpaper path. Windows error:"
                   << GetLastError();
        return {};
    }

    return QString::fromWCharArray(wallpaperPath.data());
}

bool isManagedWallpaperPath(const QString &path,
                            const QString &wallpaperDirectoryPath) {
    if (path.isEmpty()) {
        return false;
    }

    const QFileInfo fileInfo(path);
    if (!fileInfo.isAbsolute()) {
        return false;
    }

    const bool isInWallpaperDirectory =
        QDir::cleanPath(fileInfo.absolutePath()).compare(
            QDir::cleanPath(wallpaperDirectoryPath),
            Qt::CaseInsensitive) == 0;
    const QString fileName = fileInfo.fileName();
    return isInWallpaperDirectory
        && fileName.startsWith(WallpaperFilePrefix, Qt::CaseInsensitive)
        && fileName.endsWith(WallpaperFileSuffix, Qt::CaseInsensitive);
}

WallpaperStoragePreparation prepareWallpaperStorage() {
    const QString appDataPath =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (appDataPath.isEmpty()) {
        return {{},
                {},
                {WallpaperApplyError::StorageDirectoryCreationFailed, 0}};
    }

    QDir appDataDirectory(appDataPath);
    if (!appDataDirectory.mkpath(WallpaperDirectoryName)) {
        return {{},
                {},
                {WallpaperApplyError::StorageDirectoryCreationFailed, 0}};
    }

    const QString wallpaperDirectoryPath =
        appDataDirectory.filePath(WallpaperDirectoryName);
    const QString requestFileName =
        WallpaperFilePrefix
        + QUuid::createUuid().toString(QUuid::WithoutBraces)
        + WallpaperFileSuffix;
    const QString configuredWallpaperPath = configuredDesktopWallpaperPath();
    const QString supersededPath =
        isManagedWallpaperPath(configuredWallpaperPath, wallpaperDirectoryPath)
        ? configuredWallpaperPath
        : QString{};

    return {QDir(wallpaperDirectoryPath).filePath(requestFileName),
            supersededPath,
            {}};
}
}

struct WallpaperController::WallpaperRequestState {
    QString wallpaperPath;
    QString supersededWallpaperPath;
    std::atomic<bool> cancelRequested{false};
    std::atomic<bool> committed{false};
};

WallpaperController::WallpaperController(QObject *parent)
    : QObject(parent) {
    qRegisterMetaType<WallpaperApplyResult>();
}

WallpaperController::~WallpaperController() {
    stopActiveTask(false);
}

void WallpaperController::cancelActiveTask() {
    stopActiveTask(true);
}

void WallpaperController::stopActiveTask(bool reportCleanupFailure) {
    const std::shared_ptr<WallpaperRequestState> request = m_activeRequest;
    if (request) {
        request->cancelRequested.store(true, std::memory_order_relaxed);
    }
    if (m_workerThread) {
        m_workerThread->requestInterruption();
        m_workerThread->quit();
        m_workerThread->wait();
        m_workerThread.reset();
    }
    if (request) {
        finalizeRequest(request, reportCleanupFailure);
    }
}

bool WallpaperController::finalizeRequest(
    const std::shared_ptr<WallpaperRequestState> &request,
    bool reportCleanupFailure) {
    if (!request || m_activeRequest != request) {
        return false;
    }

    const bool committed =
        request->committed.load(std::memory_order_acquire);
    const QString cleanupPath = committed
        ? request->supersededWallpaperPath
        : request->wallpaperPath;
    cleanupFile(cleanupPath, reportCleanupFailure);
    m_activeRequest.reset();
    return true;
}

bool WallpaperController::cleanupFile(const QString &path,
                                      bool reportCleanupFailure) {
    if (path.isEmpty() || !QFile::exists(path)) {
        return true;
    }
    if (QFile::remove(path)) {
        return true;
    }

    qWarning() << "Failed to clean up wallpaper file:" << path;
    if (reportCleanupFailure) {
        emit wallpaperFileCleanupFailed(path);
    }
    return false;
}

void WallpaperController::setWallpaper(std::shared_ptr<const QImage> sourceImage, MW *mw) {
    if (!sourceImage || sourceImage->isNull() || !mw) {
        if (mw) {
            mw->showMessage(tr("Set wallpaper: failed to get image"));
        }
        return;
    }

    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) {
        mw->showMessage(tr("Set wallpaper: screen not found"));
        return;
    }

    cancelActiveTask();

    const WallpaperStoragePreparation storage = prepareWallpaperStorage();
    if (!storage.succeeded()) {
        emit wallpaperApplyFinished(storage.result);
        return;
    }

    auto request = std::make_shared<WallpaperRequestState>();
    request->wallpaperPath = storage.requestPath;
    request->supersededWallpaperPath = storage.supersededPath;
    m_activeRequest = request;

    const QSize monitorSize = screen->size();
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString modelName = settings->upscaylModel();

    mw->showMessage(tr("Setting wallpaper..."),
                    WallpaperProgressMessageDurationMs);

    QPointer<MW> mwPointer = mw;

    m_workerThread.reset(QThread::create([this,
                                          sourceImage,
                                          monitorSize,
                                          mwPointer,
                                          appDir,
                                          modelName,
                                          request]() {
        const auto finishRequest =
            [this, request](
                const std::optional<WallpaperApplyResult> &applyResult) {
                const bool queued = QMetaObject::invokeMethod(
                    this,
                    [this, request, applyResult]() {
                        if (!finalizeRequest(request, true)) {
                            return;
                        }
                        if (applyResult.has_value()) {
                            emit wallpaperApplyFinished(*applyResult);
                        }
                    },
                    Qt::QueuedConnection);
                if (!queued) {
                    qWarning() << "Failed to queue wallpaper request completion";
                }
            };

        if (request->cancelRequested.load(std::memory_order_relaxed)) {
            finishRequest(std::nullopt);
            return;
        }

        int monitorWidth = monitorSize.width();
        int monitorHeight = monitorSize.height();

        if (monitorWidth <= 0 || monitorHeight <= 0) {
            QMetaObject::invokeMethod(mwPointer, [mwPointer]() {
                if (mwPointer) {
                    mwPointer->showMessage(tr("Set wallpaper: invalid monitor size"));
                }
            }, Qt::QueuedConnection);
            finishRequest(std::nullopt);
            return;
        }

        int imageWidth = sourceImage->width();
        int imageHeight = sourceImage->height();

        double mAR = (double)monitorWidth / monitorHeight;
        double iAR = (double)imageWidth / imageHeight;

        QImage croppedImage;
        if (std::abs(iAR - mAR) < AspectRatioComparisonTolerance) {
            croppedImage = *sourceImage;
        } else {
            int cropW = imageWidth;
            int cropH = imageHeight;

            if (iAR > mAR) {
                cropW = qRound(imageHeight * mAR);
            } else {
                cropH = qRound(imageWidth / mAR);
            }

            int cropX = (imageWidth - cropW) / 2;
            int cropY = (imageHeight - cropH) / 2;

            cropX = std::clamp(cropX, 0, imageWidth - 1);
            cropY = std::clamp(cropY, 0, imageHeight - 1);
            cropW = std::clamp(cropW, 1, imageWidth - cropX);
            cropH = std::clamp(cropH, 1, imageHeight - cropY);

            croppedImage = sourceImage->copy(cropX, cropY, cropW, cropH);
        }

        if (request->cancelRequested.load(std::memory_order_relaxed)) {
            finishRequest(std::nullopt);
            return;
        }

        if (croppedImage.isNull()) {
            QMetaObject::invokeMethod(mwPointer, [mwPointer]() {
                if (mwPointer) {
                    mwPointer->showMessage(tr("Set wallpaper: cropping failed"));
                }
            }, Qt::QueuedConnection);
            finishRequest(std::nullopt);
            return;
        }

        QImage scaledImg;
        bool upscalingNeeded = (croppedImage.width() < monitorWidth || croppedImage.height() < monitorHeight);
        bool aiUpscaleSuccess = false;

        if (upscalingNeeded) {
            if (request->cancelRequested.load(std::memory_order_relaxed)) {
                finishRequest(std::nullopt);
                return;
            }

            QMetaObject::invokeMethod(mwPointer, [mwPointer]() {
                if (mwPointer) {
                    mwPointer->showMessageAiUpscale(
                        tr("AI upscaling..."),
                        AiUpscaleProgressMessageDurationMs);
                }
            }, Qt::QueuedConnection);

            UpscaylScaler *upscaler = UpscaylScaler::getInstance();
            if (upscaler) {
                const UpscaylInferenceResult inferenceResult =
                    upscaler->upscale(
                        UpscaylInferenceRequest{appDir, modelName, croppedImage,
                                                &request->cancelRequested});
                QImage upscaled = inferenceResult.image;
                if (request->cancelRequested.load(std::memory_order_relaxed)) {
                    finishRequest(std::nullopt);
                    return;
                }

                if (!upscaled.isNull()) {
                    if (upscaled.size() == monitorSize) {
                        scaledImg = upscaled;
                    } else {
                        auto upscaledShared = std::make_shared<const QImage>(upscaled);
                        scaledImg = ImageLib::scaled_MKS2021(upscaledShared, monitorSize);
                    }
                    if (!scaledImg.isNull()) {
                        aiUpscaleSuccess = true;
                    }
                }
            }
        }

        if (request->cancelRequested.load(std::memory_order_relaxed)) {
            finishRequest(std::nullopt);
            return;
        }

        if (!aiUpscaleSuccess) {
            if (croppedImage.size() == monitorSize) {
                scaledImg = croppedImage;
            } else {
                auto croppedShared = std::make_shared<const QImage>(croppedImage);
                scaledImg = ImageLib::scaled_MKS2021(croppedShared, monitorSize);
            }
        }

        if (request->cancelRequested.load(std::memory_order_relaxed)) {
            finishRequest(std::nullopt);
            return;
        }

        if (scaledImg.isNull()) {
            QMetaObject::invokeMethod(mwPointer, [mwPointer]() {
                if (mwPointer) {
                    mwPointer->showMessage(tr("Set wallpaper: scaling failed"));
                }
            }, Qt::QueuedConnection);
            finishRequest(std::nullopt);
            return;
        }

        if (!scaledImg.save(request->wallpaperPath, WallpaperImageFormat)) {
            QMetaObject::invokeMethod(mwPointer, [mwPointer]() {
                if (mwPointer) {
                    mwPointer->showMessage(tr("Set wallpaper: failed to save PNG"));
                }
            }, Qt::QueuedConnection);
            finishRequest(std::nullopt);
            return;
        }

        if (request->cancelRequested.load(std::memory_order_relaxed)) {
            finishRequest(std::nullopt);
            return;
        }

        const WallpaperApplyResult applyResult =
            applyDesktopWallpaper(request->wallpaperPath);
        if (applyResult.succeeded()) {
            request->committed.store(true, std::memory_order_release);
        }

        if (request->cancelRequested.load(std::memory_order_relaxed)) {
            finishRequest(std::nullopt);
            return;
        }

        finishRequest(applyResult);
    }));

    m_workerThread->start();
}
