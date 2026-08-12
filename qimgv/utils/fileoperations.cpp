#include "fileoperations.h"
#include <QFile>
#include <QImage>
#include <QTemporaryFile>
#include <QUuid>
#include <string>
#include <windows.h>

static QString g_lastErrorMsg;

namespace {

constexpr DWORD kReplaceFileFlags = 0;
constexpr DWORD kMoveFileFlags = MOVEFILE_WRITE_THROUGH;
constexpr qsizetype kFileCopyBufferSizeBytes = 1024 * 1024;

// Shared with FileOperations::isInternalArtifact() below. These markers
// identify staging and backup artifacts of the atomic save mechanism,
// including files retained for recovery. Nothing outside this file should
// hardcode them.
const QString kTemporaryFileMarker = QStringLiteral(".qimgv_tmp_");
const QString kBackupFileMarker = QStringLiteral(".qimgv_bak_");

std::wstring nativePath(const QString &path) {
    return QDir::toNativeSeparators(path).toStdWString();
}

ImageSaveResult commitTemporaryFile(const QString &temporaryPath,
                                    const QString &destinationPath) {
    const std::wstring nativeTemporaryPath = nativePath(temporaryPath);
    const std::wstring nativeDestinationPath = nativePath(destinationPath);

    if (!QFile::exists(destinationPath)) {
        if (MoveFileExW(nativeTemporaryPath.c_str(), nativeDestinationPath.c_str(),
                        kMoveFileFlags)) {
            return {};
        }

        const DWORD moveError = GetLastError();
        qWarning() << "FileOperations - MoveFileExW failed committing"
                   << temporaryPath << "to" << destinationPath
                   << "Windows error:" << moveError;
        return {ImageSaveError::CommitFailed, {}, moveError};
    }

    const QString backupPath =
        destinationPath + kBackupFileMarker +
        QUuid::createUuid().toString(QUuid::WithoutBraces);
    const std::wstring nativeBackupPath = nativePath(backupPath);

    if (ReplaceFileW(nativeDestinationPath.c_str(), nativeTemporaryPath.c_str(),
                     nativeBackupPath.c_str(), kReplaceFileFlags, nullptr, nullptr)) {
        if (QFile::exists(backupPath) && !QFile::remove(backupPath)) {
            qWarning() << "FileOperations - Saved file, but could not remove backup:"
                       << backupPath;
            return {ImageSaveError::None, backupPath, 0};
        }
        return {};
    }

    const DWORD commitError = GetLastError();
    if (commitError == ERROR_FILE_NOT_FOUND &&
        !QFile::exists(destinationPath) &&
        !QFile::exists(backupPath)) {
        if (MoveFileExW(nativeTemporaryPath.c_str(), nativeDestinationPath.c_str(),
                        kMoveFileFlags)) {
            return {};
        }
        const DWORD fallbackMoveError = GetLastError();
        qWarning() << "FileOperations - Fallback MoveFileExW failed committing"
                   << temporaryPath << "to" << destinationPath
                   << "Windows error:" << fallbackMoveError;
        return {ImageSaveError::CommitFailed, {}, fallbackMoveError};
    }

    if (!QFile::exists(backupPath)) {
        qWarning() << "FileOperations - ReplaceFileW failed before creating a backup for"
                   << destinationPath << "Windows error:" << commitError;
        return {ImageSaveError::CommitFailed, {}, commitError};
    }

    if (QFile::exists(destinationPath)) {
        qCritical() << "FileOperations - Replacement failed; recovery backup retained at:"
                    << backupPath;
        return {ImageSaveError::CommitFailed, backupPath, commitError};
    }

    if (MoveFileExW(nativeBackupPath.c_str(), nativeDestinationPath.c_str(),
                    kMoveFileFlags)) {
        qWarning() << "FileOperations - ReplaceFileW failed for" << destinationPath
                   << "Windows error:" << commitError
                   << "- original file restored from backup, no data lost.";
        return {ImageSaveError::CommitFailed, {}, commitError};
    }

    const DWORD recoveryError = GetLastError();
    qCritical() << "FileOperations - Replacement recovery failed; backup retained at:"
                << backupPath << "Windows error:" << recoveryError;
    return {ImageSaveError::RecoveryFailed, backupPath, recoveryError};
}

ImageSaveResult finalizeFailedCommit(ImageSaveResult result,
                                     const QString &temporaryPath) {
    if (result.succeeded() || !QFile::exists(temporaryPath))
        return result;

    if (!result.retainedBackupPath.isEmpty()) {
        result.retainedTemporaryPath = temporaryPath;
        qCritical() << "FileOperations - Failed commit retained a staged file at:"
                    << temporaryPath;
        return result;
    }

    QFile temporaryFile(temporaryPath);
    if (temporaryFile.remove())
        return result;

    result.retainedTemporaryPath = temporaryPath;
    result.cleanupError = ImageSaveCleanupError::TemporaryFileRemovalFailed;
    qCritical() << "FileOperations - Could not remove staged file after failed commit;"
                   " recovery copy retained at:"
                << temporaryPath << temporaryFile.errorString();
    return result;
}

} // namespace

QString FileOperations::generateHash(const QString &str) {
    return QString(QCryptographicHash::hash(str.toUtf8(), QCryptographicHash::Md5).toHex());
}

void FileOperations::removeFile(const QString &filePath, FileOpResult &result) {
    QFileInfo file(filePath);
    if(!file.exists()) {
        result = FileOpResult::SOURCE_DOES_NOT_EXIST;
    } else if(!file.isWritable()) {
        result = FileOpResult::SOURCE_NOT_WRITABLE;
    } else {
        if(QFile::remove(filePath))
            result = FileOpResult::SUCCESS;
        else
            result = FileOpResult::OTHER_ERROR;
    }
    return;
}

// non-recursive
void FileOperations::removeDir(const QString &dirPath, bool recursive, FileOpResult &result) {
    QDir dir(dirPath);
    if(!dir.exists()) {
        result = FileOpResult::SOURCE_DOES_NOT_EXIST;
    } else {
        if(recursive ? dir.removeRecursively() : dir.rmdir(dirPath))
            result = FileOpResult::SUCCESS;
        else if(!recursive && !dir.isEmpty())
            result = FileOpResult::DIRECTORY_NOT_EMPTY;
        else
            result = FileOpResult::OTHER_ERROR;
    }
    return;
}

QString FileOperations::decodeResult(const FileOpResult &result) {
    switch(result) {
    case FileOpResult::SUCCESS:
        return QObject::tr("Operation completed succesfully.");
    case FileOpResult::DESTINATION_FILE_EXISTS:
        return QObject::tr("Destination file exists.");
    case FileOpResult::DESTINATION_DIR_EXISTS:
        return QObject::tr("Destination directory exists.");
    case FileOpResult::SOURCE_NOT_WRITABLE:
        return QObject::tr("Source file is not writable.");
    case FileOpResult::DESTINATION_NOT_WRITABLE:
        return QObject::tr("Destination is not writable.");
    case FileOpResult::SOURCE_DOES_NOT_EXIST:
        return QObject::tr("Source file does not exist.");
    case FileOpResult::DESTINATION_DOES_NOT_EXIST:
        return QObject::tr("Destination does not exist.");
    case FileOpResult::DIRECTORY_NOT_EMPTY:
        return QObject::tr("Directory is not empty.");
    case FileOpResult::NOTHING_TO_DO:
        return QObject::tr("Nothing to do.");
    case FileOpResult::OTHER_ERROR:
        return g_lastErrorMsg.isEmpty() ? QObject::tr("Other error.") : g_lastErrorMsg;
    }
    return nullptr;
}

void FileOperations::copyFileTo(const QString &srcFilePath, const QString &destDirPath, bool force, FileOpResult &result) {
    QFileInfo srcFile(srcFilePath);
    QString tmpPath;
    bool exists = false;
    // error checks
    if(destDirPath == srcFile.absolutePath()) {
        result = FileOpResult::NOTHING_TO_DO;
        return;
    }
    if(!srcFile.exists()) {
        result = FileOpResult::SOURCE_DOES_NOT_EXIST;
        return;
    }
    QFileInfo destDir(destDirPath);
    if(!destDir.exists()) {
        result = FileOpResult::DESTINATION_DOES_NOT_EXIST;
        return;
    }
    if(!destDir.isWritable()) {
        result = FileOpResult::DESTINATION_NOT_WRITABLE;
        return;
    }
    QFileInfo destFile(destDirPath + "/" + srcFile.fileName());
    if(destFile.exists()) {
        if(destFile.isDir()) {
            result = FileOpResult::DESTINATION_DIR_EXISTS;
            return;
        }
        if(!destFile.isWritable()) {
            result = FileOpResult::DESTINATION_NOT_WRITABLE;
            return;
        }
        if(!force) {
            result = FileOpResult::DESTINATION_FILE_EXISTS;
            return;
        }
        // remove just in case it exists
        tmpPath = destFile.absoluteFilePath() + "_" + generateHash(destFile.absoluteFilePath());
        QFile::remove(tmpPath);
        // move backup
        QFile::rename(destFile.absoluteFilePath(), tmpPath);
        exists = true;
    }
    // copy
    auto srcModTime = srcFile.lastModified();
    auto srcReadTime = srcFile.lastRead();
    if(QFile::copy(srcFile.absoluteFilePath(), destFile.absoluteFilePath())) {
        result = FileOpResult::SUCCESS;
        // restore timestamps
        QFile dstF(destFile.absoluteFilePath());
        (void)dstF.open(QIODevice::ReadWrite);
        dstF.setFileTime(srcModTime, QFileDevice::FileModificationTime);
        dstF.setFileTime(srcReadTime, QFileDevice::FileAccessTime);
        dstF.close();
        // ok; remove the backup
        if(exists)
            QFile::remove(tmpPath);
    } else {
        result = FileOpResult::OTHER_ERROR;
        // fail; revert
        QFile::rename(tmpPath, destFile.absoluteFilePath());
    }
    return;
}

void FileOperations::moveFileTo(const QString &srcFilePath, const QString &destDirPath, bool force, FileOpResult &result) {
    QFileInfo srcFile(srcFilePath);
    QString tmpPath;
    bool exists = false;
    // error checks
    if(destDirPath == srcFile.absolutePath()) {
        result = FileOpResult::NOTHING_TO_DO;
        return;
    }
    if(!srcFile.exists()) {
        result = FileOpResult::SOURCE_DOES_NOT_EXIST;
        return;
    }
    if(!srcFile.isWritable()) {
        result = FileOpResult::SOURCE_NOT_WRITABLE;
        return;
    }
    QFileInfo destDir(destDirPath);
    if(!destDir.exists()) {
        result = FileOpResult::DESTINATION_DOES_NOT_EXIST;
        return;
    }
    if(!destDir.isWritable()) {
        result = FileOpResult::DESTINATION_NOT_WRITABLE;
        return;
    }
    QFileInfo destFile(destDirPath + "/" + srcFile.fileName());
    if(destFile.exists()) {
        if(destFile.isDir()) {
            result = FileOpResult::DESTINATION_DIR_EXISTS;
            return;
        }
        if(!destFile.isWritable()) {
            result = FileOpResult::DESTINATION_NOT_WRITABLE;
            return;
        }
        if(!force) {
            result = FileOpResult::DESTINATION_FILE_EXISTS;
            return;
        }
        tmpPath = destFile.absoluteFilePath() + "_" + generateHash(destFile.absoluteFilePath());
        QFile::remove(tmpPath);
        // move backup
        QFile::rename(destFile.absoluteFilePath(), tmpPath);
        exists = true;
    }
    // move
    auto srcModTime = srcFile.lastModified();
    auto srcReadTime = srcFile.lastRead();
    if(QFile::copy(srcFile.absoluteFilePath(), destFile.absoluteFilePath())) {
        // remove original file
        FileOpResult removeResult;
        removeFile(srcFile.absoluteFilePath(), removeResult);
        if(removeResult == FileOpResult::SUCCESS) {
            // OK
            result = FileOpResult::SUCCESS;
            // restore timestamps
            QFile dstF(destFile.absoluteFilePath());
            (void)dstF.open(QIODevice::ReadWrite);
            dstF.setFileTime(srcModTime, QFileDevice::FileModificationTime);
            dstF.setFileTime(srcReadTime, QFileDevice::FileAccessTime);
            dstF.close();
            // remove backup
            if(exists)
                QFile::remove(tmpPath);
            return;
        }
        // revert on failure
        result = FileOpResult::SOURCE_NOT_WRITABLE;
        if(QFile::remove(destFile.absoluteFilePath()))
            result = FileOpResult::OTHER_ERROR;
    } else {
        // could not COPY
        result = FileOpResult::OTHER_ERROR;
    }
    if(exists) // failed; revert backup
        QFile::rename(tmpPath, destFile.absoluteFilePath());
    return;
}

void FileOperations::rename(const QString &srcFilePath, const QString &newName, bool force, FileOpResult &result) {
    QFileInfo srcFile(srcFilePath);

    QString tmpPath;
    // error checks
    if(!srcFile.exists()) {
        result = FileOpResult::SOURCE_DOES_NOT_EXIST;
        return;
    }
    if(!srcFile.isDir() && !srcFile.isWritable()) {
        result = FileOpResult::SOURCE_NOT_WRITABLE;
        return;
    }
    if(newName.isEmpty() || newName == srcFile.fileName()) {
        result = FileOpResult::NOTHING_TO_DO;
        return;
    }
    if (newName.contains('/') || newName.contains('\\') || newName == "." || newName == "..") {
        result = FileOpResult::OTHER_ERROR;
        return;
    }
    QString newFilePath = srcFile.absolutePath() + "/" + newName;
    QFileInfo destFile(newFilePath);
    if (destFile.absolutePath() != srcFile.absolutePath()) {
        result = FileOpResult::OTHER_ERROR;
        return;
    }
    if(destFile.exists()) {
        if(!destFile.isWritable())
            result = FileOpResult::DESTINATION_NOT_WRITABLE;
        if(destFile.isDir()) {
            result = FileOpResult::DESTINATION_DIR_EXISTS;
            return;
        }
        if(!force) {
            result = FileOpResult::DESTINATION_FILE_EXISTS;
            return;
        }
        tmpPath = newFilePath + "_" + generateHash(newFilePath);
        QFile::remove(tmpPath);
        // move dest file
        QFile::rename(newFilePath, tmpPath);
    }
    bool renameSuccess = false;
    if(srcFile.isDir()) {
        renameSuccess = QDir().rename(srcFile.filePath(), newFilePath);
    } else {
        renameSuccess = QFile::rename(srcFile.filePath(), newFilePath);
    }

    if(renameSuccess) {
        g_lastErrorMsg.clear();
        result = FileOpResult::SUCCESS;
        if(QFile::exists(tmpPath))
            QFile::remove(tmpPath);
    } else {
        DWORD err = GetLastError();
        wchar_t buf[512];
        FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                       NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                       buf, (sizeof(buf) / sizeof(wchar_t)), NULL);
        g_lastErrorMsg = QObject::tr("Rename failed: %1 (Error %2)").arg(QString::fromWCharArray(buf).trimmed()).arg(err);
        result = FileOpResult::OTHER_ERROR;
        // restore dest file
        QFile::rename(tmpPath, newFilePath);
    }
}

void FileOperations::moveToTrash(const QString &filePath, FileOpResult &result) {
    QFileInfo file(filePath);
    if(!file.exists()) {
        result = FileOpResult::SOURCE_DOES_NOT_EXIST;
    } else if(!file.isDir() && !file.isWritable()) {
        result = FileOpResult::SOURCE_NOT_WRITABLE;
    } else {
        if(moveToTrashImpl(filePath))
            result = FileOpResult::SUCCESS;
        else
            result = FileOpResult::OTHER_ERROR;
    }
    return;
}

bool FileOperations::moveToTrashImpl(const QString &filePath) {
    return QFile::moveToTrash(filePath);
}

ImageSaveResult FileOperations::copyFileAtomically(const QString &sourcePath,
                                                   const QString &destPath) {
    if (destPath.isEmpty()) {
        qWarning() << "FileOperations::copyFileAtomically() - Destination path is empty.";
        return {ImageSaveError::InvalidDestinationPath};
    }

    QFile sourceFile(sourcePath);
    if (!sourceFile.open(QIODevice::ReadOnly)) {
        qWarning() << "FileOperations::copyFileAtomically() - Could not open source file:"
                   << sourcePath << sourceFile.errorString();
        return {ImageSaveError::SourceUnavailable};
    }

    const QString temporaryFileSuffix = kTemporaryFileMarker + QStringLiteral("XXXXXX");
    QString temporaryPath;
    {
        // Scoped so the QTemporaryFile object - and the native Windows handle
        // it keeps open internally even after close() - is fully released
        // before commitTemporaryFile() below tries to move/replace the
        // destination. Committing while that handle is still alive causes
        // MoveFileExW/ReplaceFileW to fail with ERROR_SHARING_VIOLATION
        // (Windows error 32), since the file has "just been created and
        // closed" from the OS's point of view.
        QTemporaryFile temporaryFile(destPath + temporaryFileSuffix);
        if (!temporaryFile.open()) {
            qWarning() << "FileOperations::copyFileAtomically() - Could not create temporary file:"
                       << temporaryFile.errorString();
            return {ImageSaveError::TemporaryFileCreationFailed};
        }

        QByteArray buffer;
        buffer.resize(kFileCopyBufferSizeBytes);
        while (!sourceFile.atEnd()) {
            const qint64 bytesRead = sourceFile.read(buffer.data(), buffer.size());
            if (bytesRead < 0) {
                qWarning() << "FileOperations::copyFileAtomically() - Could not read source file:"
                           << sourcePath << sourceFile.errorString();
                return {ImageSaveError::FileCopyFailed};
            }

            qint64 bytesWritten = 0;
            while (bytesWritten < bytesRead) {
                const qint64 writeResult =
                    temporaryFile.write(buffer.constData() + bytesWritten,
                                        bytesRead - bytesWritten);
                if (writeResult <= 0) {
                    qWarning() << "FileOperations::copyFileAtomically() - Could not write temporary file:"
                               << temporaryFile.fileName()
                               << temporaryFile.errorString();
                    return {ImageSaveError::FileCopyFailed};
                }
                bytesWritten += writeResult;
            }
        }

        if (!temporaryFile.flush()) {
            qWarning() << "FileOperations::copyFileAtomically() - Failed to flush temporary file:"
                       << temporaryFile.fileName() << temporaryFile.errorString();
            return {ImageSaveError::TemporaryFileFlushFailed};
        }

        temporaryPath = temporaryFile.fileName();
        // We commit this file ourselves right below; don't let the
        // QTemporaryFile destructor delete it out from under us.
        temporaryFile.setAutoRemove(false);
    } // temporaryFile is destroyed here, releasing its native handle.

    return finalizeFailedCommit(commitTemporaryFile(temporaryPath, destPath),
                                temporaryPath);
}

ImageSaveResult FileOperations::saveImage(const QImage &image,
                                          const QString &destPath,
                                          int quality) {
    if (image.isNull()) {
        qWarning() << "FileOperations::saveImage() - Source image is null.";
        return {ImageSaveError::InvalidSourceImage};
    }
    if (destPath.isEmpty()) {
        qWarning() << "FileOperations::saveImage() - Destination path is empty.";
        return {ImageSaveError::InvalidDestinationPath};
    }

    const QFileInfo destinationInfo(destPath);
    const QByteArray imageFormat = destinationInfo.suffix().toLatin1();
    const QString temporaryFileSuffix = kTemporaryFileMarker + QStringLiteral("XXXXXX");
    QString temporaryPath;
    {
        // See the comment in copyFileAtomically(): the QTemporaryFile must be
        // fully destroyed - not just close()d - before we try to move/replace
        // the destination, or the still-open native handle causes
        // ERROR_SHARING_VIOLATION on MoveFileExW/ReplaceFileW.
        QTemporaryFile temporaryFile(destPath + temporaryFileSuffix);
        if (!temporaryFile.open()) {
            qWarning() << "FileOperations::saveImage() - Could not create temporary file:"
                       << temporaryFile.errorString();
            return {ImageSaveError::TemporaryFileCreationFailed};
        }

        if (!image.save(&temporaryFile, imageFormat.constData(), quality)) {
            qWarning() << "FileOperations::saveImage() - Failed to encode image in temporary file:"
                       << temporaryFile.fileName();
            return {ImageSaveError::ImageEncodingFailed};
        }

        if (!temporaryFile.flush()) {
            qWarning() << "FileOperations::saveImage() - Failed to flush temporary file:"
                       << temporaryFile.fileName() << temporaryFile.errorString();
            return {ImageSaveError::TemporaryFileFlushFailed};
        }

        temporaryPath = temporaryFile.fileName();
        temporaryFile.setAutoRemove(false);
    } // temporaryFile is destroyed here, releasing its native handle.

    return finalizeFailedCommit(commitTemporaryFile(temporaryPath, destPath),
                                temporaryPath);
}

// Any consumer that reacts to raw filesystem events (namely DirectoryManager's
// DirectoryWatcher handlers) needs to recognize and ignore the temp/backup
// files this class creates during an atomic save - they are internal staging
// or recovery artifacts, and the caller of saveImage()/copyFileAtomically()
// already pushes the real, user-facing update (DirectoryModel::saveFile()
// calls updateFileEntry()/emits fileModified() directly once the save
// succeeds).
bool FileOperations::isInternalArtifact(const QString &fileName) {
    return fileName.contains(kTemporaryFileMarker) || fileName.contains(kBackupFileMarker);
}
