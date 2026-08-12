#pragma once

#include <QCryptographicHash>
#include <QDebug>
#include <QString>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QStandardPaths>
#include <QtGlobal>

enum FileOpResult {
    SUCCESS,
    DESTINATION_FILE_EXISTS,
    DESTINATION_DIR_EXISTS,
    SOURCE_NOT_WRITABLE,
    DESTINATION_NOT_WRITABLE,
    SOURCE_DOES_NOT_EXIST,
    DESTINATION_DOES_NOT_EXIST,
    DIRECTORY_NOT_EMPTY,
    NOTHING_TO_DO,
    OTHER_ERROR
};

enum class ImageSaveError {
    None,
    InvalidSourceImage,
    InvalidDestinationPath,
    SourceUnavailable,
    TemporaryFileCreationFailed,
    ImageEncodingFailed,
    TemporaryFileFlushFailed,
    FileCopyFailed,
    CommitFailed,
    RecoveryFailed
};

enum class ImageSaveCleanupError {
    None,
    TemporaryFileRemovalFailed
};

struct ImageSaveResult {
    ImageSaveError error = ImageSaveError::None;
    QString retainedBackupPath;
    quint32 nativeError = 0;
    QString retainedTemporaryPath;
    ImageSaveCleanupError cleanupError = ImageSaveCleanupError::None;

    [[nodiscard]] bool succeeded() const noexcept {
        return error == ImageSaveError::None;
    }
};

class QImage;

class FileOperations {
public:
    static void copyFileTo(const QString &srcFilePath, const QString &destDirPath, bool force, FileOpResult &result);
    static void moveFileTo(const QString &srcFilePath, const QString &destDirPath, bool force, FileOpResult &result);
    static void rename(const QString &srcFilePath, const QString &newName, bool force, FileOpResult &result);
    static void removeFile(const QString &filePath, FileOpResult &result);
    static void removeDir(const QString &dirPath, bool recursive, FileOpResult &result);
    static void moveToTrash(const QString &filePath, FileOpResult &result);

    static QString decodeResult(const FileOpResult &result);
    static QString generateHash(const QString &str);
    // True if fileName is one of FileOperations' own staging/backup artifacts
    // (see saveImage()/copyFileAtomically()). Filesystem-watcher consumers
    // should ignore these, including artifacts retained for recovery.
    [[nodiscard]] static bool isInternalArtifact(const QString &fileName);
    [[nodiscard]] static ImageSaveResult copyFileAtomically(
        const QString &sourcePath, const QString &destPath);
    [[nodiscard]] static ImageSaveResult saveImage(const QImage &image,
                                                   const QString &destPath,
                                                   int quality);

private:
    static bool moveToTrashImpl(const QString &path);
};
