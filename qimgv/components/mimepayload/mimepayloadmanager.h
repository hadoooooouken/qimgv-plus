#pragma once

#include <QHash>
#include <QMimeData>
#include <QObject>
#include <QString>
#include <QTimer>
#include <memory>

class Image;

enum class MimePayloadTarget {
    Clipboard,
    Drop
};

enum class MimePayloadError {
    None,
    ImageUnavailable,
    SourcePathUnavailable,
    StorageRootUnavailable,
    PayloadDirectoryCreationFailed,
    PayloadFileCreationFailed,
    ImageEncodingFailed,
    PayloadCommitFailed
};

struct MimePayloadRequest {
    std::shared_ptr<Image> image;
    MimePayloadTarget target = MimePayloadTarget::Drop;
};

struct MimePayloadResult {
    MimePayloadError error = MimePayloadError::None;
    std::unique_ptr<QMimeData> mimeData;

    [[nodiscard]] bool succeeded() const noexcept {
        return error == MimePayloadError::None && mimeData != nullptr;
    }
};

class MimePayloadManager final : public QObject {
    Q_OBJECT
public:
    explicit MimePayloadManager(QString storageRootPath,
                                QObject *parent = nullptr);
    ~MimePayloadManager() override;

    [[nodiscard]] MimePayloadResult
    createPayload(const MimePayloadRequest &request);

signals:
    void payloadCleanupFailed(QString path);

private:
    [[nodiscard]] bool ensureStorageDirectory();
    void cleanupOrphanedPayloadDirectories();
    void scheduleCleanup(const QString &path);
    void retryPendingCleanup();

    QString m_storageRootPath;
    QString m_instanceDirectoryPath;
    QTimer m_cleanupRetryTimer;
    QHash<QString, int> m_cleanupAttempts;
};
