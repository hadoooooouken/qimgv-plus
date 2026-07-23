#include "fileoperations.h"
#include <QImage>
#include <windows.h>

static QString g_lastErrorMsg;

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

bool FileOperations::saveImage(const QImage &image, const QString &destPath, int quality) {
    if (image.isNull()) {
        qWarning() << "FileOperations::saveImage() - Source image is null.";
        return false;
    }
    if (destPath.isEmpty()) {
        qWarning() << "FileOperations::saveImage() - Destination path is empty.";
        return false;
    }

    QFileInfo fi(destPath);
    QString ext = fi.suffix();
    QString hashStr = generateHash(destPath);
    QString tmpWritePath = destPath + ".qimgv_tmp_" + hashStr;
    QString backupPath = destPath + ".qimgv_bak_" + hashStr;

    // Clean up any stale temporary file from a previous interrupted save
    if (QFile::exists(tmpWritePath)) {
        QFile::remove(tmpWritePath);
    }

    // Save image to temporary path first so original destination is never corrupted during encoding
    bool saved = image.save(tmpWritePath, ext.toStdString().c_str(), quality);
    if (!saved) {
        qWarning() << "FileOperations::saveImage() - Failed to save image to temporary path:" << tmpWritePath;
        QFile::remove(tmpWritePath);
        return false;
    }

    bool originalExists = QFile::exists(destPath);

    // Create backup of original destination file if it exists
    if (originalExists) {
        if (QFile::exists(backupPath)) {
            QFile::remove(backupPath);
        }
        if (!QFile::copy(destPath, backupPath)) {
            qWarning() << "FileOperations::saveImage() - Could not create backup of destination file:" << backupPath;
            QFile::remove(tmpWritePath);
            return false;
        }
    }

    // Replace original destination file with temporary save file
    if (originalExists) {
        QFile::remove(destPath);
    }

    bool committed = QFile::rename(tmpWritePath, destPath);
    if (committed) {
        if (originalExists) {
            QFile::remove(backupPath);
        }
        return true;
    }

    // Failed to commit: clean up temp file and attempt rollback from backup
    qWarning() << "FileOperations::saveImage() - Failed to rename temporary file to destination path:" << destPath;
    QFile::remove(tmpWritePath);

    if (originalExists) {
        bool restored = QFile::copy(backupPath, destPath);
        if (restored) {
            QFile::remove(backupPath);
        } else {
            qCritical() << "FileOperations::saveImage() - CRITICAL: Rollback failed! Original backup retained at:" << backupPath;
        }
    }

    return false;
}
