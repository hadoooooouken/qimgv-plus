#include "fileoptaskrunnable.h"

#include <QCoreApplication>
#include <QDebug>
#include <QMetaObject>
#include <utility>

#include "components/directorymodel.h"
#include "gui/mainwindow.h"

namespace {

bool isDestinationInsideSource(const QString &srcPath, const QString &destDirectory) {
    QFileInfo srcFi(srcPath);
    if (!srcFi.isDir()) {
        return false;
    }

    QString cleanSrc = srcFi.canonicalFilePath();
    if (cleanSrc.isEmpty()) {
        cleanSrc = QDir::cleanPath(srcFi.absoluteFilePath());
    }

    QFileInfo dstFi(destDirectory);
    QString cleanDst = dstFi.canonicalFilePath();
    if (cleanDst.isEmpty()) {
        cleanDst = QDir::cleanPath(dstFi.absoluteFilePath());
    }

    cleanSrc = QDir::cleanPath(cleanSrc);
    cleanDst = QDir::cleanPath(cleanDst);

    if (cleanSrc.isEmpty() || cleanDst.isEmpty()) {
        return false;
    }

    if (QString::compare(cleanSrc, cleanDst, Qt::CaseInsensitive) == 0) {
        return true;
    }

    if (!cleanSrc.endsWith(u'/')) {
        cleanSrc.append(u'/');
    }

    return cleanDst.startsWith(cleanSrc, Qt::CaseInsensitive);
}

} // namespace

void FileOpTaskNotifier::reportProgress(FileOpProgress progress) {
    emit progressReported(std::move(progress));
}

void FileOpTaskNotifier::reportFinished(FileOpSummary summary) {
    emit operationFinished(std::move(summary));
}

FileOperationTask::FileOperationTask(FileOpRequest request,
                                     QPointer<DirectoryModel> model,
                                     QPointer<MW> mw,
                                     FileOpTaskNotifier &notifier,
                                     std::shared_ptr<std::atomic<bool>> cancelled)
    : request(std::move(request)),
      model(model),
      mw(mw),
      notifier(notifier),
      cancelled(std::move(cancelled)) {
    setAutoDelete(true);
}

bool FileOperationTask::isCancelled() const {
    return cancelled && cancelled->load();
}

void FileOperationTask::run() {
    DialogResult overwriteFiles;
    for (const auto &path : std::as_const(request.paths)) {
        if (isCancelled())
            break;
        if (request.isMove)
            processMove(path, request.destDirectory, overwriteFiles);
        else
            processCopy(path, request.destDirectory, overwriteFiles);
        if (overwriteFiles.cancel || isCancelled())
            break;
    }

    FileOpSummary summary;
    summary.cancelled = overwriteFiles.cancel || isCancelled();
    summary.filesProcessed = filesProcessed;
    summary.isMove = request.isMove;
    notifier.reportFinished(std::move(summary));
}

// SINGLE FILE / DIR COPY, recursive.
// Direct port of the former Core::doInteractiveCopy(); only the calls that
// touch GUI-owned objects (fileReplaceDialog(), showError()) are wrapped to
// run on the GUI thread instead of this worker thread.
void FileOperationTask::processCopy(const QString &path, const QString &destDirectory,
                                    DialogResult &overwriteFiles) {
    if (isCancelled())
        return;

    QFileInfo srcFi(path);
    if (srcFi.isDir() && isDestinationInsideSource(path, destDirectory)) {
        if (mw) {
            QMetaObject::invokeMethod(mw, [mwPtr = mw]() {
                if (mwPtr)
                    mwPtr->showError(QCoreApplication::translate(
                        "FileOperationTask",
                        "Cannot copy a directory into itself or a subdirectory of itself."));
            }, Qt::QueuedConnection);
        }
        return;
    }
    // SINGLE FILE COPY
    // ===========================================================================
    if (!srcFi.isDir()) {
        FileOpResult result;
        FileOperations::copyFileTo(path, destDirectory, overwriteFiles, result);
        if (result == FileOpResult::DESTINATION_FILE_EXISTS) {
            if (overwriteFiles.all) // skipping all
                return;
            DialogResult dialogResult;
            QString srcPath = srcFi.absoluteFilePath();
            QString dstPath = destDirectory + "/" + srcFi.fileName();
            if (mw) {
                QMetaObject::invokeMethod(mw, [mwPtr = mw, srcPath, dstPath, &dialogResult]() {
                    if (mwPtr)
                        dialogResult = mwPtr->fileReplaceDialog(srcPath, dstPath, FILE_TO_FILE, true);
                }, Qt::BlockingQueuedConnection);
            }
            overwriteFiles = dialogResult;
            if (!overwriteFiles || overwriteFiles.cancel)
                return;
            FileOperations::copyFileTo(path, destDirectory, true, result);
        }
        if (result != FileOpResult::SUCCESS &&
            !(result == FileOpResult::DESTINATION_FILE_EXISTS && !overwriteFiles)) {
            QString errorText = FileOperations::decodeResult(result);
            if (mw) {
                QMetaObject::invokeMethod(mw, [mwPtr = mw, errorText]() {
                    if (mwPtr)
                        mwPtr->showError(errorText);
                }, Qt::QueuedConnection);
            }
            qDebug() << errorText;
        }
        if (result == FileOpResult::SUCCESS) {
            ++filesProcessed;
            notifier.reportProgress({path, filesProcessed, /*isMove=*/false});
        }
        if (!overwriteFiles.all) // copy attempt done; reset temporary flag
            overwriteFiles.yes = false;
        return;
    }
    // DIR COPY (RECURSIVE)
    // =======================================================================
    QDir srcDir(srcFi.absoluteFilePath());
    QFileInfo dstFi(destDirectory + "/" + srcFi.fileName());
    QDir dstDir(dstFi.absoluteFilePath());
    if (dstFi.exists() && !dstFi.isDir()) { // overwriting file with a folder
        if (!overwriteFiles && !overwriteFiles.all) {
            DialogResult dialogResult;
            QString srcPath = srcFi.absoluteFilePath();
            QString dstPath = dstFi.absoluteFilePath();
            if (mw) {
                QMetaObject::invokeMethod(mw, [mwPtr = mw, srcPath, dstPath, &dialogResult]() {
                    if (mwPtr)
                        dialogResult = mwPtr->fileReplaceDialog(srcPath, dstPath, DIR_TO_FILE, true);
                }, Qt::BlockingQueuedConnection);
            }
            overwriteFiles = dialogResult;
            if (!overwriteFiles || overwriteFiles.cancel)
                return;
            if (!overwriteFiles.all) // reset temp flag right away
                overwriteFiles.yes = false;
        }
        // remove dst file; give up if not writable
        FileOpResult result;
        FileOperations::removeFile(dstFi.absoluteFilePath(), result);
        if (result != FileOpResult::SUCCESS) {
            QString errorText = FileOperations::decodeResult(result);
            if (mw) {
                QMetaObject::invokeMethod(mw, [mwPtr = mw, errorText]() {
                    if (mwPtr)
                        mwPtr->showError(errorText);
                }, Qt::QueuedConnection);
            }
            qDebug() << errorText;
            return;
        }
    } else if (!dstDir.mkpath(".")) {
        QString dirPath = dstDir.absolutePath();
        if (mw) {
            QMetaObject::invokeMethod(mw, [mwPtr = mw, dirPath]() {
                if (mwPtr)
                    mwPtr->showError(QCoreApplication::translate(
                        "FileOperationTask", "Could not create directory ") + dirPath);
            }, Qt::QueuedConnection);
        }
        qDebug() << "Could not create directory " << dirPath;
        return;
    }
    // copy all contents
    QStringList entryList =
        srcDir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot |
                         QDir::Hidden | QDir::System);
    for (const auto &entry : std::as_const(entryList)) {
        if (isCancelled())
            return;
        processCopy(srcDir.absolutePath() + "/" + entry, dstDir.absolutePath(), overwriteFiles);
        if (overwriteFiles.cancel)
            return;
    }
}

// SINGLE FILE / DIR MOVE, recursive.
// Direct port of the former Core::doInteractiveMove(). In addition to
// fileReplaceDialog()/showError(), model->moveFileTo() and model->removeDir()
// both also mutate DirectoryManager bookkeeping (see DirectoryModel::
// moveFileTo()'s "chew through watcher events" comment) and so are wrapped
// the same way as the dialog calls, not left to run on this worker thread.
void FileOperationTask::processMove(const QString &path, const QString &destDirectory,
                                    DialogResult &overwriteFiles) {
    if (isCancelled())
        return;

    QFileInfo srcFi(path);
    if (srcFi.isDir() && isDestinationInsideSource(path, destDirectory)) {
        if (mw) {
            QMetaObject::invokeMethod(mw, [mwPtr = mw]() {
                if (mwPtr)
                    mwPtr->showError(QCoreApplication::translate(
                        "FileOperationTask",
                        "Cannot move a directory into itself or a subdirectory of itself."));
            }, Qt::QueuedConnection);
        }
        return;
    }
    // SINGLE FILE MOVE
    // ===========================================================================
    if (!srcFi.isDir()) {
        FileOpResult result = FileOpResult::OTHER_ERROR;
        if (model) {
            bool force = overwriteFiles;
            QMetaObject::invokeMethod(model, [modelPtr = model, path, destDirectory, force, &result]() {
                if (modelPtr)
                    modelPtr->moveFileTo(path, destDirectory, force, result);
            }, Qt::BlockingQueuedConnection);
        }
        if (result == FileOpResult::DESTINATION_FILE_EXISTS) {
            if (overwriteFiles.all) // skipping all
                return;
            DialogResult dialogResult;
            QString srcPath = srcFi.absoluteFilePath();
            QString dstPath = destDirectory + "/" + srcFi.fileName();
            if (mw) {
                QMetaObject::invokeMethod(mw, [mwPtr = mw, srcPath, dstPath, &dialogResult]() {
                    if (mwPtr)
                        dialogResult = mwPtr->fileReplaceDialog(srcPath, dstPath, FILE_TO_FILE, true);
                }, Qt::BlockingQueuedConnection);
            }
            overwriteFiles = dialogResult;
            if (!overwriteFiles || overwriteFiles.cancel)
                return;
            if (model) {
                QMetaObject::invokeMethod(model, [modelPtr = model, path, destDirectory, &result]() {
                    if (modelPtr)
                        modelPtr->moveFileTo(path, destDirectory, true, result);
                }, Qt::BlockingQueuedConnection);
            }
        }
        if (result != FileOpResult::SUCCESS &&
            !(result == FileOpResult::DESTINATION_FILE_EXISTS && !overwriteFiles)) {
            QString errorText = FileOperations::decodeResult(result);
            if (mw) {
                QMetaObject::invokeMethod(mw, [mwPtr = mw, errorText]() {
                    if (mwPtr)
                        mwPtr->showError(errorText);
                }, Qt::QueuedConnection);
            }
            qDebug() << errorText;
        }
        if (result == FileOpResult::SUCCESS) {
            ++filesProcessed;
            notifier.reportProgress({path, filesProcessed, /*isMove=*/true});
        }
        if (!overwriteFiles.all) // move attempt done; reset temporary flag
            overwriteFiles.yes = false;
        return;
    }
    // DIR MOVE (RECURSIVE)
    // =======================================================================
    QDir srcDir(srcFi.absoluteFilePath());
    QFileInfo dstFi(destDirectory + "/" + srcFi.fileName());
    QDir dstDir(dstFi.absoluteFilePath());
    if (dstFi.exists() && !dstFi.isDir()) { // overwriting file with a folder
        if (!overwriteFiles && !overwriteFiles.all) {
            DialogResult dialogResult;
            QString srcPath = srcFi.absoluteFilePath();
            QString dstPath = dstFi.absoluteFilePath();
            if (mw) {
                QMetaObject::invokeMethod(mw, [mwPtr = mw, srcPath, dstPath, &dialogResult]() {
                    if (mwPtr)
                        dialogResult = mwPtr->fileReplaceDialog(srcPath, dstPath, DIR_TO_FILE, true);
                }, Qt::BlockingQueuedConnection);
            }
            overwriteFiles = dialogResult;
            if (!overwriteFiles || overwriteFiles.cancel)
                return;
            if (!overwriteFiles.all) // reset temp flag right away
                overwriteFiles.yes = false;
        }
        // remove dst file; give up if not writable
        FileOpResult result;
        FileOperations::removeFile(dstFi.absoluteFilePath(), result);
        if (result != FileOpResult::SUCCESS) {
            QString errorText = FileOperations::decodeResult(result);
            if (mw) {
                QMetaObject::invokeMethod(mw, [mwPtr = mw, errorText]() {
                    if (mwPtr)
                        mwPtr->showError(errorText);
                }, Qt::QueuedConnection);
            }
            qDebug() << errorText;
            return;
        }
    } else if (!dstDir.mkpath(".")) {
        QString dirPath = dstDir.absolutePath();
        if (mw) {
            QMetaObject::invokeMethod(mw, [mwPtr = mw, dirPath]() {
                if (mwPtr)
                    mwPtr->showError(QCoreApplication::translate(
                        "FileOperationTask", "Could not create directory ") + dirPath);
            }, Qt::QueuedConnection);
        }
        qDebug() << "Could not create directory " << dirPath;
        return;
    }
    // move all contents
    QStringList entryList =
        srcDir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot |
                         QDir::Hidden | QDir::System);
    for (const auto &entry : std::as_const(entryList)) {
        if (isCancelled())
            return;
        processMove(srcDir.absolutePath() + "/" + entry, dstDir.absolutePath(), overwriteFiles);
        if (overwriteFiles.cancel)
            return;
    }
    // Clean up the now-empty source directory. FileOperations::removeDir()
    // only ever returns SUCCESS, SOURCE_DOES_NOT_EXIST, DIRECTORY_NOT_EMPTY,
    // or OTHER_ERROR - never a "nothing to do" style soft-success - so any
    // non-SUCCESS result here is a genuine failure worth surfacing, same as
    // every other FileOpResult check in this file. The original
    // Core::doInteractiveMove() discarded this result silently; that was a
    // pre-existing gap, not something Phase 1 needed to preserve.
    FileOpResult dirRmRes = FileOpResult::OTHER_ERROR;
    if (model) {
        QString dirPath = srcDir.absolutePath();
        QMetaObject::invokeMethod(model, [modelPtr = model, dirPath, &dirRmRes]() {
            if (modelPtr)
                modelPtr->removeDir(dirPath, false, false, dirRmRes);
        }, Qt::BlockingQueuedConnection);
    }
    if (dirRmRes != FileOpResult::SUCCESS) {
        QString errorText = FileOperations::decodeResult(dirRmRes);
        if (mw) {
            QMetaObject::invokeMethod(mw, [mwPtr = mw, errorText]() {
                if (mwPtr)
                    mwPtr->showError(errorText);
            }, Qt::QueuedConnection);
        }
        qDebug() << errorText;
    }
}
