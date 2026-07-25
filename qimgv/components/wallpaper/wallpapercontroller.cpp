#include "components/wallpaper/wallpapercontroller.h"
#include "gui/mainwindow.h"
#include "settings.h"
#include "components/upscaler/upscaler.h"
#include "utils/imagelib.h"

#include <QGuiApplication>
#include <QScreen>
#include <QCoreApplication>
#include <QFile>
#include <QDateTime>
#include <windows.h>
#include <cmath>
#include <algorithm>
#include <cstddef>
#include <string>

namespace {
constexpr wchar_t DesktopRegistryPath[] = L"Control Panel\\Desktop";
constexpr wchar_t WallpaperStyleValueName[] = L"WallpaperStyle";
constexpr wchar_t CenteredWallpaperStyle[] = L"0";
constexpr wchar_t TileWallpaperValueName[] = L"TileWallpaper";
constexpr wchar_t TileWallpaperDisabled[] = L"0";

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
}

WallpaperController::WallpaperController(QObject *parent)
    : QObject(parent) {
    qRegisterMetaType<WallpaperApplyResult>();
}

WallpaperController::~WallpaperController() {
    cancelActiveTask();
}

void WallpaperController::cancelActiveTask() {
    if (m_cancelToken) {
        m_cancelToken->store(true);
    }
    if (m_workerThread) {
        m_workerThread->requestInterruption();
        m_workerThread->quit();
        m_workerThread->wait();
        delete m_workerThread;
        m_workerThread = nullptr;
    }
    cleanupFile(m_currentWallpaperPath);
    m_currentWallpaperPath.clear();
}

void WallpaperController::cleanupFile(const QString &path) {
    if (!path.isEmpty() && QFile::exists(path)) {
        QFile::remove(path);
    }
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

    // Cancel any previous active task safely before starting a new one
    cancelActiveTask();

    QSize monitorSize = screen->size();
    static std::atomic<uint64_t> taskIdCounter{0};
    uint64_t taskId = ++taskIdCounter;

    QString wallpaperPath = settings->tmpDir() + QString("qimgv_wallpaper_%1_%2.png")
                                                    .arg(QCoreApplication::applicationPid())
                                                    .arg(taskId);

    m_currentWallpaperPath = wallpaperPath;
    m_cancelToken = std::make_shared<std::atomic<bool>>(false);

    QString appDir = QCoreApplication::applicationDirPath();
    QString modelName = settings->upscaylModel();

    mw->showMessage(tr("Setting wallpaper..."), 10000);

    QPointer<MW> mwPointer = mw;
    auto cancelToken = m_cancelToken;

    m_workerThread = QThread::create([this, sourceImage, monitorSize, wallpaperPath, mwPointer, appDir, modelName, cancelToken]() {
        if (cancelToken->load()) return;

        int monitorWidth = monitorSize.width();
        int monitorHeight = monitorSize.height();

        if (monitorWidth <= 0 || monitorHeight <= 0) {
            QMetaObject::invokeMethod(mwPointer, [mwPointer]() {
                if (mwPointer) {
                    mwPointer->showMessage(tr("Set wallpaper: invalid monitor size"));
                }
            }, Qt::QueuedConnection);
            return;
        }

        int imageWidth = sourceImage->width();
        int imageHeight = sourceImage->height();

        double mAR = (double)monitorWidth / monitorHeight;
        double iAR = (double)imageWidth / imageHeight;

        QImage croppedImage;
        if (std::abs(iAR - mAR) < 0.001) {
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

        if (cancelToken->load()) return;

        if (croppedImage.isNull()) {
            QMetaObject::invokeMethod(mwPointer, [mwPointer]() {
                if (mwPointer) {
                    mwPointer->showMessage(tr("Set wallpaper: cropping failed"));
                }
            }, Qt::QueuedConnection);
            return;
        }

        QImage scaledImg;
        bool upscalingNeeded = (croppedImage.width() < monitorWidth || croppedImage.height() < monitorHeight);
        bool aiUpscaleSuccess = false;

        if (upscalingNeeded) {
            if (cancelToken->load()) return;

            QMetaObject::invokeMethod(mwPointer, [mwPointer]() {
                if (mwPointer) {
                    mwPointer->showMessageAiUpscale(tr("AI upscaling..."), 60000);
                }
            }, Qt::QueuedConnection);

            if (UpscaylScaler::getInstance()->init(appDir, modelName)) {
                QImage upscaled = UpscaylScaler::getInstance()->upscale(croppedImage, cancelToken.get());
                if (cancelToken->load()) return;

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

        if (cancelToken->load()) return;

        if (!aiUpscaleSuccess) {
            if (croppedImage.size() == monitorSize) {
                scaledImg = croppedImage;
            } else {
                auto croppedShared = std::make_shared<const QImage>(croppedImage);
                scaledImg = ImageLib::scaled_MKS2021(croppedShared, monitorSize);
            }
        }

        if (cancelToken->load()) return;

        if (scaledImg.isNull()) {
            QMetaObject::invokeMethod(mwPointer, [mwPointer]() {
                if (mwPointer) {
                    mwPointer->showMessage(tr("Set wallpaper: scaling failed"));
                }
            }, Qt::QueuedConnection);
            return;
        }

        if (!scaledImg.save(wallpaperPath, "PNG")) {
            QMetaObject::invokeMethod(mwPointer, [mwPointer]() {
                if (mwPointer) {
                    mwPointer->showMessage(tr("Set wallpaper: failed to save PNG"));
                }
            }, Qt::QueuedConnection);
            return;
        }

        if (cancelToken->load()) return;

        const WallpaperApplyResult applyResult = applyDesktopWallpaper(wallpaperPath);

        if (cancelToken->load()) return;

        emit wallpaperApplyFinished(applyResult);
    });

    m_workerThread->start();
}
