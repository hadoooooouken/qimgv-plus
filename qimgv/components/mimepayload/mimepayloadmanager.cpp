#include "components/mimepayload/mimepayloadmanager.h"

#include "sourcecontainers/image.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QMetaObject>
#include <QPointer>
#include <QRegularExpression>
#include <QSaveFile>
#include <QUuid>
#include <QUrl>
#include <functional>
#include <limits>
#include <utility>
#include <windows.h>

namespace {
constexpr int DropPngQuality = 80;
constexpr int ClipboardPngQuality = 30;
constexpr int CleanupRetryIntervalMs = 1000;
constexpr int CleanupFailureReportAttempt = 3;
constexpr int InitialCleanupAttemptCount = 0;
constexpr qint64 InvalidPayloadOwnerProcessId = 0;
constexpr DWORD ProcessStatusPollTimeoutMs = 0;
constexpr auto InstanceDirectoryPrefix = "mime_payload_";
constexpr auto LegacyDirectoryPrefix = "payload_";
constexpr auto FallbackPayloadBaseName = "image";
constexpr auto PayloadImageFormat = "PNG";
constexpr auto PayloadFileSuffix = ".png";

[[nodiscard]] bool removeDirectoryTree(const QString &path) {
    if (path.isEmpty()) {
        return false;
    }

    const QFileInfo pathInfo(path);
    if (!pathInfo.exists()) {
        return true;
    }
    if (!pathInfo.isDir()) {
        return false;
    }
    return QDir(path).removeRecursively();
}

[[nodiscard]] bool isProcessRunning(qint64 processId) {
    if (processId <= InvalidPayloadOwnerProcessId ||
        static_cast<quint64>(processId) >
            (std::numeric_limits<DWORD>::max)()) {
        return false;
    }

    const HANDLE process =
        OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(processId));
    if (!process) {
        // Access denial is not proof that the process is gone. Only the
        // invalid-parameter result reliably identifies a missing PID here.
        return GetLastError() != ERROR_INVALID_PARAMETER;
    }

    const DWORD waitResult =
        WaitForSingleObject(process, ProcessStatusPollTimeoutMs);
    const DWORD waitError =
        waitResult == WAIT_FAILED ? GetLastError() : ERROR_SUCCESS;
    if (!CloseHandle(process)) {
        qWarning() << "Failed to close MIME payload process handle:"
                   << GetLastError();
    }
    if (waitResult == WAIT_FAILED) {
        qWarning() << "Failed to query MIME payload owner process:"
                   << processId << waitError;
        return true;
    }
    return waitResult != WAIT_OBJECT_0;
}

[[nodiscard]] qint64 payloadOwnerProcessId(const QString &directoryName) {
    static const QRegularExpression InstanceDirectoryPattern(
        QStringLiteral("^%1(\\d+)_[0-9a-fA-F-]+$")
            .arg(QRegularExpression::escape(
                QString::fromLatin1(InstanceDirectoryPrefix))));
    static const QRegularExpression LegacyDirectoryPattern(
        QStringLiteral("^%1(\\d+)_[0-9a-fA-F-]+$")
            .arg(QRegularExpression::escape(
                QString::fromLatin1(LegacyDirectoryPrefix))));

    QRegularExpressionMatch match =
        InstanceDirectoryPattern.match(directoryName);
    if (!match.hasMatch()) {
        match = LegacyDirectoryPattern.match(directoryName);
    }
    if (!match.hasMatch()) {
        return InvalidPayloadOwnerProcessId;
    }

    bool converted = false;
    const qint64 processId = match.captured(1).toLongLong(&converted);
    return converted ? processId : InvalidPayloadOwnerProcessId;
}

class PayloadCleanupLease final : public QObject {
public:
    using FailureHandler = std::function<void(const QString &)>;

    PayloadCleanupLease(QString directoryPath, FailureHandler failureHandler)
        : m_directoryPath(std::move(directoryPath)),
          m_failureHandler(std::move(failureHandler)) {}

    ~PayloadCleanupLease() override {
        if (removeDirectoryTree(m_directoryPath)) {
            return;
        }

        qWarning() << "Failed to clean up MIME payload directory:"
                   << m_directoryPath;
        if (m_failureHandler) {
            m_failureHandler(m_directoryPath);
        }
    }

private:
    QString m_directoryPath;
    FailureHandler m_failureHandler;
};
} // namespace

MimePayloadManager::MimePayloadManager(QString storageRootPath, QObject *parent)
    : QObject(parent), m_storageRootPath(std::move(storageRootPath)) {
    if (!m_storageRootPath.isEmpty()) {
        m_storageRootPath = QDir::cleanPath(m_storageRootPath);
        const QString instanceDirectoryName =
            QStringLiteral("%1%2_%3")
                .arg(QLatin1StringView(InstanceDirectoryPrefix))
                .arg(QCoreApplication::applicationPid())
                .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
        m_instanceDirectoryPath =
            QDir(m_storageRootPath).filePath(instanceDirectoryName);
    }

    m_cleanupRetryTimer.setInterval(CleanupRetryIntervalMs);
    connect(&m_cleanupRetryTimer, &QTimer::timeout, this,
            &MimePayloadManager::retryPendingCleanup);
    cleanupOrphanedPayloadDirectories();
}

MimePayloadManager::~MimePayloadManager() {
    m_cleanupRetryTimer.stop();

    const QStringList pendingPaths = m_cleanupAttempts.keys();
    for (const QString &path : pendingPaths) {
        if (!removeDirectoryTree(path)) {
            qWarning() << "Failed to clean up pending MIME payload directory:"
                       << path;
        }
    }

    if (!m_instanceDirectoryPath.isEmpty() &&
        !removeDirectoryTree(m_instanceDirectoryPath)) {
        qWarning() << "Failed to clean up MIME payload instance directory:"
                   << m_instanceDirectoryPath;
    }
}

MimePayloadResult
MimePayloadManager::createPayload(const MimePayloadRequest &request) {
    if (!request.image) {
        return {MimePayloadError::ImageUnavailable, nullptr};
    }

    QString payloadPath = request.image->filePath();
    std::unique_ptr<PayloadCleanupLease> cleanupLease;
    const bool needsTemporaryPayload =
        request.image->type() == STATIC && request.image->isEdited();

    if (needsTemporaryPayload) {
        const std::shared_ptr<const QImage> sourceImage =
            request.image->getImage();
        if (!sourceImage || sourceImage->isNull()) {
            return {MimePayloadError::ImageUnavailable, nullptr};
        }
        if (!ensureStorageDirectory()) {
            return {MimePayloadError::StorageRootUnavailable, nullptr};
        }

        const QString payloadDirectoryPath =
            QDir(m_instanceDirectoryPath)
                .filePath(QUuid::createUuid().toString(
                    QUuid::WithoutBraces));
        const QPointer<MimePayloadManager> manager(this);
        cleanupLease = std::make_unique<PayloadCleanupLease>(
            payloadDirectoryPath,
            [manager](const QString &failedPath) {
                if (!manager) {
                    qWarning() << "Cannot retry MIME payload cleanup because "
                                  "the manager is unavailable:"
                               << failedPath;
                    return;
                }

                const bool queued = QMetaObject::invokeMethod(
                    manager.data(),
                    [manager, failedPath]() {
                        if (manager) {
                            manager->scheduleCleanup(failedPath);
                        } else {
                            qWarning()
                                << "Cannot retry MIME payload cleanup because "
                                   "the manager was destroyed:"
                                << failedPath;
                        }
                    },
                    Qt::QueuedConnection);
                if (!queued) {
                    qWarning() << "Failed to queue MIME payload cleanup:"
                               << failedPath;
                }
            });
        if (!QDir().mkpath(payloadDirectoryPath)) {
            return {MimePayloadError::PayloadDirectoryCreationFailed,
                    nullptr};
        }

        QString payloadBaseName = request.image->baseName();
        if (payloadBaseName.isEmpty()) {
            payloadBaseName = QLatin1StringView(FallbackPayloadBaseName);
        }
        payloadPath = QDir(payloadDirectoryPath)
                          .filePath(
                              payloadBaseName +
                              QLatin1StringView(PayloadFileSuffix));

        QSaveFile payloadFile(payloadPath);
        if (!payloadFile.open(QIODevice::WriteOnly)) {
            return {MimePayloadError::PayloadFileCreationFailed, nullptr};
        }

        const int pngQuality =
            request.target == MimePayloadTarget::Drop
                ? DropPngQuality
                : ClipboardPngQuality;
        if (!sourceImage->save(&payloadFile, PayloadImageFormat, pngQuality)) {
            payloadFile.cancelWriting();
            return {MimePayloadError::ImageEncodingFailed, nullptr};
        }
        if (!payloadFile.commit()) {
            return {MimePayloadError::PayloadCommitFailed, nullptr};
        }
    }

    if (payloadPath.isEmpty()) {
        return {MimePayloadError::SourcePathUnavailable, nullptr};
    }

    auto mimeData = std::make_unique<QMimeData>();
    mimeData->setUrls({QUrl::fromLocalFile(payloadPath)});
    if (request.target == MimePayloadTarget::Clipboard) {
        const std::shared_ptr<const QImage> clipboardImage =
            request.image->getImage();
        if (clipboardImage && !clipboardImage->isNull()) {
            mimeData->setImageData(*clipboardImage);
        }
    }

    if (cleanupLease) {
        cleanupLease->setParent(mimeData.get());
        cleanupLease.release();
    }

    return {MimePayloadError::None, std::move(mimeData)};
}

bool MimePayloadManager::ensureStorageDirectory() {
    if (m_storageRootPath.isEmpty() || m_instanceDirectoryPath.isEmpty()) {
        return false;
    }

    const QFileInfo storageRootInfo(m_storageRootPath);
    if (storageRootInfo.exists() && !storageRootInfo.isDir()) {
        return false;
    }
    if (!QDir().mkpath(m_storageRootPath)) {
        return false;
    }

    const QFileInfo instanceDirectoryInfo(m_instanceDirectoryPath);
    if (instanceDirectoryInfo.exists() && !instanceDirectoryInfo.isDir()) {
        return false;
    }
    return QDir().mkpath(m_instanceDirectoryPath);
}

void MimePayloadManager::cleanupOrphanedPayloadDirectories() {
    const QFileInfo storageRootInfo(m_storageRootPath);
    if (m_storageRootPath.isEmpty() || !storageRootInfo.isDir()) {
        return;
    }

    const QDir storageRoot(m_storageRootPath);
    const QStringList directoryNames = storageRoot.entryList(
        {QStringLiteral("%1*").arg(
             QLatin1StringView(InstanceDirectoryPrefix)),
         QStringLiteral("%1*").arg(
             QLatin1StringView(LegacyDirectoryPrefix))},
        QDir::Dirs | QDir::NoDotAndDotDot);
    const qint64 currentProcessId = QCoreApplication::applicationPid();

    for (const QString &directoryName : directoryNames) {
        const qint64 ownerProcessId =
            payloadOwnerProcessId(directoryName);
        if (ownerProcessId <= InvalidPayloadOwnerProcessId ||
            (ownerProcessId != currentProcessId &&
             isProcessRunning(ownerProcessId))) {
            continue;
        }

        const QString directoryPath = storageRoot.filePath(directoryName);
        if (!removeDirectoryTree(directoryPath)) {
            scheduleCleanup(directoryPath);
        }
    }
}

void MimePayloadManager::scheduleCleanup(const QString &path) {
    if (path.isEmpty() || m_cleanupAttempts.contains(path)) {
        return;
    }

    m_cleanupAttempts.insert(path, InitialCleanupAttemptCount);
    if (!m_cleanupRetryTimer.isActive()) {
        m_cleanupRetryTimer.start();
    }
}

void MimePayloadManager::retryPendingCleanup() {
    const QStringList pendingPaths = m_cleanupAttempts.keys();
    for (const QString &path : pendingPaths) {
        if (removeDirectoryTree(path)) {
            m_cleanupAttempts.remove(path);
            continue;
        }

        const int attempt = m_cleanupAttempts.value(path) + 1;
        m_cleanupAttempts.insert(path, attempt);
        if (attempt == CleanupFailureReportAttempt) {
            emit payloadCleanupFailed(path);
        }
    }

    if (m_cleanupAttempts.isEmpty()) {
        m_cleanupRetryTimer.stop();
    }
}
