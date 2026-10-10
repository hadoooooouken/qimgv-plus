#include "filereplacedialogmodel.h"

namespace {
FileReplaceDialogModel::Collision collisionFor(FileReplaceMode mode) {
    using Collision = FileReplaceDialogModel::Collision;
    switch (mode) {
    case DIR_TO_DIR:
        return Collision::DirectoryOverDirectory;
    case FILE_TO_DIR:
        return Collision::FileOverDirectory;
    case DIR_TO_FILE:
        return Collision::DirectoryOverFile;
    case FILE_TO_FILE:
        break;
    }
    return Collision::FileOverFile;
}

constexpr FileReplaceDecision kCancelled{.yes = false, .all = false, .cancel = true};
} // namespace

FileReplaceDialogModel::FileReplaceDialogModel(QObject *parent) : DialogSession(parent) {}

FileReplaceDialogModel::Collision FileReplaceDialogModel::collision() const {
    return collisionFor(mRequest.mode);
}

QString FileReplaceDialogModel::source() const {
    return mRequest.sourcePath;
}

QString FileReplaceDialogModel::destination() const {
    return mRequest.targetPath;
}

bool FileReplaceDialogModel::isMultiple() const {
    return mRequest.multiple;
}

FileReplaceDecision FileReplaceDialogModel::result() const {
    return mResult;
}

bool FileReplaceDialogModel::start(const FileReplaceRequest &request) {
    if (!canStart("file replace"))
        return false;
    mResult = kCancelled;
    mRequest = request;
    emit requestChanged();
    open();
    return true;
}

void FileReplaceDialogModel::answer(bool replace, bool applyToAll) {
    finish({.yes = replace, .all = mRequest.multiple && applyToAll, .cancel = false});
}

void FileReplaceDialogModel::cancel() {
    finish(kCancelled);
}

void FileReplaceDialogModel::dismiss() {
    finish({.yes = false, .all = false, .cancel = false});
}

void FileReplaceDialogModel::finish(FileReplaceDecision decision) {
    if (!isOpen())
        return;
    mResult = decision;
    conclude();
}
