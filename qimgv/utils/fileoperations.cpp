#include "fileoperations.h"

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
        return QObject::tr("Other error.");
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
        if(!destFile.isWritable()) {
            result = FileOpResult::DESTINATION_NOT_WRITABLE;
            return;
        }
        if(destFile.isDir()) {
            result = FileOpResult::DESTINATION_DIR_EXISTS;
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
        if(!destFile.isWritable()) {
            result = FileOpResult::DESTINATION_NOT_WRITABLE;
            return;
        }
        if(destFile.isDir()) {
            result = FileOpResult::DESTINATION_DIR_EXISTS;
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
    if(!srcFile.isWritable()) {
        result = FileOpResult::SOURCE_NOT_WRITABLE;
        return;
    }
    if(newName.isEmpty() || newName == srcFile.fileName()) {
        result = FileOpResult::NOTHING_TO_DO;
        return;
    }
    QString newFilePath = srcFile.absolutePath() + "/" + newName;
    QFileInfo destFile(newFilePath);
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
    if(QFile::rename(srcFile.filePath(), newFilePath)) {
        result = FileOpResult::SUCCESS;
        if(QFile::exists(tmpPath))
            QFile::remove(tmpPath);
    } else {
        result = FileOpResult::OTHER_ERROR;
        // restore dest file
        QFile::rename(tmpPath, newFilePath);
    }
}

void FileOperations::moveToTrash(const QString &filePath, FileOpResult &result) {
    QFileInfo file(filePath);
    if(!file.exists()) {
        result = FileOpResult::SOURCE_DOES_NOT_EXIST;
    } else if(!file.isWritable()) {
        result = FileOpResult::SOURCE_NOT_WRITABLE;
    } else {
        if(moveToTrashImpl(filePath))
            result = FileOpResult::SUCCESS;
        else
            result = FileOpResult::OTHER_ERROR;
    }
    return;
}

#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
bool FileOperations::moveToTrashImpl(const QString &filePath) {
    return QFile::moveToTrash(filePath);
}
#else
bool FileOperations::moveToTrashImpl(const QString &file) {
    QFileInfo fileinfo( file );
    if( !fileinfo.exists() )
        return false;
    WCHAR* from = (WCHAR*) calloc((size_t)fileinfo.absoluteFilePath().length() + 2, sizeof(WCHAR));
    fileinfo.absoluteFilePath().toWCharArray(from);    
    SHFILEOPSTRUCTW fileop;
    memset( &fileop, 0, sizeof( fileop ) );
    fileop.wFunc = FO_DELETE;
    fileop.pFrom = from;
    fileop.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;
    int rv = SHFileOperationW( &fileop );
    free(from);
    if( 0 != rv ){
        qWarning() << rv << QString::number( rv ).toInt( nullptr, 8 );
        qWarning() << "move to trash failed";
        return false;
    }
    return true;
}
#endif
